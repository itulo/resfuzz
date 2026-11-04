/* SPDX-License-Identifier: Apache-2.0 */
/*
 * The deliberately buggy core of buggy_rm. Three planted bugs, each a
 * classic resource-manager mistake:
 *
 *  BUG 1  (memory corruption, devctl buffer overflow)
 *         DCMD_BUGGY_SET_NAME in admin mode copies msg->i.nbytes bytes - the
 *         length the CLIENT claims - into a 32-byte field, instead of
 *         min(sizeof field, bytes actually received). Neighbouring pointer
 *         gets overwritten -> SIGSEGV on next use.
 *         Gate: requires a prior SET_MODE{magic "ADMN", level 2}; blind
 *         fuzzing rarely gets there, coverage + cmp feedback or a header
 *         derived dictionary does.
 *  BUG 2  (NULL dereference on a bad message)
 *         DCMD_BUGGY_GET_STATS indexes an 8-slot table of which only 4 slots
 *         are populated, with an index taken straight from the message.
 *  BUG 3  (real-time deadline miss, and a hang)
 *         3a: DCMD_BUGGY_COMPUTE with flags == BUGGY_FLAG_SLOW does up to
 *         2^18 iterations of work inline on the (single) server thread -
 *         tens of ms during which every client is blocked. resfuzz's
 *         deadline oracle measures exactly that reply latency.
 *         3b: the cap is `if (n + 1 > MAX)`, which wraps for n == UINT32_MAX,
 *         and the loop is `for (uint32_t i = 0; i <= n; i++)`: never ends.
 */
#include "buggy_logic.h"
#include "buggy_rm.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

struct counters { uint32_t reads, writes, devctls; };
struct name_cfg { uint32_t max_len; uint32_t flags; };

static const struct name_cfg default_cfg = { BUGGY_NAME_LEN, 0 };
static struct counters ctr_main, ctr_admin, ctr_user, ctr_misc;

static struct {
    struct {
        char                   name[BUGGY_NAME_LEN];
        const struct name_cfg *cfg;          /* sits right after name[]: BUG 1 target */
    } id;
    uint32_t         level;
    struct counters *table[8];               /* only [0..3] populated: BUG 2 */
    char             last_write[64];
} st;

void buggy_init(void)
{
    memset(&st, 0, sizeof st);
    strcpy(st.id.name, "buggy");
    st.id.cfg = &default_cfg;
    st.level = BUGGY_LEVEL_USER;
    st.table[0] = &ctr_main; st.table[1] = &ctr_admin; st.table[2] = &ctr_user; st.table[3] = &ctr_misc;
}

static uint32_t rd32(const uint8_t *p, size_t avail, size_t off)
{
    uint32_t v = 0;
    if (off + 4 <= avail) memcpy(&v, p + off, 4);
    return v;
}

static volatile uint32_t sink;

#define PRECISE_MAX_ITER (1u << 18)   /* ~10-30 ms of work on a desktop-class core */

static uint32_t compute(uint32_t n, uint32_t flags)
{
    uint32_t acc = 0x12345678u;
    if (flags != BUGGY_FLAG_SLOW) {
        if (n > 100) n = 100;                    /* fast mode: bounded, always within deadline */
    } else if (n + 1 > PRECISE_MAX_ITER) {       /* BUG 3b: n + 1 wraps to 0 for n == UINT32_MAX */
        n = PRECISE_MAX_ITER - 1;                /* BUG 3a: still far too long for an RT server */
    }
    for (uint32_t i = 0; i <= n; i++) {          /* n == UINT32_MAX: i <= n is always true -> hang */
        for (int k = 0; k < 64; k++) acc = acc * 1664525u + 1013904223u;
        sink = acc;
    }
    return acc;
}

int buggy_devctl(int dcmd, const uint8_t *in, size_t claimed, size_t avail,
                 uint8_t *out, size_t outcap, size_t *outlen)
{
    *outlen = 0;
    ctr_main.devctls++;
    switch (dcmd) {
    case DCMD_BUGGY_SET_MODE: {
        if (avail < sizeof(struct buggy_mode)) return EINVAL;
        uint32_t magic = rd32(in, avail, 0), level = rd32(in, avail, 4);
        if (level == BUGGY_LEVEL_ADMIN) {
            if (magic != BUGGY_MAGIC_ADMIN) return EPERM;
            ctr_admin.devctls++;
        } else if (level != BUGGY_LEVEL_USER) {
            return EINVAL;
        }
        st.level = level;
        return 0;
    }
    case DCMD_BUGGY_SET_NAME:
        if (st.level == BUGGY_LEVEL_ADMIN) {
            /* BUG 1: trusts the header's nbytes; should be
             *        min(claimed, avail, sizeof st.id.name - 1). */
            memcpy(st.id.name, in, claimed);
        } else {
            size_t n = avail < sizeof st.id.name - 1 ? avail : sizeof st.id.name - 1;
            memcpy(st.id.name, in, n);
            st.id.name[n] = '\0';
        }
        /* Uses the (possibly corrupted) cfg pointer. */
        st.id.name[st.id.cfg->max_len - 1] = '\0';
        return 0;
    case DCMD_BUGGY_GET_NAME:
        if (outcap < sizeof(struct buggy_name)) return EOVERFLOW;
        memset(out, 0, sizeof(struct buggy_name));
        memcpy(out, st.id.name, strnlen(st.id.name, BUGGY_NAME_LEN - 1));
        *outlen = sizeof(struct buggy_name);
        return 0;
    case DCMD_BUGGY_GET_STATS: {
        uint32_t idx = rd32(in, avail, 0);
        if (idx > 7) return EINVAL;              /* range-checks the table size... */
        struct counters *c = st.table[idx];      /* BUG 2: ...not which slots exist */
        struct buggy_stats s = { idx, c->reads, c->writes, c->devctls };
        if (outcap < sizeof s) return EOVERFLOW;
        memcpy(out, &s, sizeof s);
        *outlen = sizeof s;
        return 0;
    }
    case DCMD_BUGGY_COMPUTE: {
        if (avail < 8) return EINVAL;
        struct buggy_compute c = { rd32(in, avail, 0), rd32(in, avail, 4), 0 };
        c.result = compute(c.iterations, c.flags);
        if (outcap < sizeof c) return EOVERFLOW;
        memcpy(out, &c, sizeof c);
        *outlen = sizeof c;
        return 0;
    }
    case DCMD_BUGGY_RESET:
        buggy_init();
        return 0;
    default:
        return ENOTTY;   /* devctl() docs: "The device doesn't support the dcmd command." */
    }
}

size_t buggy_status_text(char *buf, size_t cap)
{
    ctr_main.reads++;
    int n = snprintf(buf, cap, "buggy_rm name=%.31s level=%u writes=%u\n", st.id.name,
                     (unsigned)st.level, (unsigned)ctr_main.writes);
    return n < 0 ? 0 : ((size_t)n < cap ? (size_t)n : cap - 1);
}

int buggy_write(const uint8_t *data, size_t len)
{
    ctr_main.writes++;
    size_t n = len < sizeof st.last_write - 1 ? len : sizeof st.last_write - 1;   /* correct */
    memcpy(st.last_write, data, n);
    st.last_write[n] = '\0';
    if (n >= 6 && memcmp(st.last_write, "reset\n", 6) == 0) buggy_init();
    return 0;
}
