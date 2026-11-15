/* SPDX-License-Identifier: Apache-2.0 */
/*
 * resfuzz-agent: the on-target fuzzing loop.
 *
 *   resfuzz-agent --target /dev/buggy [--spawn "/data/resfuzz/buggy_rm"] [--cov]
 *                 [--dict F]... [--corpus DIR] [--out DIR] [--seconds N]
 *                 [--iterations N] [--deadline-us N] [--hang-ms N] [--seed N]
 *                 [--no-raw] [--max-ops N] [--max-payload N]
 *   resfuzz-agent --target T [--spawn CMD] --replay FILE      (exit 0 ok,10 crash,11 hang,12 deadline)
 *   resfuzz-agent --target T  --spawn CMD  --minimize FILE --min-out FILE
 *       FILE may be a single test case (RFZ1) or a sequence (RFS1, see rf_testcase.h):
 *       a sequence is replayed in order against ONE fresh server instance.
 *   resfuzz-agent --decode FILE
 *
 * Output directory layout (consumed by host/resfuzz/triage.py):
 *   out/events.jsonl   one JSON object per finding / lifecycle event
 *   out/stats.json     rolling statistics
 *   out/{crashes,hangs,deadlines,corpus}/NAME.rfz   saved test cases
 *   out/{crashes,hangs}/NAME.seq    the server's history up to a stateful finding
 *
 * Stateful findings: with --spawn the server is restarted every
 * --respawn-every test cases (default 1000) and after every crash/hang, and
 * the agent remembers the cases run on the current instance. A finding that
 * does not reproduce alone on a fresh server is saved together with that
 * history as a sequence, which --minimize reduces (suffix search, case-level
 * delta debugging, then flattening into one test case where possible).
 */
#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "rf_corpus.h"
#include "rf_cov.h"
#include "rf_dict.h"
#include "rf_exec.h"
#include "rf_minimize.h"
#include "rf_mutate.h"
#include "rf_platform.h"
#include "rf_util.h"

#define RF_VERSION "0.1.0"
#define MAX_DICTS 8
#define MAX_BUCKETS 1024
#define SAVE_PER_BUCKET 3

typedef struct {
    const char *target, *spawn, *corpus_dir, *out_dir, *replay, *minimize, *min_out, *decode;
    const char *dicts[MAX_DICTS];
    unsigned ndicts;
    unsigned long long iterations, seconds, seed;
    unsigned deadline_us, hang_ms, max_ops, max_payload, respawn_every;
    int cov, no_raw, quiet, nonblock;
} opts_t;

/* ---------------- global state ---------------- */
static opts_t   O;
static rf_cov   g_cov;
static int      g_have_cov;
static pid_t    g_pid = -1;
static char    *g_spawn_argv[32];
static char     g_spawn_buf[1024];
static char     g_shm_env[96];
static FILE    *g_events;
static uint64_t g_cps;
static volatile sig_atomic_t g_stop;

/* watchdog */
static atomic_uint_fast64_t wd_since;     /* 0 = idle, else start time in ns */
static atomic_int           wd_fired;
static uint64_t             wd_limit_ns;
static const rf_tc         *wd_current;   /* test case in flight (for unmanaged hang dumps) */

/* Test cases run on the current server instance (oldest first). */
#define HIST_MAX 1024u
static rf_tc    g_hist[HIST_MAX];
static unsigned g_hn;
static int      g_hist_overflow;   /* older cases were dropped: a sequence may be incomplete */

static void hist_clear(void)
{
    for (unsigned i = 0; i < g_hn; i++) rf_tc_free(&g_hist[i]);
    g_hn = 0;
    g_hist_overflow = 0;
}

static void hist_push(const rf_tc *tc)
{
    if (g_hn == HIST_MAX) {                 /* drop the oldest */
        rf_tc_free(&g_hist[0]);
        memmove(&g_hist[0], &g_hist[1], (HIST_MAX - 1) * sizeof g_hist[0]);
        g_hn--;
        g_hist_overflow = 1;
    }
    if (rf_tc_copy(&g_hist[g_hn], tc) == 0) g_hn++;
}

typedef struct { uint64_t key; unsigned count; } bucket_t;
static bucket_t g_buckets[MAX_BUCKETS];
static unsigned g_nbuckets;
static uint8_t  g_sig_virgin[65536 / 8];

static struct {
    unsigned long long execs, crashes, hangs, deadlines, setup_fail, respawns, new_cov, new_sig;
    uint64_t start_ns, max_latency_ns;
} S;

static void on_signal(int s) { (void)s; g_stop = 1; }

static void usage(void)
{
    fprintf(stderr,
            "resfuzz-agent " RF_VERSION " (%s)\n"
            "usage: resfuzz-agent --target PATH|name:NAME [options]\n"
            "  --spawn \"CMD ARGS\"   start (and restart) the server under test\n"
            "  --cov                coverage feedback via shm (needs --spawn and an instrumented server)\n"
            "  --dict FILE          dictionary (repeatable)\n"
            "  --corpus DIR --out DIR --seconds N --iterations N --seed N\n"
            "  --deadline-us N (default 5000, 0=off) --hang-ms N (default 1000)\n"
            "  --no-raw --nonblock (open O_NONBLOCK: ttys) --max-ops N --max-payload N --quiet\n"
            "  --respawn-every N    restart the server every N cases (default 1000, 0=never; bounds stateful history)\n"
            "  --replay FILE | --minimize FILE --min-out FILE | --decode FILE\n",
            rf_plat_info()->name);
}

