/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Agent side of the coverage channel. Uses POSIX shm_open()/mmap(), which
 * QNX 8.0 documents in its C library reference:
 *   https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.lib_ref/topic/s/shm_open.html
 * (UNVERIFIED on target until the SDP is installed; see docs/TODO-QNX.md.)
 */
#include "rf_cov.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

uint8_t rf_cov_bucket(uint8_t c)
{
    if (c == 0) return 0;
    if (c == 1) return 1;
    if (c == 2) return 2;
    if (c == 3) return 4;
    if (c <= 7) return 8;
    if (c <= 15) return 16;
    if (c <= 31) return 32;
    if (c <= 127) return 64;
    return 128;
}

int rf_cov_update_virgin(uint8_t *virgin, const uint8_t *map, size_t n, unsigned *edges_seen)
{
    int ret = 0;
    for (size_t i = 0; i < n; i++) {
        if (!map[i]) continue;
        uint8_t b = rf_cov_bucket(map[i]);
        if (virgin[i] & b) {
            if (virgin[i] == 0xff) { ret = 2; if (edges_seen) (*edges_seen)++; }
            else if (ret < 1) ret = 1;
            virgin[i] &= (uint8_t)~b;
        }
    }
    return ret;
}

int rf_cov_create(rf_cov *c, const char *name)
{
    memset(c, 0, sizeof *c);
    memset(c->virgin, 0xff, sizeof c->virgin);
    snprintf(c->name, sizeof c->name, "%s", name);
    int fd = shm_open(c->name, O_RDWR | O_CREAT, 0600);
    if (fd < 0) return -1;
    if (ftruncate(fd, (off_t)sizeof(rf_cov_shm)) != 0) { close(fd); shm_unlink(c->name); return -1; }
    void *p = mmap(NULL, sizeof(rf_cov_shm), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (p == MAP_FAILED) { shm_unlink(c->name); return -1; }
    c->shm = p;
    memset(c->shm, 0, sizeof *c->shm);
    c->shm->magic = RF_SHM_MAGIC;
    return 0;
}

void rf_cov_destroy(rf_cov *c)
{
    if (c->shm) {
        munmap(c->shm, sizeof(rf_cov_shm));
        shm_unlink(c->name);
        c->shm = NULL;
    }
}

void rf_cov_reset(rf_cov *c)
{
    if (!c->shm) return;
    memset(c->shm->map, 0, sizeof c->shm->map);
    c->shm->cmp_count = 0;
}

int rf_cov_has_new(rf_cov *c)
{
    if (!c->shm) return 0;
    return rf_cov_update_virgin(c->virgin, c->shm->map, RF_MAP_SIZE, &c->edges_seen);
}
