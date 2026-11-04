/* SPDX-License-Identifier: Apache-2.0 */
/* Deterministic PRNG (xoshiro256**, seeded with splitmix64). Portable. */
#ifndef RF_RNG_H
#define RF_RNG_H
#include <stdint.h>

typedef struct { uint64_t s[4]; } rf_rng;

void     rf_rng_seed(rf_rng *r, uint64_t seed);
uint64_t rf_rng_next(rf_rng *r);
/* Uniform in [0, n). n == 0 returns 0. */
uint32_t rf_rng_below(rf_rng *r, uint32_t n);
/* True with probability 1/n. */
int      rf_rng_one_in(rf_rng *r, uint32_t n);

#endif