static int parse_args(int argc, char **argv)
{
    memset(&O, 0, sizeof O);
    O.out_dir = "resfuzz-out";
    O.deadline_us = 5000;
    O.hang_ms = 1000;
    O.max_ops = 16;
    O.max_payload = 1024;
    O.respawn_every = 1000;
    O.seed = (unsigned long long)time(NULL) ^ ((unsigned long long)getpid() << 16);
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        const char *v = i + 1 < argc ? argv[i + 1] : NULL;
#define ARG(name) (strcmp(a, name) == 0 && v && (i++, 1))
        if (ARG("--target")) O.target = v;
        else if (ARG("--spawn")) O.spawn = v;
        else if (ARG("--corpus")) O.corpus_dir = v;
        else if (ARG("--out")) O.out_dir = v;
        else if (ARG("--replay")) O.replay = v;
        else if (ARG("--minimize")) O.minimize = v;
        else if (ARG("--min-out")) O.min_out = v;
        else if (ARG("--decode")) O.decode = v;
        else if (ARG("--dict")) { if (O.ndicts < MAX_DICTS) O.dicts[O.ndicts++] = v; }
        else if (ARG("--iterations")) O.iterations = strtoull(v, NULL, 0);
        else if (ARG("--seconds")) O.seconds = strtoull(v, NULL, 0);
        else if (ARG("--seed")) O.seed = strtoull(v, NULL, 0);
        else if (ARG("--deadline-us")) O.deadline_us = (unsigned)strtoul(v, NULL, 0);
        else if (ARG("--hang-ms")) O.hang_ms = (unsigned)strtoul(v, NULL, 0);
        else if (ARG("--max-ops")) O.max_ops = (unsigned)strtoul(v, NULL, 0);
        else if (ARG("--max-payload")) O.max_payload = (unsigned)strtoul(v, NULL, 0);
        else if (ARG("--respawn-every")) O.respawn_every = (unsigned)strtoul(v, NULL, 0);
        else if (!strcmp(a, "--cov")) O.cov = 1;
        else if (!strcmp(a, "--no-raw")) O.no_raw = 1;
        else if (!strcmp(a, "--nonblock")) O.nonblock = 1;
        else if (!strcmp(a, "--quiet")) O.quiet = 1;
        else if (!strcmp(a, "--help") || !strcmp(a, "-h")) { usage(); exit(0); }
        else { fprintf(stderr, "resfuzz-agent: bad argument '%s'\n", a); usage(); return -1; }
#undef ARG
    }
    if (O.max_ops == 0 || O.max_ops > RF_MAX_OPS) O.max_ops = RF_MAX_OPS;
    rf_plat_set_nonblock(O.nonblock);
    if (O.max_payload == 0 || O.max_payload > RF_MAX_PAYLOAD) O.max_payload = RF_MAX_PAYLOAD;
    if (O.hang_ms == 0) O.hang_ms = 1000;
    if (!O.decode && !O.target) { usage(); return -1; }
    if (O.cov && !O.spawn) {
        fprintf(stderr, "resfuzz-agent: --cov needs --spawn (the server must map the shm at start)\n");
        return -1;
    }
    if (O.minimize && (!O.spawn || !O.min_out)) {
        fprintf(stderr, "resfuzz-agent: --minimize needs --spawn and --min-out\n");
        return -1;
    }
    return 0;
}

/* ---------------- JSON event log ---------------- */
static void json_str(FILE *f, const char *s)
{
    fputc('"', f);
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\') { fputc('\\', f); fputc(c, f); }
        else if (c < 0x20) fprintf(f, "\\u%04x", c);
        else fputc(c, f);
    }
    fputc('"', f);
}

static void event_finding(const char *type, const rf_tc *tc, const rf_result *r, const char *file, uint64_t key,
                          int repro, const char *seq, int seq_repro, unsigned seq_len)
{
    if (!g_events) return;
    const rf_op *op = (r->op_index >= 0 && r->op_index < (int)tc->nops) ? &tc->ops[r->op_index] : NULL;
    fprintf(g_events, "{\"t_ms\":%" PRIu64 ",\"type\":", (rf_plat_now_ns() - S.start_ns) / 1000000u);
    json_str(g_events, type);
    fprintf(g_events, ",\"bucket\":\"%016" PRIx64 "\",\"op_index\":%d,\"op\":", key, r->op_index);
    json_str(g_events, op ? rf_op_kind_name(op->kind) : "");
    fprintf(g_events, ",\"dcmd\":\"0x%08x\",\"errno\":%d,\"errno_name\":", op ? op->a : 0u, r->err);
    json_str(g_events, r->err ? strerror(r->err) : "");
    fprintf(g_events, ",\"signal\":%d,\"max_latency_us\":%" PRIu64 ",\"latency_op\":%d,\"file\":",
            r->signal, r->max_latency_ns / 1000u, r->max_latency_op);
    json_str(g_events, file ? file : "");
    fprintf(g_events, ",\"reproducible\":%s,\"execs\":%llu",
            repro < 0 ? "null" : (repro ? "true" : "false"), S.execs);
    if (seq && seq[0]) {
        fputs(",\"sequence\":", g_events);
        json_str(g_events, seq);
        fprintf(g_events, ",\"sequence_cases\":%u,\"sequence_reproducible\":%s", seq_len,
                seq_repro < 0 ? "null" : (seq_repro ? "true" : "false"));
    }
    fputs("}\n", g_events);
    fflush(g_events);
}

