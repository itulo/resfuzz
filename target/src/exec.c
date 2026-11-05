/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Executes one test case through the platform layer and applies the three
 * oracles: crash (server gone), hang (timeout / watchdog), and the real-time
 * deadline oracle (per-call reply latency measured with the platform cycle
 * counter: ClockCycles() on QNX).
 */
#include "rf_exec.h"
#include "rf_util.h"

#include <errno.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static const char *const res_names[] = { "ok", "crash", "hang", "deadline", "setup_fail" };

const char *rf_result_name(rf_result_kind k)
{
    return (unsigned)k < RF_ARRAY_LEN(res_names) ? res_names[k] : "?";
}

int rf_err_means_server_gone(int err)
{
    /* QNX MsgSend(): ESRCH = "The server died while the calling thread was
     * SEND-blocked or REPLY-blocked"; EBADF = "The connection indicated by coid
     * is no longer connected to a channel" (lib_ref/topic/m/msgsend.html). */
    if (err == ESRCH || err == EBADF) return 1;
#ifdef ESRVRFAULT
    /* "A fault occurred in a server's address space when the kernel tried to
     * access the server's message buffers" - a server-side bug as well. */
    if (err == ESRVRFAULT) return 1;
#endif
    return 0;
}

uint64_t rf_cycles_to_ns(uint64_t cycles, uint64_t cps)
{
    if (cps == 0) return 0;
    return (cycles / cps) * 1000000000ull + ((cycles % cps) * 1000000000ull) / cps;
}

static uint32_t sig_of(const rf_op *op, long r)
{
    uint32_t cls = r < 0 ? (uint32_t)(-r) & 0xfffu : (r == 0 ? 0x1000u : 0x2000u);
    uint32_t a = (op->kind == RF_OP_DEVCTL || op->kind == RF_OP_DEVCTL_RAW) ? op->a : 0;
    uint32_t h = (uint32_t)rf_fnv1a64(&a, sizeof a, ((uint64_t)op->kind << 32) | cls);
    return h;
}

static int needs_conn(uint8_t kind) { return kind != RF_OP_OPEN && kind != RF_OP_SLEEP; }

