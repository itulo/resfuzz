/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Mutation engine. Everything here is portable C99 and is unit-tested on
 * Linux; it knows nothing about QNX except the *shape* of a devctl call
 * (command, payload, claimed length), which is captured by rf_op.
 */
#include "rf_mutate.h"
#include "rf_util.h"

#include <stdlib.h>
#include <string.h>

static const char *const mut_names[RF_MUT__COUNT] = {
    "bitflip", "randbyte", "interesting", "arith", "insert_bytes", "delete_bytes",
    "clone_bytes", "dict_token", "field_a", "field_b", "dcmd_swap", "claimed_len",
    "op_insert", "op_delete", "op_dup", "op_swap", "splice",
};

const char *rf_mut_name(rf_mut_kind k) { return k < RF_MUT__COUNT ? mut_names[k] : "?"; }

static const int8_t   interesting8[]  = { -128, -1, 0, 1, 16, 32, 64, 100, 127 };
static const int16_t  interesting16[] = { -32768, -129, 128, 255, 256, 512, 1000, 1024, 4096, 32767 };
static const int32_t  interesting32[] = { -2147483647 - 1, -100663046, -32769, 32768, 65535, 65536,
                                          100663045, 2147483647 };
/* Lengths that commonly break length checks in message handlers. */
static const uint32_t interesting_len[] = { 0, 1, 2, 3, 4, 7, 8, 15, 16, 31, 32, 33, 63, 64, 65,
                                            127, 128, 255, 256, 511, 512, 1024, 2047, 2048, 4095,
                                            4096, 8192, 16383, 16384, 65535, 65536, 0x7fffffff,
                                            0x80000000u, 0xfffffff0u, 0xffffffffu };

void rf_mutator_init(rf_mutator *m, rf_rng *rng, const rf_dict *dict)
{
    memset(m, 0, sizeof *m);
    m->rng = rng;
    m->dict = dict;
    m->max_ops = 16;
    m->max_payload = 1024;
    m->enable_raw = 1;
}

static uint32_t pick_u32(rf_mutator *m, uint32_t n) { return rf_rng_below(m->rng, n); }

/* Pick an op that has a payload (or -1). */
static int pick_payload_op(rf_mutator *m, rf_tc *tc)
{
    unsigned idx[RF_MAX_OPS], n = 0;
    for (unsigned i = 0; i < tc->nops; i++)
        if (tc->ops[i].len) idx[n++] = i;
    return n ? (int)idx[pick_u32(m, n)] : -1;
}

static int pick_kind_op(rf_mutator *m, rf_tc *tc, int want_dcmd)
{
    unsigned idx[RF_MAX_OPS], n = 0;
    for (unsigned i = 0; i < tc->nops; i++) {
        uint8_t k = tc->ops[i].kind;
        if (!want_dcmd || k == RF_OP_DEVCTL || k == RF_OP_DEVCTL_RAW) idx[n++] = i;
    }
    return n ? (int)idx[pick_u32(m, n)] : -1;
}

static void store_le(uint8_t *p, uint64_t v, unsigned size, int swap)
{
    for (unsigned i = 0; i < size; i++) {
        unsigned j = swap ? size - 1 - i : i;
        p[j] = (uint8_t)(v >> (8 * i));
    }
}

static uint64_t load_le(const uint8_t *p, unsigned size)
{
    uint64_t v = 0;
    for (unsigned i = 0; i < size; i++) v |= (uint64_t)p[i] << (8 * i);
    return v;
}

static int resize_payload(rf_op *op, size_t newlen)
{
    if (newlen > RF_MAX_PAYLOAD) return -1;
    if (newlen == 0) { free(op->data); op->data = NULL; op->len = 0; return 0; }
    uint8_t *p = realloc(op->data, newlen);
    if (!p) return -1;
    if (newlen > op->len) memset(p + op->len, 0, newlen - op->len);
    op->data = p;
    op->len = (uint16_t)newlen;
    return 0;
}