static void event_simple(const char *type, const char *detail)
{
    if (!g_events) return;
    fprintf(g_events, "{\"t_ms\":%" PRIu64 ",\"type\":", (rf_plat_now_ns() - S.start_ns) / 1000000u);
    json_str(g_events, type);
    fputs(",\"detail\":", g_events);
    json_str(g_events, detail ? detail : "");
    fputs("}\n", g_events);
    fflush(g_events);
}

static void write_stats(void)
{
    char path[1024], buf[1024];
    double secs = (double)(rf_plat_now_ns() - S.start_ns) / 1e9;
    int n = snprintf(buf, sizeof buf,
        "{\"version\":\"" RF_VERSION "\",\"platform\":\"%s\",\"target\":\"%s\",\"elapsed_s\":%.1f,"
        "\"execs\":%llu,\"execs_per_s\":%.1f,\"crashes\":%llu,\"hangs\":%llu,\"deadline_misses\":%llu,"
        "\"setup_failures\":%llu,\"respawns\":%llu,\"coverage\":%s,\"edges\":%u,\"reply_signatures\":%llu,"
        "\"buckets\":%u,\"max_latency_us\":%" PRIu64 ",\"cycles_per_sec\":%" PRIu64 ",\"deadline_us\":%u,"
        "\"seed\":%llu}\n",
        rf_plat_info()->name, O.target ? O.target : "", secs, S.execs, secs > 0 ? (double)S.execs / secs : 0.0,
        S.crashes, S.hangs, S.deadlines, S.setup_fail, S.respawns, g_have_cov ? "true" : "false",
        g_have_cov ? g_cov.edges_seen : 0u, S.new_sig, g_nbuckets, S.max_latency_ns / 1000u, g_cps,
        O.deadline_us, O.seed);
    snprintf(path, sizeof path, "%s/stats.json", O.out_dir);
    if (n > 0) rf_write_file(path, buf, (size_t)n);
}

/* ---------------- watchdog ---------------- */
static void wd_begin(void *ctx) { (void)ctx; atomic_store(&wd_since, rf_plat_now_ns()); }
static int wd_end(void *ctx)
{
    (void)ctx;
    atomic_store(&wd_since, 0);
    return atomic_exchange(&wd_fired, 0);
}

static void *wd_thread(void *arg)
{
    (void)arg;
    for (;;) {
        usleep(20000);
        uint64_t since = atomic_load(&wd_since);
        if (!since || rf_plat_now_ns() - since < wd_limit_ns) continue;
        atomic_store(&wd_fired, 1);
        atomic_store(&wd_since, 0);
        if (g_pid > 0) {
            /* SIGABRT, not SIGKILL: dumper writes a core for SIGABRT, so the
             * hang gets a backtrace of where the server was stuck.
             * (dumper docs: .../utilities/topic/d/dumper.html) */
            kill(g_pid, SIGABRT);
        } else {
            /* Server not ours to kill: save the evidence and let the host
             * orchestrator take over (it knows the server pid). */
            char path[1024];
            snprintf(path, sizeof path, "%s/hangs/unmanaged_%" PRIu64 ".rfz", O.out_dir, rf_plat_now_ns());
            if (wd_current) rf_tc_save(wd_current, path);
            event_simple("hang_unmanaged", path);
            write_stats();
            _exit(3);
        }
    }
    return NULL;
}

/* ---------------- target lifecycle ---------------- */
static int split_cmd(const char *cmd)
{
    snprintf(g_spawn_buf, sizeof g_spawn_buf, "%s", cmd);
    int n = 0;
    char *save = NULL;
    for (char *t = strtok_r(g_spawn_buf, " ", &save); t && n < 31; t = strtok_r(NULL, " ", &save))
        g_spawn_argv[n++] = t;
    g_spawn_argv[n] = NULL;
    return n;
}

static int wait_ready(unsigned ms)
{
    for (unsigned waited = 0; waited < ms; waited += 10) {
        rf_conn c = {0};
        if (rf_plat_open(&c, O.target, 0) == 0) { rf_plat_close(&c); return 0; }
        if (g_pid > 0 && rf_plat_poll_child(g_pid, NULL) == 1) { g_pid = -1; return -1; }
        usleep(10000);
    }
    return -1;
}

