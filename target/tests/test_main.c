/* SPDX-License-Identifier: Apache-2.0 */
/* Portable unit tests + mock_rm integration tests (Linux stub platform). */
#include "tinytest.h"

#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include "buggy_rm.h"
#include "rf_corpus.h"
#include "rf_cov.h"
#include "rf_dict.h"
#include "rf_exec.h"
#include "rf_minimize.h"
#include "rf_mutate.h"
#include "rf_platform.h"
#include "rf_rng.h"
#include "rf_testcase.h"
#include "rf_util.h"

int tt_failures, tt_checks;
static char tmpdir[256];

/* ---------------------------------------------------------------- rng */
static void test_rng(void)
{
    rf_rng a, b;
    rf_rng_seed(&a, 42); rf_rng_seed(&b, 42);
    for (int i = 0; i < 100; i++) CHECK(rf_rng_next(&a) == rf_rng_next(&b));
    rf_rng_seed(&b, 43);
    CHECK(rf_rng_next(&a) != rf_rng_next(&b));
    unsigned hist[7] = {0};
    for (int i = 0; i < 70000; i++) {
        uint32_t v = rf_rng_below(&a, 7);
        CHECK(v < 7);
        if (v < 7) hist[v]++;
    }
    for (int i = 0; i < 7; i++) CHECK(hist[i] > 9000 && hist[i] < 11000);
    CHECK_EQ(rf_rng_below(&a, 1), 0);
}

/* ---------------------------------------------------------------- testcase */
static void make_sample(rf_tc *tc)
{
    rf_tc_init(tc);
    rf_tc_append(tc, RF_OP_OPEN, 1, 2, 0, NULL, 0);
    rf_tc_append(tc, RF_OP_WRITE, 0, 0, 0, "hello", 5);
    rf_tc_append(tc, RF_OP_DEVCTL, 1, 0xdeadbeef, 64, "\x01\x02\x03", 3);
    rf_tc_append(tc, RF_OP_READ, 0, 100, 0, NULL, 0);
}

static void test_testcase_roundtrip(void)
{
    rf_tc tc, back;
    make_sample(&tc);
    CHECK_EQ(tc.nops, 4);
    uint8_t buf[1024];
    size_t n = rf_tc_serialize(&tc, buf, sizeof buf);
    CHECK_EQ(n, rf_tc_serialized_size(&tc));
    CHECK_EQ(n, RF_TC_HDR_LEN + 4 * RF_OP_HDR_LEN + 5 + 3);
    CHECK(memcmp(buf, RF_TC_MAGIC, 4) == 0);
    CHECK_EQ(rf_tc_parse(&back, buf, n), 0);
    CHECK_EQ(back.nops, 4);
    CHECK_EQ(back.ops[2].a, 0xdeadbeef);
    CHECK_EQ(back.ops[2].b, 64);
    CHECK_EQ(back.ops[2].flags, 1);
    CHECK(back.ops[1].len == 5 && memcmp(back.ops[1].data, "hello", 5) == 0);
    CHECK(rf_tc_hash(&tc) == rf_tc_hash(&back));
    back.ops[1].data[0] ^= 1;
    CHECK(rf_tc_hash(&tc) != rf_tc_hash(&back));
    rf_tc_free(&back);

    /* Exact little-endian layout of op #2 (documented in docs/DESIGN.md). */
    const uint8_t *op2 = buf + RF_TC_HDR_LEN + 2 * RF_OP_HDR_LEN + 5;
    CHECK_EQ(op2[0], RF_OP_DEVCTL);
    CHECK_EQ(op2[1], 1);
    CHECK_EQ(rf_get_u16(op2 + 2), 3);
    CHECK_EQ(rf_get_u32(op2 + 4), 0xdeadbeef);

    /* Rejections. */
    CHECK(rf_tc_parse(&back, buf, n - 1) != 0);                 /* truncated */
    uint8_t big[1100];
    memcpy(big, buf, n); big[n] = 0;
    CHECK(rf_tc_parse(&back, big, n + 1) != 0);                 /* trailing byte */
    buf[0] = 'X';
    CHECK(rf_tc_parse(&back, buf, n) != 0);                     /* magic */
    buf[0] = 'R';
    buf[RF_TC_HDR_LEN] = RF_OP__COUNT;
    CHECK(rf_tc_parse(&back, buf, n) != 0);                     /* bad kind */

    /* File save/load. */
    char path[400];
    snprintf(path, sizeof path, "%s/t.rfz", tmpdir);
    CHECK_EQ(rf_tc_save(&tc, path), 0);
    CHECK_EQ(rf_tc_load(&back, path), 0);
    CHECK(rf_tc_hash(&tc) == rf_tc_hash(&back));
    rf_tc_free(&back);

    /* Structural edits. */
    rf_tc_remove(&tc, 0);
    CHECK_EQ(tc.nops, 3);
    CHECK_EQ(tc.ops[0].kind, RF_OP_WRITE);
    rf_tc_free(&tc);
    CHECK_EQ(tc.nops, 0);
}

