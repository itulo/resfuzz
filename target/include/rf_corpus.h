/* SPDX-License-Identifier: Apache-2.0 */
/* In-memory corpus with on-disk persistence and hash de-duplication. */
#ifndef RF_CORPUS_H
#define RF_CORPUS_H
#include "rf_testcase.h"
#include "rf_rng.h"

typedef struct {
    rf_tc    tc;
    uint64_t hash;
    uint32_t new_edges;   /* edges this entry discovered */
    uint32_t picks;       /* times chosen for mutation */
    uint64_t exec_ns;     /* last execution time */
    uint8_t  favored;
} rf_corpus_entry;

typedef struct {
    rf_corpus_entry *e;
    unsigned n, cap, max;
    char dir[512];        /* "" = memory only */
} rf_corpus;

int  rf_corpus_init(rf_corpus *c, const char *dir, unsigned max_entries);
void rf_corpus_free(rf_corpus *c);
/* Add a deep copy unless an identical test case exists. Returns index, -1 if dup/full. */
int  rf_corpus_add(rf_corpus *c, const rf_tc *tc, uint32_t new_edges, uint64_t exec_ns);
int  rf_corpus_contains(const rf_corpus *c, uint64_t hash);
/* Load every *.rfz file in dir (non-recursive). Returns count loaded, skips bad files. */
int  rf_corpus_load_dir(rf_corpus *c, const char *dir);
/* Weighted pick: favored / fast / productive entries are chosen more often. */
rf_corpus_entry *rf_corpus_pick(rf_corpus *c, rf_rng *rng);

#endif