static int start_target(void)
{
    if (!O.spawn) return 0;
    char *env[2] = { g_have_cov ? g_shm_env : NULL, NULL };
    g_pid = rf_plat_spawn(g_spawn_argv, env);
    if (g_pid < 0) { perror("resfuzz-agent: spawn"); return -1; }
    if (wait_ready(5000) != 0) {
        fprintf(stderr, "resfuzz-agent: target did not come up on %s\n", O.target);
        return -1;
    }
    return 0;
}

static int restart_target(int already_reaped, int want_core)
{
    if (!O.spawn) return 0;
    if (!already_reaped && g_pid > 0) {
        int pr = 0;
        if (want_core) {
            /* Ask for a core first (dumper handles SIGABRT), then make sure. */
            kill(g_pid, SIGABRT);
            for (int i = 0; i < 100 && (pr = rf_plat_poll_child(g_pid, NULL)) == 0; i++) usleep(10000);
        }
        if (pr == 0) rf_plat_kill(g_pid);
    }
    g_pid = -1;
    S.respawns++;
    return start_target();
}

static rf_exec_cfg make_cfg(void)
{
    rf_exec_cfg cfg = { O.target, (uint64_t)O.deadline_us * 1000u, (uint64_t)O.hang_ms * 1000000u,
                        g_pid, wd_begin, wd_end, NULL };
    return cfg;
}

/* ---------------- feedback ---------------- */
static int sig_feedback(const rf_result *r)
{
    int novel = 0;
    for (unsigned i = 0; i < r->nsigs; i++) {
        uint32_t bit = r->sigs[i] & 0xffffu;
        if (!(g_sig_virgin[bit >> 3] & (1u << (bit & 7)))) {
            g_sig_virgin[bit >> 3] |= (uint8_t)(1u << (bit & 7));
            novel = 1;
        }
    }
    return novel;
}

static void harvest_cmplog(rf_dict *d)
{
    if (!g_have_cov) return;
    rf_cov_shm *s = g_cov.shm;
    uint32_t n = s->cmp_count < RF_CMPLOG_N ? s->cmp_count : RF_CMPLOG_N;
    for (uint32_t i = 0; i < n; i++) {
        const rf_cmp_entry *e = &s->cmp[i];
        if (e->size < 2 || e->size > 8) continue;
        uint64_t vals[2] = { e->a, e->b };
        for (int k = 0; k < (e->is_const ? 1 : 2); k++) {   /* const: only the constant (a) */
            uint64_t v = vals[k];
            if (v < 256 || v == UINT64_MAX) continue;   /* tiny values: interesting[] covers them */
            uint8_t tok[8];
            for (unsigned b = 0; b < e->size; b++) tok[b] = (uint8_t)(v >> (8 * b));
            rf_dict_add_token(d, tok, e->size);
        }
    }
}

static uint64_t bucket_key(rf_result_kind kind, const rf_tc *tc, const rf_result *r)
{
    const rf_op *op = (r->op_index >= 0 && r->op_index < (int)tc->nops) ? &tc->ops[r->op_index] : NULL;
    uint32_t parts[4] = { (uint32_t)kind, op ? op->kind : 0xffu,
                          op && (op->kind == RF_OP_DEVCTL || op->kind == RF_OP_DEVCTL_RAW) ? op->a : 0,
                          (uint32_t)r->signal };
    return rf_fnv1a64(parts, sizeof parts, 0);
}

static rf_result_kind run_one(const rf_tc *tc, rf_result *r);
static rf_result_kind run_seq(const rf_tc *const *cases, unsigned n, rf_result *r);

/* Servers keep state across test cases, so a finding may depend on what ran
 * before. With --spawn, crashes/hangs have just restarted the server: replay
 * the case once on that fresh instance and record whether it reproduces. */
static void record_finding(rf_result_kind kind, const rf_tc *tc, const rf_result *r)
{
    static const char *dirs[] = { "", "crashes", "hangs", "deadlines", "" };
    uint64_t key = bucket_key(kind, tc, r);
    bucket_t *b = NULL;
    for (unsigned i = 0; i < g_nbuckets; i++) if (g_buckets[i].key == key) { b = &g_buckets[i]; break; }
    if (!b && g_nbuckets < MAX_BUCKETS) { b = &g_buckets[g_nbuckets++]; b->key = key; b->count = 0; }
    char path[1024] = "";
    if (b && b->count < SAVE_PER_BUCKET) {
        snprintf(path, sizeof path, "%s/%s/%s_%016" PRIx64 "_%u.rfz", O.out_dir, dirs[kind],
                 rf_result_name(kind), key, b->count);
        if (rf_tc_save(tc, path) != 0) path[0] = '\0';
    }
    int repro = -1, seq_repro = -1;
    char seq[1100] = "";
    unsigned seq_len = 0;
    if (path[0] && O.spawn && !O.replay && !O.minimize) {
        /* g_hist still holds the cases run on the instance that failed. */
        static const rf_tc *hv[HIST_MAX + 1];
        unsigned hn = g_hn;
        for (unsigned i = 0; i < hn; i++) hv[i] = &g_hist[i];
        hv[hn] = tc;
        rf_result r2;
        repro = run_one(tc, &r2) == kind;
        if (!repro && hn > 0 && kind != RF_RES_DEADLINE) {
            snprintf(seq, sizeof seq, "%s/%s/%s_%016" PRIx64 "_%u.seq", O.out_dir, dirs[kind],
                     rf_result_name(kind), key, b->count);
            if (rf_seq_save(hv, hn + 1, seq) == 0) {
                seq_len = hn + 1;
                seq_repro = run_seq(hv, hn + 1, &r2) == kind;
            } else {
                seq[0] = '\0';
            }
        }
    }
    if (b) b->count++;
    if (!b || b->count <= SAVE_PER_BUCKET)
        event_finding(rf_result_name(kind), tc, r, path, key, repro, seq, seq_repro, seq_len);
    if (!O.quiet && path[0]) {
        fprintf(stderr, "[!] %s at op #%d (%s) -> %s%s\n", rf_result_name(kind), r->op_index,
                r->err ? strerror(r->err) : "-", path,
                repro == 0 ? (seq_repro == 1 ? " [stateful: reproduces as a sequence]"
                                             : " [did not reproduce on a fresh server: stateful]") : "");
    }
}