static void test_testcase_parser_fuzz(void)
{
    /* The parser takes files from disk/host: it must never crash on garbage. */
    rf_rng r;
    rf_rng_seed(&r, 7);
    rf_tc tc;
    make_sample(&tc);
    uint8_t good[1024];
    size_t n = rf_tc_serialize(&tc, good, sizeof good);
    rf_tc_free(&tc);
    int accepted = 0;
    for (int it = 0; it < 20000; it++) {
        uint8_t buf[1024];
        size_t len = n;
        memcpy(buf, good, n);
        int flips = 1 + (int)rf_rng_below(&r, 4);
        for (int k = 0; k < flips; k++) buf[rf_rng_below(&r, (uint32_t)n)] = (uint8_t)rf_rng_next(&r);
        if (rf_rng_one_in(&r, 4)) len = rf_rng_below(&r, (uint32_t)n + 1);
        rf_tc out;
        if (rf_tc_parse(&out, buf, len) == 0) {
            accepted++;
            uint8_t again[2048];
            size_t m = rf_tc_serialize(&out, again, sizeof again);
            CHECK(m == len && memcmp(again, buf, len) == 0);    /* canonical */
            rf_tc_free(&out);
        }
    }
    CHECK(accepted > 0);
}

/* ---------------------------------------------------------------- dict */
static void test_dict(void)
{
    static rf_dict d;
    rf_dict_init(&d);
    const char *text =
        "# comment\n"
        "\n"
        "magic=\"ADMN\"\n"
        "  bin=\"\\x00\\xff\\\\\\\"\"  \n"
        "\"bare\"\n"
        "dcmd DCMD_FOO 0x40080d40 size=8 dir=to\n"
        "dcmd DCMD_BAR 3405 dir=none\n";
    int line = 0;
    CHECK_EQ(rf_dict_parse(&d, text, strlen(text), &line), 5);
    CHECK_EQ(d.ntokens, 3);
    CHECK_EQ(d.ndcmds, 2);
    CHECK(d.tokens[0].len == 4 && memcmp(d.tokens[0].bytes, "ADMN", 4) == 0);
    CHECK(d.tokens[1].len == 4 && memcmp(d.tokens[1].bytes, "\x00\xff\\\"", 4) == 0);
    CHECK(strcmp(d.dcmds[0].name, "DCMD_FOO") == 0);
    CHECK_EQ(d.dcmds[0].value, 0x40080d40);
    CHECK_EQ(d.dcmds[0].size, 8);
    CHECK_EQ(d.dcmds[0].dir, RF_DIR_TO);
    CHECK_EQ(d.dcmds[1].value, 3405);
    /* Duplicates are ignored. */
    CHECK_EQ(rf_dict_add_token(&d, "ADMN", 4), 0);
    /* Errors report the line. */
    const char *bad[] = { "x=\"unterminated\n", "dcmd X 0x1 dir=sideways\n", "x=\"\\q\"\n", "noquotes\n",
                          "ok=\"a\"\ndcmd\n" };
    for (unsigned i = 0; i < RF_ARRAY_LEN(bad); i++) {
        static rf_dict e;
        rf_dict_init(&e);
        line = 0;
        CHECK(rf_dict_parse(&e, bad[i], strlen(bad[i]), &line) < 0);
        CHECK(line >= 1);
    }
    /* The shipped example dictionary parses. */
    rf_dict_init(&d);
    CHECK(rf_dict_load(&d, "examples/buggy_rm.dict") >= 10);
    CHECK_EQ(d.ndcmds, 6);
    int found = 0;
    for (unsigned i = 0; i < d.ndcmds; i++)
        if (d.dcmds[i].value == (uint32_t)DCMD_BUGGY_GET_STATS) found = 1;
    CHECK(found);   /* example dict matches the stub header encoding */
}