int rf_gen_op(rf_mutator *m, rf_op *out)
{
    memset(out, 0, sizeof *out);
    const rf_dict *d = m->dict;
    uint32_t r = pick_u32(m, 100);
    uint8_t buf[256];
    size_t len = 0;

    out->flags = (uint8_t)(rf_rng_one_in(m->rng, 4) ? pick_u32(m, RF_MAX_SLOTS) : 0);
    if (r < 45 && d && d->ndcmds) {
        const rf_dcmd *c = &d->dcmds[pick_u32(m, d->ndcmds)];
        out->kind = (m->enable_raw && rf_rng_one_in(m->rng, 3)) ? RF_OP_DEVCTL_RAW : RF_OP_DEVCTL;
        out->a = c->value;
        len = RF_MIN((size_t)c->size, sizeof buf);
        for (size_t i = 0; i < len; i++) buf[i] = (uint8_t)rf_rng_next(m->rng);
        out->b = (uint32_t)len;
    } else if (r < 55) {
        out->kind = RF_OP_DEVCTL;
        out->a = (uint32_t)rf_rng_next(m->rng);
        len = pick_u32(m, 64);
        for (size_t i = 0; i < len; i++) buf[i] = (uint8_t)rf_rng_next(m->rng);
        out->b = (uint32_t)len;
    } else if (r < 70) {
        out->kind = RF_OP_WRITE;
        len = 1 + pick_u32(m, 128);
        if (d && d->ntokens && rf_rng_one_in(m->rng, 2)) {
            const rf_token *t = &d->tokens[pick_u32(m, d->ntokens)];
            len = t->len;
            memcpy(buf, t->bytes, len);
        } else {
            for (size_t i = 0; i < len; i++) buf[i] = (uint8_t)rf_rng_next(m->rng);
        }
    } else if (r < 82) {
        out->kind = RF_OP_READ;
        out->a = interesting_len[pick_u32(m, RF_ARRAY_LEN(interesting_len))] % (RF_MAX_READ + 1);
    } else if (r < 87) {
        out->kind = RF_OP_LSEEK;
        out->a = (uint32_t)interesting32[pick_u32(m, RF_ARRAY_LEN(interesting32))];
        out->b = pick_u32(m, 3);
    } else if (r < 92) {
        out->kind = RF_OP_OPEN;
        out->a = pick_u32(m, 3);
    } else if (r < 95) {
        out->kind = RF_OP_CLOSE;
    } else if (r < 98 && m->enable_raw) {
        out->kind = RF_OP_MSG_RAW;
        len = 2 + pick_u32(m, 62);
        for (size_t i = 0; i < len; i++) buf[i] = (uint8_t)rf_rng_next(m->rng);
    } else {
        out->kind = RF_OP_SLEEP;
        out->a = pick_u32(m, 2000);
    }
    return rf_op_set_payload(out, buf, len);
}

unsigned rf_gen_seeds(rf_mutator *m, rf_tc *out, unsigned max)
{
    unsigned n = 0;
    const rf_dict *d = m->dict;
    uint8_t zero[256] = {0};
    if (n < max) {   /* generic probe: read + write + read */
        rf_tc_init(&out[n]);
        rf_tc_append(&out[n], RF_OP_READ, 0, 64, 0, NULL, 0);
        rf_tc_append(&out[n], RF_OP_WRITE, 0, 0, 0, "resfuzz\n", 8);
        rf_tc_append(&out[n], RF_OP_READ, 0, 64, 0, NULL, 0);
        n++;
    }
    for (unsigned i = 0; d && i < d->ndcmds && n < max; i++) {
        const rf_dcmd *c = &d->dcmds[i];
        size_t len = RF_MIN((size_t)c->size, sizeof zero);
        rf_tc_init(&out[n]);
        rf_tc_append(&out[n], RF_OP_DEVCTL, 0, c->value, (uint32_t)len, zero, len);
        n++;
    }
    return n;
}