/* Execute + book-keeping. Returns the result kind. */
static rf_result_kind run_one(const rf_tc *tc, rf_result *r)
{
    rf_exec_cfg cfg = make_cfg();
    if (g_have_cov) rf_cov_reset(&g_cov);
    wd_current = tc;
    rf_exec(&cfg, tc, r);
    wd_current = NULL;
    S.execs++;
    if (r->max_latency_ns > S.max_latency_ns) S.max_latency_ns = r->max_latency_ns;
    if (r->kind == RF_RES_CRASH || r->kind == RF_RES_HANG) {
        if (restart_target(r->target_reaped, 1) != 0) {
            event_simple("fatal", "target restart failed");
            g_stop = 1;
        }
    } else if (r->kind == RF_RES_SETUP_FAIL && O.spawn) {
        restart_target(r->target_reaped, 0);
    }
    return r->kind;
}

/* Replay cases in order on ONE fresh server instance. Stops at the first
 * non-OK result; *r and the return value describe that case (or the last). */
static rf_result_kind run_seq(const rf_tc *const *cases, unsigned n, rf_result *r)
{
    if (O.spawn && restart_target(0, 0) != 0) { memset(r, 0, sizeof *r); return RF_RES_SETUP_FAIL; }
    rf_result_kind k = RF_RES_OK;
    for (unsigned i = 0; i < n; i++) {
        k = run_one(cases[i], r);
        if (k != RF_RES_OK && k != RF_RES_DEADLINE) {
            if (i + 1 < n) return RF_RES_SETUP_FAIL;   /* failed early: not the same finding */
            return k;
        }
    }
    return k;
}

/* ---------------- modes ---------------- */
static int mode_decode(void)
{
    rf_tc tc;
    if (rf_tc_load(&tc, O.decode) != 0) { fprintf(stderr, "cannot parse %s\n", O.decode); return 2; }
    printf("%s: %u ops\n", O.decode, tc.nops);
    rf_tc_dump(&tc, stdout);
    rf_tc_free(&tc);
    return 0;
}

static int exit_code(rf_result_kind k)
{
    switch (k) {
    case RF_RES_CRASH: return 10;
    case RF_RES_HANG: return 11;
    case RF_RES_DEADLINE: return 12;
    case RF_RES_SETUP_FAIL: return 2;
    default: return 0;
    }
}

static int load_seq_ptrs(const char *path, rf_tc **cases, unsigned *n, const rf_tc ***ptrs)
{
    if (rf_seq_load(cases, n, path) != 0) return -1;
    *ptrs = malloc(*n * sizeof **ptrs);
    if (!*ptrs) { rf_seq_free(*cases, *n); return -1; }
    for (unsigned i = 0; i < *n; i++) (*ptrs)[i] = &(*cases)[i];
    return 0;
}

static int mode_replay_seq(void)
{
    rf_tc *cases; unsigned n; const rf_tc **v;
    if (load_seq_ptrs(O.replay, &cases, &n, &v) != 0) { fprintf(stderr, "cannot parse %s\n", O.replay); return 2; }
    rf_result r;
    rf_result_kind k = run_seq(v, n, &r);
    printf("{\"result\":\"%s\",\"sequence_cases\":%u,\"op_index\":%d,\"errno\":%d,\"signal\":%d,"
           "\"max_latency_us\":%" PRIu64 "}\n", rf_result_name(k), n, r.op_index, r.err, r.signal,
           r.max_latency_ns / 1000u);
    free(v);
    rf_seq_free(cases, n);
    return exit_code(k);
}

