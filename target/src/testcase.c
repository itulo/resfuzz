/* SPDX-License-Identifier: Apache-2.0 */
#include "rf_testcase.h"
#include "rf_util.h"

#include <stdlib.h>
#include <string.h>

static const char *const kind_names[RF_OP__COUNT] = {
    "OPEN", "CLOSE", "READ", "WRITE", "DEVCTL", "DEVCTL_RAW", "LSEEK", "MSG_RAW", "SLEEP",
};

const char *rf_op_kind_name(uint8_t kind)
{
    return kind < RF_OP__COUNT ? kind_names[kind] : "?";
}

void rf_tc_init(rf_tc *tc) { memset(tc, 0, sizeof *tc); }

void rf_tc_free(rf_tc *tc)
{
    for (unsigned i = 0; i < tc->nops; i++) free(tc->ops[i].data);
    memset(tc, 0, sizeof *tc);
}

int rf_op_set_payload(rf_op *op, const void *data, size_t len)
{
    if (len > RF_MAX_PAYLOAD) return -1;
    uint8_t *p = NULL;
    if (len) {
        p = malloc(len);
        if (!p) return -1;
        if (data) memcpy(p, data, len); else memset(p, 0, len);
    }
    free(op->data);
    op->data = p;
    op->len = (uint16_t)len;
    return 0;
}

static int op_copy(rf_op *dst, const rf_op *src)
{
    *dst = *src;
    dst->data = NULL;
    dst->len = 0;
    return rf_op_set_payload(dst, src->data, src->len);
}

int rf_tc_copy(rf_tc *dst, const rf_tc *src)
{
    rf_tc_init(dst);
    for (unsigned i = 0; i < src->nops; i++) {
        if (op_copy(&dst->ops[i], &src->ops[i]) != 0) { rf_tc_free(dst); return -1; }
        dst->nops++;
    }
    return 0;
}

int rf_tc_append(rf_tc *tc, uint8_t kind, uint8_t flags, uint32_t a, uint32_t b,
                 const void *data, size_t len)
{
    if (tc->nops >= RF_MAX_OPS || kind >= RF_OP__COUNT) return -1;
    rf_op *op = &tc->ops[tc->nops];
    memset(op, 0, sizeof *op);
    op->kind = kind; op->flags = flags; op->a = a; op->b = b;
    if (rf_op_set_payload(op, data, len) != 0) return -1;
    return tc->nops++;
}

int rf_tc_insert(rf_tc *tc, unsigned idx, const rf_op *op)
{
    if (tc->nops >= RF_MAX_OPS || idx > tc->nops) return -1;
    rf_op tmp;
    memset(&tmp, 0, sizeof tmp);
    if (op_copy(&tmp, op) != 0) return -1;
    memmove(&tc->ops[idx + 1], &tc->ops[idx], (tc->nops - idx) * sizeof(rf_op));
    tc->ops[idx] = tmp;
    tc->nops++;
    return (int)idx;
}

void rf_tc_remove(rf_tc *tc, unsigned idx)
{
    if (idx >= tc->nops) return;
    free(tc->ops[idx].data);
    memmove(&tc->ops[idx], &tc->ops[idx + 1], (tc->nops - idx - 1) * sizeof(rf_op));
    tc->nops--;
    memset(&tc->ops[tc->nops], 0, sizeof(rf_op));
}

size_t rf_tc_serialized_size(const rf_tc *tc)
{
    size_t n = RF_TC_HDR_LEN;
    for (unsigned i = 0; i < tc->nops; i++) n += RF_OP_HDR_LEN + tc->ops[i].len;
    return n;
}

