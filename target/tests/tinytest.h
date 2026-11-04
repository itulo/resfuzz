/* SPDX-License-Identifier: Apache-2.0 */
/* Minimal single-header test harness. */
#ifndef TINYTEST_H
#define TINYTEST_H
#include <stdio.h>
#include <string.h>
extern int tt_failures, tt_checks;
#define CHECK(cond) do { tt_checks++; if (!(cond)) { tt_failures++; \
    fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define CHECK_EQ(a, b) do { long long _a = (long long)(a), _b = (long long)(b); tt_checks++; \
    if (_a != _b) { tt_failures++; fprintf(stderr, "  FAIL %s:%d: %s == %s (%lld vs %lld)\n", \
    __FILE__, __LINE__, #a, #b, _a, _b); } } while (0)
typedef void (*tt_fn)(void);
typedef struct { const char *name; tt_fn fn; } tt_case;
#endif