static int mode_replay(void)
{
    if (rf_file_is_seq(O.replay)) return mode_replay_seq();
    rf_tc tc;
    if (rf_tc_load(&tc, O.replay) != 0) { fprintf(stderr, "cannot parse %s\n", O.replay); return 2; }
    rf_result r;
    run_one(&tc, &r);
    printf("{\"result\":\"%s\",\"op_index\":%d,\"errno\":%d,\"signal\":%d,\"max_latency_us\":%" PRIu64
           ",\"latency_op\":%d}\n",
           rf_result_name(r.kind), r.op_index, r.err, r.signal, r.max_latency_ns / 1000u, r.max_latency_op);
    rf_tc_free(&tc);
    return exit_code(r.kind);
}

static rf_result_kind g_min_want;
static int min_pred(const rf_tc *cand, void *ctx)
{
    (void)ctx;
    rf_result r;
    /* Fresh server per candidate: leftover state from a previous candidate
     * would otherwise make the minimised case depend on history. */
    if (g_pid > 0 && restart_target(0, 0) != 0) return 0;
    return run_one(cand, &r) == g_min_want;
}

static rf_result_kind g_seq_want;
static unsigned       g_seq_attempts;
static int seq_pred(const rf_tc *const *v, unsigned n)
{
    rf_result r;
    g_seq_attempts++;
    return run_seq(v, n, &r) == g_seq_want;
}

/*
 * Minimise a sequence: (1) shortest history suffix that still reproduces
 * (binary search; state is usually set shortly before the failure),
 * (2) delta debugging over whole cases, (3) flatten into one test case and,
 * if that still reproduces on a fresh server, minimise it op by op. The
 * failing case is always kept last.
 */
static int mode_minimize_seq(void)
{
    rf_tc *cases; unsigned n; const rf_tc **v;
    if (load_seq_ptrs(O.minimize, &cases, &n, &v) != 0) { fprintf(stderr, "cannot parse %s\n", O.minimize); return 2; }
    rf_result r;
    g_seq_want = run_seq(v, n, &r);
    if (g_seq_want == RF_RES_OK || g_seq_want == RF_RES_SETUP_FAIL || g_seq_want == RF_RES_DEADLINE) {
        fprintf(stderr, "resfuzz-agent: sequence %s does not reproduce (%s)\n", O.minimize, rf_result_name(g_seq_want));
        free(v); rf_seq_free(cases, n);
        return 1;
    }
    unsigned n0 = n;
    /* (1) suffix: keep k history cases before the last one */
    unsigned lo = 0, hi = n - 1;
    while (lo < hi) {
        unsigned mid = (lo + hi) / 2;
        if (seq_pred(v + (n - 1 - mid), mid + 1)) hi = mid; else lo = mid + 1;
    }
    const rf_tc **w = v + (n - 1 - lo);
    unsigned m = lo + 1;
    if (m < n && !seq_pred(w, m)) { w = v; m = n; }     /* not monotonic: keep everything */
    /* (2) remove chunks of history cases (never the last case) */
    const rf_tc **cand = malloc(m * sizeof *cand);
    for (unsigned chunk = (m - 1) / 2 ? (m - 1) / 2 : 1; m > 1 && cand; chunk /= 2) {
        for (unsigned start = 0; start + 1 < m;) {
            unsigned cut = RF_MIN(chunk, m - 1 - start), c = 0;
            for (unsigned i = 0; i < m; i++) if (i < start || i >= start + cut) cand[c++] = w[i];
            if (seq_pred(cand, c)) { memcpy((void *)w, cand, c * sizeof *cand); m = c; }
            else start += cut;
        }
        if (chunk == 1) break;
    }
    free(cand);
    int rc = 2;
    const char *form = "sequence";
    unsigned ops_after = 0;
    /* (3) flatten */
    rf_tc flat;
    rf_tc *mc = calloc(m, sizeof *mc);
    for (unsigned i = 0; mc && i < m; i++) rf_tc_copy(&mc[i], w[i]);
    int flat_ok = mc && rf_seq_flatten(&flat, mc, m) == 0;
    if (flat_ok) {
        g_min_want = g_seq_want;
        rf_min_stats st = { 0, 0, 2000 };
        if (min_pred(&flat, NULL)) {
            rf_minimize(&flat, min_pred, NULL, &st);
            rc = rf_tc_save(&flat, O.min_out) == 0 ? 0 : 2;
            form = "testcase";
            ops_after = flat.nops;
            g_seq_attempts += st.attempts;
        }
        rf_tc_free(&flat);
    }
    if (rc != 0) {
        rc = rf_seq_save(w, m, O.min_out) == 0 ? 0 : 2;
        for (unsigned i = 0; i < m; i++) ops_after += w[i]->nops;
    }
    printf("{\"result\":\"%s\",\"input\":\"sequence\",\"cases_before\":%u,\"cases_after\":%u,"
           "\"output\":\"%s\",\"ops_after\":%u,\"attempts\":%u}\n",
           rf_result_name(g_seq_want), n0, m, form, ops_after, g_seq_attempts);
    if (!strcmp(form, "testcase")) {
        rf_tc out;
        if (rf_tc_load(&out, O.min_out) == 0) { rf_tc_dump(&out, stdout); rf_tc_free(&out); }
    } else {
        for (unsigned i = 0; i < m; i++) { printf("-- case %u\n", i); rf_tc_dump(w[i], stdout); }
    }
    if (mc) rf_seq_free(mc, m);
    free(v); rf_seq_free(cases, n);
    return rc;
}