/* ---------------------------------------------------------------- mutate */
static void test_mutate_invariants(void)
{
    static rf_dict d;
    rf_dict_init(&d);
    CHECK(rf_dict_load(&d, "examples/buggy_rm.dict") > 0);
    rf_rng r;
    rf_rng_seed(&r, 99);
    rf_mutator m;
    rf_mutator_init(&m, &r, &d);
    m.max_ops = 12;
    m.max_payload = 300;
    m.enable_raw = 1;

    rf_tc seeds[16];
    unsigned ns = rf_gen_seeds(&m, seeds, 16);
    CHECK(ns >= 1 + d.ndcmds);
    int seen_dcmd = 0;
    for (unsigned i = 0; i < ns; i++)
        for (unsigned k = 0; k < seeds[i].nops; k++)
            if (seeds[i].ops[k].kind == RF_OP_DEVCTL && seeds[i].ops[k].a == (uint32_t)DCMD_BUGGY_COMPUTE) seen_dcmd = 1;
    CHECK(seen_dcmd);

    rf_tc tc;
    rf_tc_copy(&tc, &seeds[0]);
    unsigned long raw_seen = 0, kinds_changed = 0;
    uint64_t prev = rf_tc_hash(&tc);
    for (int it = 0; it < 20000; it++) {
        m.splice_with = &seeds[it % ns];
        rf_mutate_havoc(&m, &tc);
        CHECK(tc.nops >= 1 && tc.nops <= m.max_ops);
        for (unsigned k = 0; k < tc.nops; k++) {
            CHECK(tc.ops[k].kind < RF_OP__COUNT);
            CHECK(tc.ops[k].len <= m.max_payload);
            CHECK((tc.ops[k].len == 0) == (tc.ops[k].data == NULL));
            if (tc.ops[k].kind == RF_OP_DEVCTL_RAW) raw_seen++;
        }
        uint8_t buf[RF_TC_HDR_LEN + RF_MAX_OPS * (RF_OP_HDR_LEN + RF_MAX_PAYLOAD)];
        static uint8_t *bp;
        bp = buf;
        size_t n = rf_tc_serialize(&tc, bp, sizeof buf);
        rf_tc back;
        CHECK(n > 0 && rf_tc_parse(&back, bp, n) == 0);
        if (n > 0) rf_tc_free(&back);
        uint64_t h = rf_tc_hash(&tc);
        if (h != prev) kinds_changed++;
        prev = h;
        if (tc.nops > 10 && it % 50 == 0) {   /* keep it from always sitting at the cap */
            rf_tc_free(&tc);
            rf_tc_copy(&tc, &seeds[it % ns]);
        }
    }
    CHECK(raw_seen > 0);
    CHECK(kinds_changed > 19000);
    unsigned long total = 0;
    for (int k = 0; k < RF_MUT__COUNT; k++) total += m.stats[k];
    CHECK(total > 40000);
    for (int k = 0; k < RF_MUT__COUNT; k++) CHECK(rf_mut_name((rf_mut_kind)k)[0] != '?');
    rf_tc_free(&tc);

    /* With raw disabled no raw ops may appear. */
    m.enable_raw = 0;
    rf_tc_copy(&tc, &seeds[1]);
    for (int it = 0; it < 5000; it++) {
        m.splice_with = NULL;
        rf_mutate_havoc(&m, &tc);
        for (unsigned k = 0; k < tc.nops; k++)
            CHECK(tc.ops[k].kind != RF_OP_DEVCTL_RAW && tc.ops[k].kind != RF_OP_MSG_RAW);
    }
    rf_tc_free(&tc);
    for (unsigned i = 0; i < ns; i++) rf_tc_free(&seeds[i]);
}

