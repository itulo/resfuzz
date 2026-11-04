/* SPDX-License-Identifier: Apache-2.0 */
/* Small portable helpers shared by the resfuzz agent and its tests. */
#ifndef RF_UTIL_H
#define RF_UTIL_H

#include <stddef.h>
#include <stdint.h>

#define RF_MIN(a, b) ((a) < (b) ? (a) : (b))
#define RF_MAX(a, b) ((a) > (b) ? (a) : (b))
#define RF_ARRAY_LEN(x) (sizeof(x) / sizeof((x)[0]))

/* 64-bit FNV-1a; used to name and de-duplicate test cases. */
uint64_t rf_fnv1a64(const void *data, size_t len, uint64_t seed);

/* Little-endian load/store, independent of host byte order. */
static inline void rf_put_u16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static inline void rf_put_u32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}
static inline uint16_t rf_get_u16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static inline uint32_t rf_get_u32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* Read a whole file into a malloc'd buffer (max_len guards against huge inputs).
 * Returns 0 on success, -1 on error. */
int rf_read_file(const char *path, uint8_t **out, size_t *out_len, size_t max_len);
int rf_write_file(const char *path, const void *data, size_t len);
/* mkdir -p for one level-at-a-time paths; returns 0 if it exists afterwards. */
int rf_mkdirs(const char *path);

#endif