static int mode_minimize(void)
{
    if (rf_file_is_seq(O.minimize)) return mode_minimize_seq();
    rf_tc tc;
    if (rf_tc_load(&tc, O.minimize) != 0) { fprintf(stderr, "cannot parse %s\n", O.minimize); return 2; }
    rf_result r;
    g_min_want = run_one(&tc, &r);
    if (g_min_want == RF_RES_OK || g_min_want == RF_RES_SETUP_FAIL) {
        fprintf(stderr, "resfuzz-agent: %s does not reproduce (%s)\n", O.minimize, rf_result_name(g_min_want));
        rf_tc_free(&tc);
        return 1;
    }
    unsigned before = tc.nops;
    size_t before_sz = rf_tc_serialized_size(&tc);
    rf_min_stats st = { 0, 0, 2000 };
    rf_minimize(&tc, min_pred, NULL, &st);
    int rc = rf_tc_save(&tc, O.min_out) == 0 ? 0 : 2;
    printf("{\"result\":\"%s\",\"ops_before\":%u,\"ops_after\":%u,\"bytes_before\":%zu,\"bytes_after\":%zu,"
           "\"attempts\":%u}\n", rf_result_name(g_min_want), before, tc.nops, before_sz,
           rf_tc_serialized_size(&tc), st.attempts);
    rf_tc_dump(&tc, stdout);
    rf_tc_free(&tc);
    return rc;
}

static int mode_fuzz(rf_dict *dict)
{
    rf_rng rng;
    rf_rng_seed(&rng, O.seed);
    rf_mutator mut;
    rf_mutator_init(&mut, &rng, dict);
    mut.max_ops = O.max_ops;
    mut.max_payload = O.max_payload;
    mut.enable_raw = !O.no_raw && rf_plat_info()->has_raw_msgsend;

    char cdir[1024];
    snprintf(cdir, sizeof cdir, "%s/corpus", O.out_dir);
    rf_corpus corpus;
    if (rf_corpus_init(&corpus, cdir, 8192) != 0) { fprintf(stderr, "cannot create %s\n", cdir); return 2; }
    if (O.corpus_dir) {
        int n = rf_corpus_load_dir(&corpus, O.corpus_dir);
        if (!O.quiet) fprintf(stderr, "[*] loaded %d seeds from %s\n", n < 0 ? 0 : n, O.corpus_dir);
    }
    if (corpus.n == 0) {
        static rf_tc seeds[RF_DICT_MAX_DCMDS + 1];
        unsigned ns = rf_gen_seeds(&mut, seeds, RF_ARRAY_LEN(seeds));
        for (unsigned i = 0; i < ns; i++) { rf_corpus_add(&corpus, &seeds[i], 0, 0); rf_tc_free(&seeds[i]); }
    }
    /* Dry run of the initial corpus establishes baseline coverage. */
    hist_clear();
    for (unsigned i = 0; i < corpus.n && !g_stop; i++) {
        rf_result r;
        unsigned long long resp0 = S.respawns;
        rf_result_kind k = run_one(&corpus.e[i].tc, &r);
        if (g_have_cov) { rf_cov_has_new(&g_cov); harvest_cmplog(dict); }
        sig_feedback(&r);
        if (k != RF_RES_OK && k != RF_RES_SETUP_FAIL) record_finding(k, &corpus.e[i].tc, &r);
        if (k == RF_RES_CRASH) S.crashes++;
        if (k == RF_RES_SETUP_FAIL) { S.setup_fail++; }
        if (S.respawns != resp0) hist_clear(); else hist_push(&corpus.e[i].tc);
    }
    if (S.setup_fail && S.setup_fail == corpus.n) {
        fprintf(stderr, "resfuzz-agent: cannot open %s\n", O.target);
        event_simple("fatal", "cannot open target");
        rf_corpus_free(&corpus);
        return 2;
    }

    uint64_t deadline_end = O.seconds ? rf_plat_now_ns() + O.seconds * 1000000000ull : 0;
    uint64_t last_stats = rf_plat_now_ns();
    rf_tc cur;
    while (!g_stop) {
        if (O.iterations && S.execs >= O.iterations) break;
        if (deadline_end && rf_plat_now_ns() >= deadline_end) break;
        if (O.spawn && O.respawn_every && g_hn >= O.respawn_every) {
            /* bound the state a finding can depend on (and the sequence length) */
            if (restart_target(0, 0) != 0) { event_simple("fatal", "target restart failed"); break; }
            hist_clear();
        }
        unsigned long long resp0 = S.respawns;

        rf_corpus_entry *e = rf_corpus_pick(&corpus, &rng);
        if (!e || rf_tc_copy(&cur, &e->tc) != 0) break;
        mut.splice_with = corpus.n > 1 ? &corpus.e[rf_rng_below(&rng, corpus.n)].tc : NULL;
        rf_mutate_havoc(&mut, &cur);

        rf_result r;
        rf_result_kind k = run_one(&cur, &r);
        int cov_new = 0, sig_new = 0;
        if (g_have_cov && k != RF_RES_SETUP_FAIL) {
            cov_new = rf_cov_has_new(&g_cov);
            if (cov_new) harvest_cmplog(dict);
        }
        if (k != RF_RES_SETUP_FAIL) sig_new = sig_feedback(&r);
        switch (k) {
        case RF_RES_CRASH:    S.crashes++;   record_finding(k, &cur, &r); break;
        case RF_RES_HANG:     S.hangs++;     record_finding(k, &cur, &r); break;
        case RF_RES_DEADLINE: S.deadlines++; record_finding(k, &cur, &r); break;
        case RF_RES_SETUP_FAIL: S.setup_fail++; break;
        default: break;
        }
        /* Deadline-missing inputs are recorded but not kept for mutation:
         * they are slow by definition and would drag the exec rate down. */
        if (k == RF_RES_OK && (cov_new || sig_new)) {
            if (rf_corpus_add(&corpus, &cur, cov_new == 2 ? 1u : 0u, r.total_ns) >= 0) {
                if (cov_new) S.new_cov++;
                if (sig_new) S.new_sig++;
            }
        }
        if (S.respawns != resp0) hist_clear(); else hist_push(&cur);
        rf_tc_free(&cur);

        uint64_t now = rf_plat_now_ns();
        if (now - last_stats > 1000000000ull) {
            last_stats = now;
            write_stats();
            if (!O.quiet)
                fprintf(stderr, "[*] execs %llu (%.0f/s) corpus %u edges %u sigs %llu crash %llu hang %llu "
                        "deadline %llu maxlat %" PRIu64 "us\n", S.execs,
                        (double)S.execs * 1e9 / (double)(now - S.start_ns), corpus.n,
                        g_have_cov ? g_cov.edges_seen : 0u, S.new_sig, S.crashes, S.hangs, S.deadlines,
                        S.max_latency_ns / 1000u);
        }
    }
    write_stats();
    event_simple("done", "");
    hist_clear();
    rf_corpus_free(&corpus);
    return 0;
}