static int mut_payload_bytes(rf_mutator *m, rf_tc *tc, rf_mut_kind kind)
{
    int oi = pick_payload_op(m, tc);
    if (oi < 0) return -1;
    rf_op *op = &tc->ops[oi];
    uint32_t pos = pick_u32(m, op->len);
    switch (kind) {
    case RF_MUT_BITFLIP:
        op->data[pos] ^= (uint8_t)(1u << pick_u32(m, 8));
        return 0;
    case RF_MUT_RANDBYTE:
        op->data[pos] = (uint8_t)rf_rng_next(m->rng);
        return 0;
    case RF_MUT_INTERESTING: {
        unsigned which = pick_u32(m, 3), size = which == 0 ? 1 : which == 1 ? 2 : 4;
        if (op->len < size) size = 1;
        pos = pick_u32(m, op->len - size + 1);
        uint64_t v = size == 1 ? (uint8_t)interesting8[pick_u32(m, RF_ARRAY_LEN(interesting8))]
                   : size == 2 ? (uint16_t)interesting16[pick_u32(m, RF_ARRAY_LEN(interesting16))]
                               : (uint32_t)interesting32[pick_u32(m, RF_ARRAY_LEN(interesting32))];
        if (size == 4 && rf_rng_one_in(m->rng, 2)) {
            /* devctl payloads are C structs: put enum/level/index-like small
             * values (0..8) into a naturally aligned u32 field. */
            pos = 4 * pick_u32(m, op->len / 4);
            v = pick_u32(m, 9);
            store_le(op->data + pos, v, 4, 0);
            return 0;
        }
        store_le(op->data + pos, v, size, rf_rng_one_in(m->rng, 2));
        return 0;
    }
    case RF_MUT_ARITH: {
        unsigned size = op->len >= 4 && rf_rng_one_in(m->rng, 2) ? 4 : 1;
        pos = pick_u32(m, op->len - size + 1);
        uint64_t v = load_le(op->data + pos, size);
        int delta = 1 + (int)pick_u32(m, 35);
        v = rf_rng_one_in(m->rng, 2) ? v + (uint64_t)delta : v - (uint64_t)delta;
        store_le(op->data + pos, v, size, 0);
        return 0;
    }
    case RF_MUT_DELETE_BYTES: {
        if (op->len < 2) return -1;
        uint32_t n = 1 + pick_u32(m, op->len - 1);
        pos = pick_u32(m, op->len - n + 1);
        memmove(op->data + pos, op->data + pos + n, op->len - pos - n);
        op->len = (uint16_t)(op->len - n);
        return 0;
    }
    case RF_MUT_CLONE_BYTES: {
        uint32_t n = 1 + pick_u32(m, RF_MIN(op->len, 64u));
        if (op->len + n > m->max_payload) return -1;
        uint32_t src = pick_u32(m, op->len - RF_MIN(n, op->len) + 1);
        uint8_t tmp[64];
        uint32_t cpy = RF_MIN(n, (uint32_t)op->len - src);
        memcpy(tmp, op->data + src, cpy);
        size_t old = op->len;
        if (resize_payload(op, old + cpy) != 0) return -1;
        pos = pick_u32(m, (uint32_t)old + 1);
        memmove(op->data + pos + cpy, op->data + pos, old - pos);
        memcpy(op->data + pos, tmp, cpy);
        return 0;
    }
    default:
        return -1;
    }
}

static int mut_insert_bytes(rf_mutator *m, rf_tc *tc)
{
    if (!tc->nops) return -1;
    rf_op *op = &tc->ops[pick_u32(m, tc->nops)];
    if (op->kind != RF_OP_WRITE && op->kind != RF_OP_DEVCTL && op->kind != RF_OP_DEVCTL_RAW &&
        op->kind != RF_OP_MSG_RAW)
        return -1;
    uint32_t n = 1 + pick_u32(m, 32);
    if (op->len + n > m->max_payload) return -1;
    size_t old = op->len;
    if (resize_payload(op, old + n) != 0) return -1;
    uint32_t pos = pick_u32(m, (uint32_t)old + 1);
    memmove(op->data + pos + n, op->data + pos, old - pos);
    uint8_t fill = (uint8_t)rf_rng_next(m->rng);
    for (uint32_t i = 0; i < n; i++)
        op->data[pos + i] = rf_rng_one_in(m->rng, 2) ? fill : (uint8_t)rf_rng_next(m->rng);
    return 0;
}

