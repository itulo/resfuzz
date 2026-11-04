/* SPDX-License-Identifier: Apache-2.0 */
#include "rf_corpus.h"
#include "rf_util.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int rf_corpus_init(rf_corpus *c, const char *dir, unsigned max_entries)
{
    memset(c, 0, sizeof *c);
    c->max = max_entries ? max_entries : 4096;
    if (dir && *dir) {
        if (strlen(dir) >= sizeof c->dir || rf_mkdirs(dir) != 0) return -1;
        strcpy(c->dir, dir);
    }
    return 0;
}

void rf_corpus_free(rf_corpus *c)
{
    for (unsigned i = 0; i < c->n; i++) rf_tc_free(&c->e[i].tc);
    free(c->e);
    memset(c, 0, sizeof *c);
}

int rf_corpus_contains(const rf_corpus *c, uint64_t hash)
{
    for (unsigned i = 0; i < c->n; i++)
        if (c->e[i].hash == hash) return 1;
    return 0;
}

static int add_entry(rf_corpus *c, const rf_tc *tc, uint64_t h, uint32_t new_edges, uint64_t exec_ns,
                     int persist)
{
    if (c->n >= c->max) return -1;
    if (c->n == c->cap) {
        unsigned ncap = c->cap ? c->cap * 2 : 64;
        rf_corpus_entry *ne = realloc(c->e, ncap * sizeof *ne);
        if (!ne) return -1;
        c->e = ne;
        c->cap = ncap;
    }
    rf_corpus_entry *e = &c->e[c->n];
    memset(e, 0, sizeof *e);
    if (rf_tc_copy(&e->tc, tc) != 0) return -1;
    e->hash = h;
    e->new_edges = new_edges;
    e->exec_ns = exec_ns;
    e->favored = new_edges > 0;
    if (persist && c->dir[0]) {
        char path[640];
        snprintf(path, sizeof path, "%s/id_%06u_%016llx.rfz", c->dir, c->n, (unsigned long long)h);
        if (rf_tc_save(tc, path) != 0) fprintf(stderr, "resfuzz: warning: cannot save %s\n", path);
    }
    return (int)c->n++;
}

int rf_corpus_add(rf_corpus *c, const rf_tc *tc, uint32_t new_edges, uint64_t exec_ns)
{
    uint64_t h = rf_tc_hash(tc);
    if (rf_corpus_contains(c, h)) return -1;
    return add_entry(c, tc, h, new_edges, exec_ns, 1);
}

int rf_corpus_load_dir(rf_corpus *c, const char *dir)
{
    DIR *d = opendir(dir);
    if (!d) return -1;
    int loaded = 0;
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        size_t n = strlen(de->d_name);
        if (n < 5 || strcmp(de->d_name + n - 4, ".rfz") != 0) continue;
        char path[1024];
        if (snprintf(path, sizeof path, "%s/%s", dir, de->d_name) >= (int)sizeof path) continue;
        rf_tc tc;
        if (rf_tc_load(&tc, path) != 0) {
            fprintf(stderr, "resfuzz: skipping malformed corpus file %s\n", path);
            continue;
        }
        uint64_t h = rf_tc_hash(&tc);
        /* Files already in our own corpus dir must not be re-written. */
        if (!rf_corpus_contains(c, h) && add_entry(c, &tc, h, 0, 0, strcmp(dir, c->dir) != 0) >= 0)
            loaded++;
        rf_tc_free(&tc);
    }
    closedir(d);
    return loaded;
}

rf_corpus_entry *rf_corpus_pick(rf_corpus *c, rf_rng *rng)
{
    if (!c->n) return NULL;
    /* Two-candidate tournament: prefer favored, then fewer picks, then faster. */
    rf_corpus_entry *a = &c->e[rf_rng_below(rng, c->n)];
    rf_corpus_entry *b = &c->e[rf_rng_below(rng, c->n)];
    rf_corpus_entry *w;
    if (a->favored != b->favored) w = a->favored ? a : b;
    else if (a->picks != b->picks) w = a->picks < b->picks ? a : b;
    else w = a->exec_ns <= b->exec_ns ? a : b;
    w->picks++;
    return w;
}
