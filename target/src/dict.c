/* SPDX-License-Identifier: Apache-2.0 */
#include "rf_dict.h"
#include "rf_util.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

void rf_dict_init(rf_dict *d) { memset(d, 0, sizeof *d); }

int rf_dict_add_token(rf_dict *d, const void *bytes, size_t len)
{
    if (len == 0 || len > RF_DICT_MAX_TOKEN || d->ntokens >= RF_DICT_MAX_TOKENS) return 0;
    for (unsigned i = 0; i < d->ntokens; i++)
        if (d->tokens[i].len == len && memcmp(d->tokens[i].bytes, bytes, len) == 0) return 0;
    d->tokens[d->ntokens].len = (uint8_t)len;
    memcpy(d->tokens[d->ntokens].bytes, bytes, len);
    d->ntokens++;
    return 1;
}

int rf_dict_add_dcmd(rf_dict *d, const char *name, uint32_t value, uint32_t size, uint8_t dir)
{
    if (d->ndcmds >= RF_DICT_MAX_DCMDS) return 0;
    for (unsigned i = 0; i < d->ndcmds; i++)
        if (d->dcmds[i].value == value) return 0;
    rf_dcmd *c = &d->dcmds[d->ndcmds++];
    memset(c, 0, sizeof *c);
    strncpy(c->name, name ? name : "?", sizeof c->name - 1);
    c->value = value; c->size = size; c->dir = dir & 3u;
    return 1;
}

static int hexval(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    c = tolower(c);
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

/* Parse "..." with \\ \" \xNN escapes. Returns token length or -1. */
static int parse_quoted(const char *p, const char *end, uint8_t *out)
{
    if (p >= end || *p != '"') return -1;
    p++;
    int n = 0;
    while (p < end && *p != '"') {
        int c = (unsigned char)*p++;
        if (c == '\\') {
            if (p >= end) return -1;
            c = (unsigned char)*p++;
            if (c == 'x') {
                if (end - p < 2) return -1;
                int h = hexval(p[0]), l = hexval(p[1]);
                if (h < 0 || l < 0) return -1;
                c = h * 16 + l;
                p += 2;
            } else if (c != '\\' && c != '"') {
                return -1;
            }
        }
        if (n >= (int)RF_DICT_MAX_TOKEN) return -1;
        out[n++] = (uint8_t)c;
    }
    if (p >= end) return -1;            /* missing closing quote */
    p++;
    while (p < end && isspace((unsigned char)*p)) p++;
    return p == end ? n : -1;           /* nothing allowed after the token */
}

static int parse_dcmd(rf_dict *d, char *line)
{
    char name[48] = {0}, dirs[16] = {0};
    char *save = NULL, *tok;
    unsigned long value = 0, size = 0;
    int have_value = 0, have_size = 0;
    uint8_t dir = RF_DIR_NONE;
    int idx = 0;
    for (tok = strtok_r(line, " \t", &save); tok; tok = strtok_r(NULL, " \t", &save), idx++) {
        char *e;
        if (idx == 0) continue;                       /* "dcmd" */
        if (idx == 1) { strncpy(name, tok, sizeof name - 1); continue; }
        if (idx == 2) {
            value = strtoul(tok, &e, 0);
            if (*e || value > 0xffffffffUL) return -1;
            have_value = 1;
            continue;
        }
        if (strncmp(tok, "size=", 5) == 0) {
            size = strtoul(tok + 5, &e, 0);
            if (*e || size > 0xffffUL) return -1;
            have_size = 1;
        } else if (strncmp(tok, "dir=", 4) == 0) {
            strncpy(dirs, tok + 4, sizeof dirs - 1);
            if (!strcmp(dirs, "to")) dir = RF_DIR_TO;
            else if (!strcmp(dirs, "from")) dir = RF_DIR_FROM;
            else if (!strcmp(dirs, "tofrom")) dir = RF_DIR_TOFROM;
            else if (!strcmp(dirs, "none")) dir = RF_DIR_NONE;
            else return -1;
        } else {
            return -1;
        }
    }
    if (!have_value || !name[0]) return -1;
    (void)have_size;
    return rf_dict_add_dcmd(d, name, (uint32_t)value, (uint32_t)size, dir);
}

int rf_dict_parse(rf_dict *d, const char *text, size_t len, int *err_line)
{
    int added = 0, lineno = 0;
    size_t i = 0;
    while (i < len) {
        size_t j = i;
        while (j < len && text[j] != '\n') j++;
        lineno++;
        char line[512];
        size_t ll = j - i;
        if (ll >= sizeof line) { if (err_line) *err_line = lineno; return -1; }
        memcpy(line, text + i, ll);
        line[ll] = '\0';
        i = j + 1;

        char *s = line;
        while (isspace((unsigned char)*s)) s++;
        char *e = s + strlen(s);
        while (e > s && isspace((unsigned char)e[-1])) *--e = '\0';
        if (*s == '\0' || *s == '#') continue;

        int r;
        if (strncmp(s, "dcmd", 4) == 0 && isspace((unsigned char)s[4])) {
            r = parse_dcmd(d, s);
        } else {
            /* name="value" or just "value" (AFL format allows an optional @level). */
            char *q = strchr(s, '"');
            uint8_t tok[RF_DICT_MAX_TOKEN];
            if (!q) { if (err_line) *err_line = lineno; return -1; }
            int n = parse_quoted(q, e, tok);
            r = n <= 0 ? -1 : rf_dict_add_token(d, tok, (size_t)n);
        }
        if (r < 0) { if (err_line) *err_line = lineno; return -1; }
        added += r;
    }
    return added;
}

int rf_dict_load(rf_dict *d, const char *path)
{
    uint8_t *buf;
    size_t n;
    int line = 0;
    if (rf_read_file(path, &buf, &n, 1u << 20) != 0) return -1;
    int r = rf_dict_parse(d, (const char *)buf, n, &line);
    free(buf);
    return r;
}
