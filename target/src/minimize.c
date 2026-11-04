/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Minimiser: (1) remove ops with shrinking chunk sizes (ddmin-style),
 * (2) truncate/halve payloads, (3) zero payload bytes so the remaining
 * non-zero bytes are the ones that matter. Portable; on target the predicate
 * respawns the server and replays the candidate.
 */
#include "rf_minimize.h"

#include <string.h>

static int try_candidate(rf_tc *cur, rf_tc *cand, rf_still_fails_fn pred, void *ctx, rf_min_stats *st)
{
    if (st->max_attempts && st->attempts >= st->max_attempts) { rf_tc_free(cand); return 0; }
    st->attempts++;
    if (pred(cand, ctx)) {
        rf_tc_free(cur);
        *cur = *cand;           /* take ownership */
        st->successes++;
        return 1;
    }
    rf_tc_free(cand);
    return 0;
}

int rf_minimize(rf_tc *tc, rf_still_fails_fn pred, void *ctx, rf_min_stats *st)
{
    rf_min_stats local = {0, 0, 0};
    if (!st) st = &local;
    st->attempts++;
    if (!pred(tc, ctx)) return -1;

    /* Pass 1: op removal with decreasing chunk size. */
    for (unsigned chunk = tc->nops / 2 ? tc->nops / 2 : 1; chunk >= 1; chunk /= 2) {
        int progress = 1;
        while (progress) {
            progress = 0;
            for (unsigned start = 0; start < tc->nops && tc->nops > 1;) {
                rf_tc cand;
                if (rf_tc_copy(&cand, tc) != 0) return 0;
                unsigned n = chunk;
                if (start + n > cand.nops) n = cand.nops - start;
                if (n >= cand.nops) { rf_tc_free(&cand); break; }
                for (unsigned k = 0; k < n; k++) rf_tc_remove(&cand, start);
                if (try_candidate(tc, &cand, pred, ctx, st)) progress = 1;
                else start += n;
            }
        }
        if (chunk == 1) break;
    }

    /* Pass 2: shorten payloads. Candidates per round: keep the head half,
     * keep the tail half, drop the last byte, drop the first byte. */
    for (unsigned i = 0; i < tc->nops; i++) {
        int progress = 1;
        while (progress && tc->ops[i].len > 0) {
            progress = 0;
            unsigned len = tc->ops[i].len;
            unsigned half = len / 2;
            struct { unsigned off, n; } cands[4] = {
                { 0, half }, { len - half, half }, { 0, len - 1 }, { 1, len - 1 },
            };
            for (unsigned k = 0; k < 4 && !progress; k++) {
                if (k < 2 && len < 4) continue;
                rf_tc cand;
                if (rf_tc_copy(&cand, tc) != 0) return 0;
                if (rf_op_set_payload(&cand.ops[i], tc->ops[i].data + cands[k].off, cands[k].n) != 0) {
                    rf_tc_free(&cand);
                    return 0;
                }
                if (try_candidate(tc, &cand, pred, ctx, st)) progress = 1;
            }
            if (st->max_attempts && st->attempts >= st->max_attempts) break;
        }
    }

    /* Pass 3: zero bytes (keeps length, simplifies content). */
    for (unsigned i = 0; i < tc->nops; i++) {
        for (unsigned j = 0; j < tc->ops[i].len; j++) {
            if (tc->ops[i].data[j] == 0) continue;
            rf_tc cand;
            if (rf_tc_copy(&cand, tc) != 0) return 0;
            cand.ops[i].data[j] = 0;
            try_candidate(tc, &cand, pred, ctx, st);
        }
    }
    return 0;
}
