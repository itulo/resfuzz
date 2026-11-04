/* SPDX-License-Identifier: Apache-2.0 */
/* Test-case minimisation (delta debugging over ops, then payload bytes). */
#ifndef RF_MINIMIZE_H
#define RF_MINIMIZE_H
#include "rf_testcase.h"

/* Predicate: return 1 if the candidate still reproduces the finding. */
typedef int (*rf_still_fails_fn)(const rf_tc *candidate, void *ctx);

typedef struct { unsigned attempts, successes, max_attempts; } rf_min_stats;

/* Shrinks tc in place. Never returns a candidate for which pred() was not 1.
 * Returns 0 on success (possibly unchanged), -1 if pred(original) == 0. */
int rf_minimize(rf_tc *tc, rf_still_fails_fn pred, void *ctx, rf_min_stats *st);

#endif