/* ---------------------------------------------------------------- corpus */
static void test_corpus(void)
{
    char dir[400];
    snprintf(dir, sizeof dir, "%s/corpus", tmpdir);
    rf_corpus c;
    CHECK_EQ(rf_corpus_init(&c, dir, 100), 0);
    rf_tc tc;
    make_sample(&tc);
    CHECK_EQ(rf_corpus_add(&c, &tc, 3, 1000), 0);
    CHECK_EQ(rf_corpus_add(&c, &tc, 3, 1000), -1);            /* duplicate */
    tc.ops[0].a = 1;
    CHECK_EQ(rf_corpus_add(&c, &tc, 0, 1000), 1);
    CHECK(rf_corpus_contains(&c, rf_tc_hash(&tc)));
    rf_rng r;
    rf_rng_seed(&r, 1);
    for (int i = 0; i < 100; i++) CHECK(rf_corpus_pick(&c, &r) != NULL);
    rf_corpus_free(&c);

    /* Persistence + a corrupt file that must be skipped. */
    char junk[500];
    snprintf(junk, sizeof junk, "%s/zz_junk.rfz", dir);
    CHECK_EQ(rf_write_file(junk, "garbage", 7), 0);
    rf_corpus c2;
    CHECK_EQ(rf_corpus_init(&c2, NULL, 100), 0);
    CHECK_EQ(rf_corpus_load_dir(&c2, dir), 2);
    CHECK(rf_corpus_contains(&c2, rf_tc_hash(&tc)));
    rf_corpus_free(&c2);
    rf_tc_free(&tc);
}

/* ---------------------------------------------------------------- minimize */
/* "Bug" = some DEVCTL op with a == 0x42 whose payload contains 'X'. */
static int pred_has_bug(const rf_tc *tc, void *ctx)
{
    (*(int *)ctx)++;
    for (unsigned i = 0; i < tc->nops; i++)
        if (tc->ops[i].kind == RF_OP_DEVCTL && tc->ops[i].a == 0x42 && tc->ops[i].len &&
            memchr(tc->ops[i].data, 'X', tc->ops[i].len))
            return 1;
    return 0;
}

static void test_minimize(void)
{
    rf_tc tc;
    rf_tc_init(&tc);
    uint8_t junk[200];
    for (int i = 0; i < 200; i++) junk[i] = (uint8_t)('a' + i % 20);
    for (int i = 0; i < 20; i++) rf_tc_append(&tc, RF_OP_WRITE, 0, 0, 0, junk, 50);
    junk[150] = 'X';
    rf_tc_append(&tc, RF_OP_DEVCTL, 0, 0x42, 0, junk, 200);
    for (int i = 0; i < 20; i++) rf_tc_append(&tc, RF_OP_READ, 0, 10, 0, NULL, 0);
    int calls = 0;
    rf_min_stats st = { 0, 0, 5000 };
    CHECK_EQ(rf_minimize(&tc, pred_has_bug, &calls, &st), 0);
    CHECK_EQ(tc.nops, 1);
    CHECK_EQ(tc.ops[0].a, 0x42);
    CHECK(tc.ops[0].len <= 8);
    int nonzero = 0;
    for (unsigned i = 0; i < tc.ops[0].len; i++) nonzero += tc.ops[0].data[i] != 0;
    CHECK_EQ(nonzero, 1);
    CHECK(pred_has_bug(&tc, &calls));
    CHECK(st.attempts <= 5000);
    rf_tc_free(&tc);

    /* A non-failing input is reported as such. */
    rf_tc_init(&tc);
    rf_tc_append(&tc, RF_OP_READ, 0, 1, 0, NULL, 0);
    CHECK_EQ(rf_minimize(&tc, pred_has_bug, &calls, NULL), -1);
    rf_tc_free(&tc);
}