size_t rf_tc_serialize(const rf_tc *tc, uint8_t *buf, size_t cap)
{
    size_t need = rf_tc_serialized_size(tc);
    if (need > cap) return 0;
    memcpy(buf, RF_TC_MAGIC, 4);
    rf_put_u16(buf + 4, tc->nops);
    rf_put_u16(buf + 6, 0);
    size_t off = RF_TC_HDR_LEN;
    for (unsigned i = 0; i < tc->nops; i++) {
        const rf_op *op = &tc->ops[i];
        buf[off] = op->kind;
        buf[off + 1] = op->flags;
        rf_put_u16(buf + off + 2, op->len);
        rf_put_u32(buf + off + 4, op->a);
        rf_put_u32(buf + off + 8, op->b);
        off += RF_OP_HDR_LEN;
        if (op->len) memcpy(buf + off, op->data, op->len);
        off += op->len;
    }
    return off;
}

int rf_tc_parse(rf_tc *tc, const uint8_t *buf, size_t len)
{
    rf_tc_init(tc);
    if (len < RF_TC_HDR_LEN || memcmp(buf, RF_TC_MAGIC, 4) != 0) return -1;
    unsigned nops = rf_get_u16(buf + 4);
    if (nops > RF_MAX_OPS || rf_get_u16(buf + 6) != 0) return -1;   /* reserved must be 0 */
    size_t off = RF_TC_HDR_LEN;
    for (unsigned i = 0; i < nops; i++) {
        if (len - off < RF_OP_HDR_LEN) goto bad;
        uint8_t kind = buf[off], flags = buf[off + 1];
        uint16_t plen = rf_get_u16(buf + off + 2);
        uint32_t a = rf_get_u32(buf + off + 4), b = rf_get_u32(buf + off + 8);
        off += RF_OP_HDR_LEN;
        if (kind >= RF_OP__COUNT || plen > RF_MAX_PAYLOAD || len - off < plen) goto bad;
        if (rf_tc_append(tc, kind, flags, a, b, buf + off, plen) < 0) goto bad;
        off += plen;
    }
    if (off != len) goto bad;   /* trailing garbage = corrupt file */
    return 0;
bad:
    rf_tc_free(tc);
    return -1;
}

uint64_t rf_tc_hash(const rf_tc *tc)
{
    uint8_t hdr[RF_OP_HDR_LEN];
    uint64_t h = rf_fnv1a64(&tc->nops, 0, tc->nops);
    for (unsigned i = 0; i < tc->nops; i++) {
        const rf_op *op = &tc->ops[i];
        hdr[0] = op->kind; hdr[1] = op->flags;
        rf_put_u16(hdr + 2, op->len); rf_put_u32(hdr + 4, op->a); rf_put_u32(hdr + 8, op->b);
        h = rf_fnv1a64(hdr, sizeof hdr, h);
        if (op->len) h = rf_fnv1a64(op->data, op->len, h);
    }
    return h;
}

void rf_tc_dump(const rf_tc *tc, FILE *out)
{
    for (unsigned i = 0; i < tc->nops; i++) {
        const rf_op *op = &tc->ops[i];
        fprintf(out, "  #%-2u slot%u %-10s a=0x%08x b=0x%08x len=%u", i, op->flags & 3u,
                rf_op_kind_name(op->kind), op->a, op->b, op->len);
        if (op->len) {
            fputs(" data=", out);
            for (unsigned j = 0; j < op->len && j < 32; j++) fprintf(out, "%02x", op->data[j]);
            if (op->len > 32) fputs("...", out);
        }
        fputc('\n', out);
    }
}

int rf_tc_save(const rf_tc *tc, const char *path)
{
    size_t n = rf_tc_serialized_size(tc);
    uint8_t *buf = malloc(n);
    if (!buf) return -1;
    int rc = rf_tc_serialize(tc, buf, n) == n ? rf_write_file(path, buf, n) : -1;
    free(buf);
    return rc;
}

int rf_tc_load(rf_tc *tc, const char *path)
{
    uint8_t *buf;
    size_t n;
    rf_tc_init(tc);
    if (rf_read_file(path, &buf, &n, RF_TC_HDR_LEN + RF_MAX_OPS * (RF_OP_HDR_LEN + RF_MAX_PAYLOAD)) != 0)
        return -1;
    int rc = rf_tc_parse(tc, buf, n);
    free(buf);
    return rc;
}
