/* SPDX-License-Identifier: Apache-2.0 */
/*
 * QNX 8.0 platform layer. NOT YET COMPILED: the SDP is not installed.
 * Every QNX call below cites the QNX 8.0 doc page it was written from.
 * Items marked UNVERIFIED are tracked in docs/TODO-QNX.md.
 *
 * Doc base: https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.lib_ref/topic/
 */
#include "rf_platform.h"

#include <devctl.h>          /* devctl(): .../d/devctl.html */
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <sys/dispatch.h>    /* name_open()/name_close(): .../n/name_open.html, .../n/name_close.html */
#include <sys/iofunc.h>      /* struct _io_devctl, _io_devctl_reply: .../i/iofunc_devctl.html */
#include <sys/neutrino.h>    /* MsgSend, ClockCycles, TimerTimeout */
#include <sys/syspage.h>     /* SYSPAGE_ENTRY(qtime): .../s/syspage_entry.html */
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "rf_testcase.h"

static uint64_t g_timeout_ns;

/* kernel_timeouts = 1: TimerTimeout() before raw MsgSend() (documented pattern). */
static const rf_platform_info g_info = { "qnx", 1, 1 };
const rf_platform_info *rf_plat_info(void) { return &g_info; }

int rf_plat_init(void) { return 0; }

/* ClockCycles(): "free-running 64-bit cycle counter"; on x86 it reads the TSC.
 * .../c/clockcycles.html  Frequency: SYSPAGE_ENTRY(qtime)->cycles_per_sec (same page).
 * Caveat from the docs: on x86_64 the frequency determined by startup "is not
 * exact and may vary from boot to boot" - so deadline thresholds must have
 * margin, and the host records cycles_per_sec with every campaign. */
uint64_t rf_plat_cycles(void) { return ClockCycles(); }
uint64_t rf_plat_cycles_per_sec(void) { return SYSPAGE_ENTRY(qtime)->cycles_per_sec; }

uint64_t rf_plat_now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

void rf_plat_set_call_timeout_ns(uint64_t ns) { g_timeout_ns = ns; }

/*
 * TimerTimeout(CLOCK_MONOTONIC, _NTO_TIMEOUT_SEND | _NTO_TIMEOUT_REPLY, NULL, &ns, NULL)
 * immediately before MsgSend(): exactly the example in .../t/timertimeout.html
 * ("If you pass NULL for notify, SIGEV_UNBLOCK is assumed"; the timed-out call
 * fails with ETIMEDOUT).
 * Known limits (from the same page):
 *  - it does not unblock a client whose message was already received by a
 *    server that set _NTO_CHF_UNBLOCK (resmgr servers typically do) - that is
 *    why the agent ALSO runs a watchdog thread (src/agent_main.c);
 *  - the Errors section lists EINVAL for "a nanosecond value less than 0 or
 *    greater than or equal to 1000 million", which conflicts with its own 10 s
 *    example. UNVERIFIED which is right; we clamp to 999,999,999 ns to be safe.
 */
static void arm_timeout(void)
{
    if (!g_timeout_ns) return;
    uint64_t ns = g_timeout_ns < 999999999ull ? g_timeout_ns : 999999999ull;
    (void)TimerTimeout(CLOCK_MONOTONIC, _NTO_TIMEOUT_SEND | _NTO_TIMEOUT_REPLY, NULL, &ns, NULL);
}

int rf_plat_open(rf_conn *c, const char *target, int access_mode)
{
    if (strncmp(target, "name:", 5) == 0) {
        /* name_open(): returns a side-channel coid or -1 with errno. .../n/name_open.html */
        int coid = name_open(target + 5, 0);
        if (coid < 0) return -errno;
        c->fd = coid; c->is_name = 1; c->open = 1;
        return 0;
    }
    static const int modes[3] = { O_RDONLY, O_WRONLY, O_RDWR };
    int fd = open(target, modes[(unsigned)access_mode % 3u]);   /* -> _IO_CONNECT */
    if (fd < 0) return -errno;
    c->fd = fd; c->is_name = 0; c->open = 1;
    return 0;
}