/* ---------------------------------------------------------------- coverage */
static void test_cov(void)
{
    CHECK_EQ(rf_cov_bucket(0), 0);
    CHECK_EQ(rf_cov_bucket(1), 1);
    CHECK_EQ(rf_cov_bucket(3), 4);
    CHECK_EQ(rf_cov_bucket(7), 8);
    CHECK_EQ(rf_cov_bucket(200), 128);
    uint8_t virgin[8], map[8] = {0};
    memset(virgin, 0xff, sizeof virgin);
    unsigned edges = 0;
    map[2] = 1;
    CHECK_EQ(rf_cov_update_virgin(virgin, map, 8, &edges), 2);
    CHECK_EQ(edges, 1);
    CHECK_EQ(rf_cov_update_virgin(virgin, map, 8, &edges), 0);   /* nothing new */
    map[2] = 5;                                                    /* new hit-count bucket */
    CHECK_EQ(rf_cov_update_virgin(virgin, map, 8, &edges), 1);
    CHECK_EQ(edges, 1);

    rf_cov c;
    char name[64];
    snprintf(name, sizeof name, "/resfuzz_test_%d", (int)getpid());
    if (rf_cov_create(&c, name) == 0) {
        CHECK_EQ(c.shm->magic, RF_SHM_MAGIC);
        c.shm->map[123] = 1;
        CHECK_EQ(rf_cov_has_new(&c), 2);
        rf_cov_reset(&c);
        CHECK_EQ(c.shm->map[123], 0);
        CHECK_EQ(rf_cov_has_new(&c), 0);
        rf_cov_destroy(&c);
    } else {
        fprintf(stderr, "  (shm unavailable, skipped shm part)\n");
    }
}

/* ---------------------------------------------------------------- exec vs mock_rm */
static pid_t mock_pid = -1;
static char mock_sock[300];

static int mock_start(void)
{
    const char *bin = getenv("MOCK_RM");
    if (!bin) bin = "build/mock_rm_blind";
    char *argv[] = { (char *)bin, mock_sock, NULL };
    mock_pid = rf_plat_spawn(argv, NULL);
    if (mock_pid < 0) return -1;
    for (int i = 0; i < 300; i++) {
        rf_conn c = {0};
        if (rf_plat_open(&c, mock_sock, 2) == 0) { rf_plat_close(&c); return 0; }
        usleep(10000);
    }
    return -1;
}

static void mock_restart_if_dead(const rf_result *r)
{
    if (r->target_reaped || rf_plat_poll_child(mock_pid, NULL) != 0) { mock_pid = -1; mock_start(); }
}

static rf_result run_case(const rf_tc *tc, uint64_t deadline_ns, uint64_t hang_ns)
{
    rf_exec_cfg cfg = { mock_sock, deadline_ns, hang_ns, mock_pid, NULL, NULL, NULL };
    rf_result r;
    rf_exec(&cfg, tc, &r);
    mock_restart_if_dead(&r);
    return r;
}

static void le32(uint8_t *p, uint32_t v) { rf_put_u32(p, v); }

static int pred_crashes_mock(const rf_tc *tc, void *ctx)
{
    (void)ctx;
    rf_result r = run_case(tc, 0, 1000000000ull);
    return r.kind == RF_RES_CRASH;
}

