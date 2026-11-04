/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Coverage feedback shared between the agent and the instrumented server.
 * The server is compiled with GCC -fsanitize-coverage=trace-pc,trace-cmp and
 * linked with covrt/cov_rt.c, which writes edge hits and recent comparison
 * operands into a POSIX shared-memory object named by $RESFUZZ_SHM.
 * Layout is plain C, fixed-size, no pointers, so both processes agree.
 */
#ifndef RF_COV_H
#define RF_COV_H
#include <stdint.h>
#include <stddef.h>

#define RF_MAP_SIZE      65536u
#define RF_CMPLOG_N      256u
#define RF_SHM_MAGIC     0x52464356u  /* "RFCV" */

typedef struct {
    uint64_t a, b;
    uint8_t  size;  /* operand size in bytes: 1, 2, 4, 8 */
    uint8_t  is_const;
    uint8_t  pad[6];
} rf_cmp_entry;

typedef struct {
    uint32_t     magic;
    uint32_t     cmp_count;            /* monotonically increasing; index = count % N */
    rf_cmp_entry cmp[RF_CMPLOG_N];
    uint8_t      map[RF_MAP_SIZE];
} rf_cov_shm;

/* Agent side. */
typedef struct {
    rf_cov_shm *shm;
    char        name[64];
    uint8_t     virgin[RF_MAP_SIZE];   /* AFL-style: bits not yet seen */
    unsigned    edges_seen;
} rf_cov;

int  rf_cov_create(rf_cov *c, const char *name);   /* shm_open(O_CREAT) + mmap */
void rf_cov_destroy(rf_cov *c);
void rf_cov_reset(rf_cov *c);                      /* clear map + cmplog before an exec */
/* Bucket hit counts (1,2,3,4-7,8-15,16-31,32-127,128+) and compare with virgin.
 * Returns 2 = new edge, 1 = new hit-count bucket, 0 = nothing new. Updates virgin. */
int  rf_cov_has_new(rf_cov *c);
/* Pure helpers (unit-tested). */
uint8_t rf_cov_bucket(uint8_t count);
int  rf_cov_update_virgin(uint8_t *virgin, const uint8_t *map, size_t n, unsigned *edges_seen);

#endif