int main(int argc, char **argv)
{
    if (parse_args(argc, argv) != 0) return 2;
    if (rf_plat_init() != 0) { fprintf(stderr, "platform init failed\n"); return 2; }
    if (O.decode) return mode_decode();

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);
    signal(SIGPIPE, SIG_IGN);
    S.start_ns = rf_plat_now_ns();
    g_cps = rf_plat_cycles_per_sec();
    wd_limit_ns = (uint64_t)O.hang_ms * 1000000u * 2u + 100000000u;

    static rf_dict dict;
    rf_dict_init(&dict);
    for (unsigned i = 0; i < O.ndicts; i++) {
        int n = rf_dict_load(&dict, O.dicts[i]);
        if (n < 0) { fprintf(stderr, "resfuzz-agent: bad dictionary %s\n", O.dicts[i]); return 2; }
        if (!O.quiet) fprintf(stderr, "[*] dictionary %s: %d entries\n", O.dicts[i], n);
    }

    char sub[1100];
    const char *subs[] = { "crashes", "hangs", "deadlines", "corpus" };
    for (unsigned i = 0; i < RF_ARRAY_LEN(subs); i++) {
        snprintf(sub, sizeof sub, "%s/%s", O.out_dir, subs[i]);
        if (rf_mkdirs(sub) != 0) { fprintf(stderr, "cannot create %s\n", sub); return 2; }
    }
    snprintf(sub, sizeof sub, "%s/events.jsonl", O.out_dir);
    g_events = fopen(sub, "a");

    if (O.spawn && split_cmd(O.spawn) == 0) { fprintf(stderr, "empty --spawn\n"); return 2; }
    if (O.cov) {
        char name[64];
        snprintf(name, sizeof name, "/resfuzz_cov_%d", (int)getpid());
        if (rf_cov_create(&g_cov, name) == 0) {
            g_have_cov = 1;
            snprintf(g_shm_env, sizeof g_shm_env, "RESFUZZ_SHM=%s", name);
        } else {
            fprintf(stderr, "resfuzz-agent: shm_open failed (%s); continuing blind\n", strerror(errno));
        }
    }
    char detail[512];
    snprintf(detail, sizeof detail, "platform=%s target=%s coverage=%d seed=%llu cps=%" PRIu64,
             rf_plat_info()->name, O.target, g_have_cov, O.seed, g_cps);
    event_simple("start", detail);

    if (start_target() != 0) { event_simple("fatal", "target failed to start"); return 2; }
    pthread_t wd;
    pthread_create(&wd, NULL, wd_thread, NULL);
    pthread_detach(wd);

    int rc;
    if (O.replay) rc = mode_replay();
    else if (O.minimize) rc = mode_minimize();
    else rc = mode_fuzz(&dict);

    if (g_pid > 0) rf_plat_kill(g_pid);
    if (g_have_cov) rf_cov_destroy(&g_cov);
    if (g_events) fclose(g_events);
    return rc;
}