static void test_exec_mock(void)
{
    snprintf(mock_sock, sizeof mock_sock, "%s/buggy.sock", tmpdir);
    if (mock_start() != 0) { CHECK(!"mock_rm did not start (set MOCK_RM)"); return; }
    rf_tc tc;
    rf_result r;
    uint8_t p[64];

    /* Benign session: read status, write, valid devctls. */
    rf_tc_init(&tc);
    rf_tc_append(&tc, RF_OP_READ, 0, 128, 0, NULL, 0);
    rf_tc_append(&tc, RF_OP_WRITE, 0, 0, 0, "hello", 5);
    memset(p, 0, sizeof p); le32(p, 2);
    rf_tc_append(&tc, RF_OP_DEVCTL, 0, (uint32_t)DCMD_BUGGY_GET_STATS, 16, p, 16);
    memset(p, 0, sizeof p); le32(p, 50); le32(p + 4, 0);
    rf_tc_append(&tc, RF_OP_DEVCTL, 0, (uint32_t)DCMD_BUGGY_COMPUTE, 12, p, 12);
    rf_tc_append(&tc, RF_OP_DEVCTL, 0, 0x12345, 0, NULL, 0);          /* unknown -> ENOTTY, not a crash */
    rf_tc_append(&tc, RF_OP_LSEEK, 0, 0, 0, NULL, 0);
    rf_tc_append(&tc, RF_OP_READ, 0, 128, 0, NULL, 0);
    r = run_case(&tc, 0, 1000000000ull);
    CHECK_EQ(r.kind, RF_RES_OK);
    CHECK_EQ(r.ops_ok, 6);
    CHECK_EQ(r.ops_err, 1);
    CHECK_EQ(r.nsigs, 7);
    rf_tc_free(&tc);

    /* BUG 2: GET_STATS with index 5 -> NULL deref -> crash, SIGSEGV. */
    rf_tc_init(&tc);
    memset(p, 0, sizeof p); le32(p, 5);
    rf_tc_append(&tc, RF_OP_DEVCTL, 0, (uint32_t)DCMD_BUGGY_GET_STATS, 16, p, 16);
    r = run_case(&tc, 0, 1000000000ull);
    CHECK_EQ(r.kind, RF_RES_CRASH);
    CHECK_EQ(r.op_index, 0);
    CHECK_EQ(r.err, ESRCH);
    CHECK_EQ(r.signal, SIGSEGV);
    CHECK(r.target_reaped);
    rf_tc_free(&tc);

    /* BUG 1 is gated: overflow as a normal user is harmless... */
    rf_tc_init(&tc);
    memset(p, 'A', sizeof p);
    rf_tc_append(&tc, RF_OP_DEVCTL, 0, (uint32_t)DCMD_BUGGY_SET_NAME, 64, p, 64);
    rf_tc_append(&tc, RF_OP_DEVCTL, 0, (uint32_t)DCMD_BUGGY_GET_NAME, 32, NULL, 0);
    r = run_case(&tc, 0, 1000000000ull);
    CHECK_EQ(r.kind, RF_RES_OK);
    rf_tc_free(&tc);
    /* ...but after SET_MODE{ADMN, 2} the claimed length smashes the cfg pointer. */
    rf_tc_init(&tc);
    uint8_t mode[8];
    le32(mode, BUGGY_MAGIC_ADMIN); le32(mode + 4, BUGGY_LEVEL_ADMIN);
    rf_tc_append(&tc, RF_OP_DEVCTL, 0, (uint32_t)DCMD_BUGGY_SET_MODE, 8, mode, 8);
    rf_tc_append(&tc, RF_OP_DEVCTL, 0, (uint32_t)DCMD_BUGGY_SET_NAME, 64, p, 64);
    r = run_case(&tc, 0, 1000000000ull);
    CHECK_EQ(r.kind, RF_RES_CRASH);
    CHECK_EQ(r.op_index, 1);
    CHECK(r.signal == SIGSEGV || r.signal == SIGBUS);
    rf_tc_free(&tc);
    /* Wrong magic is refused with EPERM (not a crash). */
    rf_tc_init(&tc);
    le32(mode, 0x41414141);
    rf_tc_append(&tc, RF_OP_DEVCTL, 0, (uint32_t)DCMD_BUGGY_SET_MODE, 8, mode, 8);
    r = run_case(&tc, 0, 1000000000ull);
    CHECK_EQ(r.kind, RF_RES_OK);
    CHECK_EQ(r.ops_err, 1);
    rf_tc_free(&tc);

    /* Raw devctl that lies about its length is accepted by the stub. */
    rf_tc_init(&tc);
    memset(p, 0, sizeof p);
    rf_tc_append(&tc, RF_OP_DEVCTL_RAW, 0, (uint32_t)DCMD_BUGGY_GET_STATS, 4000, p, 4);
    r = run_case(&tc, 0, 1000000000ull);
    CHECK_EQ(r.kind, RF_RES_OK);
    rf_tc_free(&tc);

    /* BUG 3a: precise COMPUTE blows a 1 ms deadline. */
    rf_tc_init(&tc);
    memset(p, 0, sizeof p); le32(p, 0x7fffffff); le32(p + 4, BUGGY_FLAG_SLOW);
    rf_tc_append(&tc, RF_OP_READ, 0, 16, 0, NULL, 0);
    rf_tc_append(&tc, RF_OP_DEVCTL, 0, (uint32_t)DCMD_BUGGY_COMPUTE, 12, p, 12);
    r = run_case(&tc, 1000000ull, 2000000000ull);
    CHECK_EQ(r.kind, RF_RES_DEADLINE);
    CHECK_EQ(r.op_index, 1);
    CHECK_EQ(r.max_latency_op, 1);
    CHECK(r.max_latency_ns > 1000000ull);
    /* Same case with the oracle off is fine. */
    r = run_case(&tc, 0, 2000000000ull);
    CHECK_EQ(r.kind, RF_RES_OK);
    rf_tc_free(&tc);

    /* BUG 3b: iterations = UINT32_MAX wraps the cap -> infinite loop -> hang. */
    rf_tc_init(&tc);
    memset(p, 0, sizeof p); le32(p, 0xffffffffu); le32(p + 4, BUGGY_FLAG_SLOW);
    rf_tc_append(&tc, RF_OP_DEVCTL, 0, (uint32_t)DCMD_BUGGY_COMPUTE, 12, p, 12);
    r = run_case(&tc, 0, 300000000ull);
    CHECK_EQ(r.kind, RF_RES_HANG);
    CHECK_EQ(r.err, ETIMEDOUT);
    rf_plat_kill(mock_pid);                       /* the server is spinning: replace it */
    mock_pid = -1;
    CHECK_EQ(mock_start(), 0);
    rf_tc_free(&tc);

    /* Setup failure when the target does not exist. */
    rf_tc_init(&tc);
    rf_tc_append(&tc, RF_OP_READ, 0, 1, 0, NULL, 0);
    rf_exec_cfg bad = { "/nonexistent/sock", 0, 100000000ull, -1, NULL, NULL, NULL };
    rf_exec(&bad, &tc, &r);
    CHECK_EQ(r.kind, RF_RES_SETUP_FAIL);
    rf_tc_free(&tc);

    /* Minimiser end to end against the real server (crash predicate). */
    rf_tc_init(&tc);
    for (int i = 0; i < 6; i++) rf_tc_append(&tc, RF_OP_WRITE, 0, 0, 0, "noise", 5);
    memset(p, 0x11, sizeof p); le32(p, 6);
    rf_tc_append(&tc, RF_OP_DEVCTL, 0, (uint32_t)DCMD_BUGGY_GET_STATS, 40, p, 40);
    rf_tc_append(&tc, RF_OP_READ, 0, 16, 0, NULL, 0);
    rf_min_stats st = { 0, 0, 500 };
    CHECK_EQ(rf_minimize(&tc, pred_crashes_mock, NULL, &st), 0);
    CHECK_EQ(tc.nops, 1);
    CHECK_EQ(tc.ops[0].a, (uint32_t)DCMD_BUGGY_GET_STATS);
    CHECK(tc.ops[0].len >= 1 && tc.ops[0].len <= 4);
    rf_tc_free(&tc);

    if (mock_pid > 0) rf_plat_kill(mock_pid);
}

/* ---------------------------------------------------------------- main */
int main(void)
{
    snprintf(tmpdir, sizeof tmpdir, "/tmp/resfuzz_ut_%d", (int)getpid());
    if (rf_mkdirs(tmpdir) != 0) { perror("mkdir"); return 1; }
    rf_plat_init();
    tt_case cases[] = {
        { "rng", test_rng },
        { "testcase_roundtrip", test_testcase_roundtrip },
        { "testcase_parser_fuzz", test_testcase_parser_fuzz },
        { "dict", test_dict },
        { "mutate_invariants", test_mutate_invariants },
        { "corpus", test_corpus },
        { "minimize", test_minimize },
        { "cov", test_cov },
        { "exec_mock_rm", test_exec_mock },
    };
    for (unsigned i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        int before = tt_failures;
        cases[i].fn();
        printf("%-24s %s\n", cases[i].name, tt_failures == before ? "ok" : "FAILED");
    }
    printf("%d checks, %d failures\n", tt_checks, tt_failures);
    char cmd[400];
    snprintf(cmd, sizeof cmd, "rm -rf %s", tmpdir);
    if (system(cmd) != 0) { /* best effort */ }
    return tt_failures ? 1 : 0;
}
