/* SPDX-License-Identifier: Apache-2.0 */
/* Structure-aware mutation engine over rf_tc op sequences. Portable C. */
#ifndef RF_MUTATE_H
#define RF_MUTATE_H
#include "rf_testcase.h"
#include "rf_rng.h"
#include "rf_dict.h"

typedef enum {
    RF_MUT_BITFLIP = 0, RF_MUT_RANDBYTE, RF_MUT_INTERESTING, RF_MUT_ARITH,
    RF_MUT_INSERT_BYTES, RF_MUT_DELETE_BYTES, RF_MUT_CLONE_BYTES, RF_MUT_DICT_TOKEN,
    RF_MUT_FIELD_A, RF_MUT_FIELD_B, RF_MUT_DCMD_SWAP, RF_MUT_CLAIMED_LEN,
    RF_MUT_OP_INSERT, RF_MUT_OP_DELETE, RF_MUT_OP_DUP, RF_MUT_OP_SWAP, RF_MUT_SPLICE,
    RF_MUT__COUNT
} rf_mut_kind;

typedef struct {
    rf_rng        *rng;
    const rf_dict *dict;        /* may be NULL */
    const rf_tc   *splice_with; /* optional donor for RF_MUT_SPLICE */
    unsigned       max_ops;     /* <= RF_MAX_OPS */
    unsigned       max_payload; /* <= RF_MAX_PAYLOAD */
    unsigned       enable_raw;  /* allow RF_OP_DEVCTL_RAW / RF_OP_MSG_RAW */
    unsigned long  stats[RF_MUT__COUNT];
} rf_mutator;

void rf_mutator_init(rf_mutator *m, rf_rng *rng, const rf_dict *dict);
/* Apply exactly one mutation of the given kind. Returns 0 if applied. */
int  rf_mutate_one(rf_mutator *m, rf_tc *tc, rf_mut_kind kind);
/* Havoc: stack 1..2^k random mutations. Always leaves tc valid. */
void rf_mutate_havoc(rf_mutator *m, rf_tc *tc);
/* Build a random op (dictionary-aware) for generation from scratch. */
int  rf_gen_op(rf_mutator *m, rf_op *out);
/* Seed test cases: one per dictionary dcmd plus generic read/write probes. */
unsigned rf_gen_seeds(rf_mutator *m, rf_tc *out, unsigned max);
const char *rf_mut_name(rf_mut_kind k);

#endif