static int mut_dict_token(rf_mutator *m, rf_tc *tc)
{
    if (!m->dict || !m->dict->ntokens) return -1;
    const rf_token *t = &m->dict->tokens[pick_u32(m, m->dict->ntokens)];
    int oi = pick_payload_op(m, tc);
    if (oi < 0) return -1;
    rf_op *op = &tc->ops[oi];
    if (rf_rng_one_in(m->rng, 2) && op->len >= t->len) {      /* overwrite */
        uint32_t pos = pick_u32(m, op->len - t->len + 1);
        /* Struct fields are naturally aligned: prefer 4-byte-aligned offsets
         * for word-sized tokens (magics, dcmd constants) 3 times out of 4. */
        if (t->len >= 4 && !rf_rng_one_in(m->rng, 4))
            pos = 4 * pick_u32(m, (op->len - t->len) / 4 + 1);
        memcpy(op->data + pos, t->bytes, t->len);
        return 0;
    }
    if (op->len + t->len > m->max_payload) return -1;          /* insert */
    size_t old = op->len;
    if (resize_payload(op, old + t->len) != 0) return -1;
    uint32_t pos = pick_u32(m, (uint32_t)old + 1);
    memmove(op->data + pos + t->len, op->data + pos, old - pos);
    memcpy(op->data + pos, t->bytes, t->len);
    return 0;
}

static int mut_field(rf_mutator *m, rf_tc *tc, int which_b)
{
    if (!tc->nops) return -1;
    rf_op *op = &tc->ops[pick_u32(m, tc->nops)];
    uint32_t *f = which_b ? &op->b : &op->a;
    switch (pick_u32(m, 5)) {
    case 4:
        /* 4-byte dictionary tokens (incl. constants harvested from the
         * server's comparisons) double as candidate dcmd / length values. */
        if (m->dict && m->dict->ntokens) {
            for (int tries = 0; tries < 8; tries++) {
                const rf_token *t = &m->dict->tokens[pick_u32(m, m->dict->ntokens)];
                if (t->len == 4) { *f = rf_get_u32(t->bytes); return 0; }
            }
        }
        /* fall through */
    case 0: *f ^= 1u << pick_u32(m, 32); break;
    case 1: *f = interesting_len[pick_u32(m, RF_ARRAY_LEN(interesting_len))]; break;
    case 2: *f = (uint32_t)interesting32[pick_u32(m, RF_ARRAY_LEN(interesting32))]; break;
    default: *f += (uint32_t)((int)pick_u32(m, 33) - 16); break;
    }
    return 0;
}

static int mut_dcmd_swap(rf_mutator *m, rf_tc *tc)
{
    if (!m->dict || !m->dict->ndcmds) return -1;
    int oi = pick_kind_op(m, tc, 1);
    if (oi < 0) return -1;
    const rf_dcmd *c = &m->dict->dcmds[pick_u32(m, m->dict->ndcmds)];
    rf_op *op = &tc->ops[oi];
    op->a = c->value;
    if (rf_rng_one_in(m->rng, 2) && c->size <= m->max_payload) {
        if (resize_payload(op, c->size) != 0) return -1;
        if (op->kind == RF_OP_DEVCTL_RAW || rf_rng_one_in(m->rng, 2)) op->b = c->size;
    }
    return 0;
}

/* The bug class resfuzz exists for: the header says N bytes, the body has M. */
static int mut_claimed_len(rf_mutator *m, rf_tc *tc)
{
    int oi = pick_kind_op(m, tc, 1);
    if (oi < 0) return -1;
    rf_op *op = &tc->ops[oi];
    if (m->enable_raw && op->kind == RF_OP_DEVCTL && rf_rng_one_in(m->rng, 2))
        op->kind = RF_OP_DEVCTL_RAW;
    switch (pick_u32(m, 4)) {
    case 0: op->b = op->len + 1 + pick_u32(m, 64); break;            /* claims more than sent */
    case 1: op->b = op->len ? pick_u32(m, op->len) : 0; break;       /* claims less */
    case 2: op->b = interesting_len[pick_u32(m, RF_ARRAY_LEN(interesting_len))]; break;
    default: op->b = op->len; break;                                 /* honest */
    }
    return 0;
}

