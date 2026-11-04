/* SPDX-License-Identifier: Apache-2.0 */
#include "rf_rng.h"

static uint64_t splitmix64(uint64_t *x)
{
    uint64_t z = (*x += 0x9e3779b97f4a7c15ull);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
    return z ^ (z >> 31);
}

static inline uint64_t rotl(uint64_t x, int k) { return (x << k) | (x >> (64 - k)); }

void rf_rng_seed(rf_rng *r, uint64_t seed)
{
    for (int i = 0; i < 4; i++) r->s[i] = splitmix64(&seed);
}

uint64_t rf_rng_next(rf_rng *r)
{
    uint64_t *s = r->s;
    uint64_t result = rotl(s[1] * 5, 7) * 9;
    uint64_t t = s[1] << 17;
    s[2] ^= s[0]; s[3] ^= s[1]; s[1] ^= s[2]; s[0] ^= s[3];
    s[2] ^= t;
    s[3] = rotl(s[3], 45);
    return result;
}

uint32_t rf_rng_below(rf_rng *r, uint32_t n)
{
    if (n == 0) return 0;
    /* Lemire's multiply-shift; bias is negligible for fuzzing purposes. */
    return (uint32_t)(((rf_rng_next(r) >> 32) * (uint64_t)n) >> 32);
}

int rf_rng_one_in(rf_rng *r, uint32_t n) { return n <= 1 || rf_rng_below(r, n) == 0; }
