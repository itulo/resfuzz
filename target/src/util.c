/* SPDX-License-Identifier: Apache-2.0 */
#include "rf_util.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

uint64_t rf_fnv1a64(const void *data, size_t len, uint64_t seed)
{
    const uint8_t *p = data;
    uint64_t h = 0xcbf29ce484222325ull ^ seed;
    for (size_t i = 0; i < len; i++) {
        h ^= p[i];
        h *= 0x100000001b3ull;
    }
    return h;
}

int rf_read_file(const char *path, uint8_t **out, size_t *out_len, size_t max_len)
{
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    size_t cap = 4096, len = 0;
    uint8_t *buf = malloc(cap);
    if (!buf) { fclose(f); return -1; }
    for (;;) {
        if (len == cap) {
            if (cap >= max_len) { free(buf); fclose(f); return -1; }
            cap = RF_MIN(cap * 2, max_len);
            uint8_t *nb = realloc(buf, cap);
            if (!nb) { free(buf); fclose(f); return -1; }
            buf = nb;
        }
        size_t r = fread(buf + len, 1, cap - len, f);
        if (r == 0) break;
        len += r;
    }
    int err = ferror(f);
    fclose(f);
    if (err) { free(buf); return -1; }
    *out = buf;
    *out_len = len;
    return 0;
}

int rf_write_file(const char *path, const void *data, size_t len)
{
    char tmp[1024];
    if (snprintf(tmp, sizeof tmp, "%s.tmp", path) >= (int)sizeof tmp) return -1;
    FILE *f = fopen(tmp, "wb");
    if (!f) return -1;
    size_t w = fwrite(data, 1, len, f);
    if (fclose(f) != 0 || w != len) { remove(tmp); return -1; }
    return rename(tmp, path);   /* atomic: the host may copy files while we run */
}

int rf_mkdirs(const char *path)
{
    char buf[1024];
    size_t n = strlen(path);
    if (n == 0 || n >= sizeof buf) return -1;
    memcpy(buf, path, n + 1);
    for (size_t i = 1; i <= n; i++) {
        if (buf[i] == '/' || buf[i] == '\0') {
            char c = buf[i];
            buf[i] = '\0';
            if (mkdir(buf, 0755) != 0 && errno != EEXIST) return -1;
            buf[i] = c;
        }
    }
    return 0;
}