static int mut_op_insert(rf_mutator *m, rf_tc *tc)
{
    if (tc->nops >= m->max_ops) return -1;
    rf_op op;
    if (rf_gen_op(m, &op) != 0) return -1;
    int r = rf_tc_insert(tc, pick_u32(m, tc->nops + 1u), &op);
    free(op.data);
    return r < 0 ? -1 : 0;
}

static int mut_splice(rf_mutator *m, rf_tc *tc)
{
    const rf_tc *d = m->splice_with;
    if (!d || !d->nops || !tc->nops) return -1;
    /* Keep a prefix of tc, append a suffix of the donor. */
    unsigned keep = 1 + pick_u32(m, tc->nops);
    unsigned from = pick_u32(m, d->nops);
    while (tc->nops > keep) rf_tc_remove(tc, tc->nops - 1);
    for (unsigned i = from; i < d->nops && tc->nops < m->max_ops; i++)
        if (rf_tc_insert(tc, tc->nops, &d->ops[i]) < 0) break;
    return 0;
}

int rf_mutate_one(rf_mutator *m, rf_tc *tc, rf_mut_kind kind)
{
    int r = -1;
    switch (kind) {
    case RF_MUT_BITFLIP: case RF_MUT_RANDBYTE: case RF_MUT_INTERESTING: case RF_MUT_ARITH:
    case RF_MUT_DELETE_BYTES: case RF_MUT_CLONE_BYTES:
        r = mut_payload_bytes(m, tc, kind); break;
    case RF_MUT_INSERT_BYTES: r = mut_insert_bytes(m, tc); break;
    case RF_MUT_DICT_TOKEN:   r = mut_dict_token(m, tc); break;
    case RF_MUT_FIELD_A:      r = mut_field(m, tc, 0); break;
    case RF_MUT_FIELD_B:      r = mut_field(m, tc, 1); break;
    case RF_MUT_DCMD_SWAP:    r = mut_dcmd_swap(m, tc); break;
    case RF_MUT_CLAIMED_LEN:  r = mut_claimed_len(m, tc); break;
    case RF_MUT_OP_INSERT:    r = mut_op_insert(m, tc); break;
    case RF_MUT_OP_DELETE:
        if (tc->nops > 1) { rf_tc_remove(tc, pick_u32(m, tc->nops)); r = 0; }
        break;
    case RF_MUT_OP_DUP:
        if (tc->nops && tc->nops < m->max_ops) {
            unsigned i = pick_u32(m, tc->nops);
            rf_op copy = tc->ops[i];   /* rf_tc_insert deep-copies */
            r = rf_tc_insert(tc, i + 1, &copy) < 0 ? -1 : 0;
        }
        break;
    case RF_MUT_OP_SWAP:
        if (tc->nops > 1) {
            unsigned i = pick_u32(m, tc->nops), j = pick_u32(m, tc->nops);
            rf_op t = tc->ops[i]; tc->ops[i] = tc->ops[j]; tc->ops[j] = t;
            r = 0;
        }
        break;
    case RF_MUT_SPLICE: r = mut_splice(m, tc); break;
    default: break;
    }
    if (r == 0) m->stats[kind]++;
    return r;
}

void rf_mutate_havoc(rf_mutator *m, rf_tc *tc)
{
    if (tc->nops == 0) mut_op_insert(m, tc);
    unsigned stack = 1u << (1 + pick_u32(m, 4));   /* 2..16 */
    for (unsigned i = 0; i < stack; i++) {
        rf_mut_kind k;
        uint32_t r = pick_u32(m, 100);
        /* Bias towards the structure-aware mutators that matter for resmgrs. */
        if (r < 12)      k = RF_MUT_CLAIMED_LEN;
        else if (r < 22) k = RF_MUT_DCMD_SWAP;
        else if (r < 30) k = RF_MUT_DICT_TOKEN;
        else             k = (rf_mut_kind)pick_u32(m, RF_MUT__COUNT);
        if (rf_mutate_one(m, tc, k) != 0 && k != RF_MUT_RANDBYTE)
            (void)rf_mutate_one(m, tc, RF_MUT_FIELD_A);
    }
    if (tc->nops == 0) mut_op_insert(m, tc);
}