int rf_exec(const rf_exec_cfg *cfg, const rf_tc *tc, rf_result *res)
{
    static uint8_t rbuf[RF_MAX_READ];
    static uint8_t dbuf[RF_MAX_PAYLOAD];
    rf_conn conns[RF_MAX_SLOTS];
    memset(conns, 0, sizeof conns);
    int omode[RF_MAX_SLOTS];                 /* access_mode index per slot: 0 RD, 1 WR, 2 RDWR */
    for (unsigned s = 0; s < RF_MAX_SLOTS; s++) omode[s] = 2;
    memset(res, 0, sizeof *res);
    res->op_index = -1;
    res->max_latency_op = -1;
    int deadline_op = -1, ever_opened = 0;
    uint64_t cps = rf_plat_cycles_per_sec();
    uint64_t start = rf_plat_now_ns();

    rf_plat_set_call_timeout_ns(cfg->hang_ns);

    /* Implicit open of slot 0 unless the test case starts by opening it. */
    if (!(tc->nops && tc->ops[0].kind == RF_OP_OPEN && (tc->ops[0].flags & 3u) == 0)) {
        int r = rf_plat_open(&conns[0], cfg->target, 2);
        if (r == -EACCES || r == -EPERM) { r = rf_plat_open(&conns[0], cfg->target, 0); omode[0] = 0; }
        if (r < 0) {
            res->kind = RF_RES_SETUP_FAIL;
            res->err = -r;
            goto done;
        }
        ever_opened = 1;
    }

    for (unsigned i = 0; i < tc->nops; i++) {
        const rf_op *op = &tc->ops[i];
        rf_conn *c = &conns[op->flags & (RF_MAX_SLOTS - 1)];
        if (needs_conn(op->kind) && !c->open) { res->ops_err++; continue; }  /* nothing sent */

        long r = 0;
        if (cfg->wd_begin) cfg->wd_begin(cfg->wd_ctx);
        uint64_t t0 = rf_plat_cycles();
        switch (op->kind) {
        case RF_OP_OPEN:
            if (c->open) rf_plat_close(c);
            r = rf_plat_open(c, cfg->target, (int)(op->a % 3u));
            omode[op->flags & (RF_MAX_SLOTS - 1)] = (int)(op->a % 3u);
            if (r == 0) ever_opened = 1;
            else if (r == -ENOENT && ever_opened) r = -ESRCH;   /* name vanished: server died */
            break;
        case RF_OP_CLOSE:
            r = rf_plat_close(c);
            break;
        case RF_OP_READ:
            r = rf_plat_read(c, rbuf, RF_MIN(op->a, RF_MAX_READ));
            break;
        case RF_OP_WRITE:
            r = rf_plat_write(c, op->data, op->len);
            break;
        case RF_OP_DEVCTL: {
            size_t n = RF_MIN(RF_MAX((size_t)op->len, (size_t)op->b), sizeof dbuf);
            memset(dbuf, 0, n);
            if (op->len) memcpy(dbuf, op->data, op->len);
            int info = 0;
            r = rf_plat_devctl(c, (int)op->a, dbuf, n, &info);
            break;
        }
        case RF_OP_DEVCTL_RAW:
            r = rf_plat_devctl_raw(c, (int)op->a, op->b, op->data, op->len, dbuf, sizeof dbuf);
            break;
        case RF_OP_LSEEK:
            r = rf_plat_lseek(c, (int32_t)op->a, (int)(op->b % 3u));
            break;
        case RF_OP_MSG_RAW:
            r = op->len ? rf_plat_msgsend_raw(c, op->data, op->len, dbuf, sizeof dbuf) : -EINVAL;
            break;
        case RF_OP_SLEEP:
            usleep(RF_MIN(op->a, RF_MAX_SLEEP_US));
            break;
        default:
            r = -EINVAL;
        }
        uint64_t dt = rf_cycles_to_ns(rf_plat_cycles() - t0, cps);
        int wd_fired = cfg->wd_end ? cfg->wd_end(cfg->wd_ctx) : 0;

        if (res->nsigs < RF_MAX_OPS) res->sigs[res->nsigs++] = sig_of(op, r);
        if (op->kind != RF_OP_SLEEP && dt > res->max_latency_ns) {
            res->max_latency_ns = dt;
            res->max_latency_op = (int)i;
        }
        if (wd_fired || r == -ETIMEDOUT) {
            res->kind = RF_RES_HANG;
            res->op_index = (int)i;
            res->err = ETIMEDOUT;
            goto done;
        }
        /* POSIX/QNX: read() on an O_WRONLY fd and write() on an O_RDONLY fd fail
         * with EBADF from the server's iofunc layer (seen on QNX 8.0, TODO-QNX
         * F1) - an ordinary error, not a vanished server. */
        int wrong_dir = r == -EBADF &&
            ((op->kind == RF_OP_READ  && omode[op->flags & (RF_MAX_SLOTS - 1)] == 1) ||
             (op->kind == RF_OP_WRITE && omode[op->flags & (RF_MAX_SLOTS - 1)] == 0));
        if (r < 0 && rf_err_means_server_gone((int)-r) && !wrong_dir &&
            !(op->kind == RF_OP_CLOSE && r == -EBADF)) {
            res->kind = RF_RES_CRASH;
            res->op_index = (int)i;
            res->err = (int)-r;
            goto done;
        }
        if (r < 0) res->ops_err++; else res->ops_ok++;
        if (cfg->deadline_ns && op->kind != RF_OP_SLEEP && op->kind != RF_OP_OPEN &&
            dt > cfg->deadline_ns && deadline_op < 0)
            deadline_op = (int)i;
    }
    if (deadline_op >= 0) {
        res->kind = RF_RES_DEADLINE;
        res->op_index = deadline_op;
    }

done:
    for (unsigned s = 0; s < RF_MAX_SLOTS; s++)
        if (conns[s].open) rf_plat_close(&conns[s]);
    if (cfg->target_pid > 0) {
        int st = 0;
        /* A server killed by a signal is a crash even if every reply arrived. */
        int polled = rf_plat_poll_child(cfg->target_pid, &st);
        /* After a "server gone" error the child may still be exiting: give it
         * up to ~200 ms so its signal is attributed to this test case. */
        for (int w = 0; polled == 0 && res->kind == RF_RES_CRASH && w < 40; w++) {
            usleep(5000);
            polled = rf_plat_poll_child(cfg->target_pid, &st);
        }
        if (polled == 1) {
            res->target_reaped = 1;
            if (res->kind == RF_RES_HANG) goto out;
            if (res->kind != RF_RES_CRASH) { res->kind = RF_RES_CRASH; res->op_index = -1; }
            res->signal = WIFSIGNALED(st) ? WTERMSIG(st) : 0;
            if (!res->err) res->err = ESRCH;
        }
    }
out:
    res->total_ns = rf_plat_now_ns() - start;
    return 0;
}