int rf_plat_close(rf_conn *c)
{
    if (!c->open) return -EBADF;
    int r = c->is_name ? name_close(c->fd) : close(c->fd);    /* .../n/name_close.html */
    c->open = 0;
    return r < 0 ? -errno : 0;
}

long rf_plat_read(rf_conn *c, void *buf, size_t n)
{
    if (!c->open) return -EBADF;
    ssize_t r = read(c->fd, buf, n);                            /* -> _IO_READ */
    return r < 0 ? -errno : (long)r;
}

long rf_plat_write(rf_conn *c, const void *buf, size_t n)
{
    if (!c->open) return -EBADF;
    ssize_t r = write(c->fd, buf, n);                           /* -> _IO_WRITE */
    return r < 0 ? -errno : (long)r;
}

long rf_plat_lseek(rf_conn *c, int64_t off, int whence)
{
    if (!c->open) return -EBADF;
    off_t r = lseek(c->fd, (off_t)off, whence);                 /* -> _IO_LSEEK */
    return r < 0 ? -errno : (long)r;
}

/* devctl() returns EOK or an error number directly (see the examples on
 * .../d/devctl.html: "if (error = devctl(...)) ... strerror(error)"). */
int rf_plat_devctl(rf_conn *c, int dcmd, void *data, size_t n, int *info)
{
    if (!c->open) return -EBADF;
    int err = devctl(c->fd, dcmd, data, n, info);               /* -> _IO_DEVCTL */
    return err == EOK ? 0 : -err;
}

/*
 * Hand-built _IO_DEVCTL. Header layout from .../i/iofunc_devctl.html:
 *   struct _io_devctl { uint16_t type; uint16_t combine_len; int32_t dcmd;
 *                       uint32_t nbytes; int32_t zero; }  + data[nbytes]
 * MsgSend() on an fd: "coid ... established by calling ConnectAttach() or one
 * of its cover functions, such as name_open() or open()" (.../m/msgsend.html).
 * UNVERIFIED: the exact combine_len value libc's devctl() puts in the header;
 * we use sizeof(header), which is what a non-combined message should carry.
 */
long rf_plat_devctl_raw(rf_conn *c, int dcmd, uint32_t claimed, const void *data, size_t len, void *reply,
                        size_t rlen)
{
    if (!c->open) return -EBADF;
    if (len > RF_MAX_PAYLOAD) return -E2BIG;
    struct {
        struct _io_devctl hdr;
        uint8_t           data[RF_MAX_PAYLOAD];
    } m;
    memset(&m.hdr, 0, sizeof m.hdr);
    m.hdr.type = _IO_DEVCTL;
    m.hdr.combine_len = sizeof m.hdr;
    m.hdr.dcmd = dcmd;
    m.hdr.nbytes = claimed;          /* may lie on purpose */
    m.hdr.zero = 0;
    if (len) memcpy(m.data, data, len);
    struct {
        struct _io_devctl_reply hdr;
        uint8_t                 data[RF_MAX_PAYLOAD];
    } r;
    size_t rbytes = sizeof r.hdr + (rlen < sizeof r.data ? rlen : sizeof r.data);
    arm_timeout();
    long st = MsgSend(c->fd, &m, sizeof m.hdr + len, &r, rbytes);
    if (st == -1) return -errno;     /* incl. ESRCH (server died), ETIMEDOUT */
    if (reply && rlen) memcpy(reply, r.data, rlen < sizeof r.data ? rlen : sizeof r.data);
    return st;
}

long rf_plat_msgsend_raw(rf_conn *c, const void *smsg, size_t slen, void *reply, size_t rlen)
{
    if (!c->open) return -EBADF;
    arm_timeout();
    long st = MsgSend(c->fd, smsg, slen, reply, rlen);          /* .../m/msgsend.html */
    return st == -1 ? -errno : st;
}
