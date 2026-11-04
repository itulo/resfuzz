/* SPDX-License-Identifier: Apache-2.0 */
/*
 * resfuzz test case = a short *sequence* of operations against one resource
 * manager. Each op maps to one client call that the QNX C library turns into
 * an io_* message (open -> _IO_CONNECT, read -> _IO_READ, write -> _IO_WRITE,
 * devctl -> _IO_DEVCTL, lseek -> _IO_LSEEK), or to a raw MsgSend() whose
 * header we craft ourselves (so header fields such as nbytes can lie).
 *
 * The on-disk format ("RFZ1") is little-endian and documented in
 * docs/DESIGN.md; host/resfuzz/testcase.py implements the same codec.
 * The parser treats its input as hostile (corpus files, crash files).
 */
#ifndef RF_TESTCASE_H
#define RF_TESTCASE_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define RF_TC_MAGIC        "RFZ1"
#define RF_TC_HDR_LEN      8u   /* magic[4] u16 nops u16 reserved */
#define RF_OP_HDR_LEN      12u  /* u8 kind u8 flags u16 len u32 a u32 b */
#define RF_MAX_OPS         64u
#define RF_MAX_PAYLOAD     4096u
#define RF_MAX_SLOTS       4u    /* concurrent open connections per test case */
#define RF_MAX_READ        65536u
#define RF_MAX_SLEEP_US    10000u

typedef enum {
    RF_OP_OPEN = 0,     /* open(path) / name_open(name) into slot (flags & 3); a = access mode 0..2 */
    RF_OP_CLOSE,        /* close slot */
    RF_OP_READ,         /* read(a bytes) */
    RF_OP_WRITE,        /* write(payload) */
    RF_OP_DEVCTL,       /* devctl(dcmd = a, payload, max(len, b) bytes buffer) */
    RF_OP_DEVCTL_RAW,   /* MsgSend of a hand-built _IO_DEVCTL header: dcmd = a, header nbytes = b, payload follows */
    RF_OP_LSEEK,        /* lseek(offset = (int32)a, whence = b % 3) */
    RF_OP_MSG_RAW,      /* MsgSend(payload) verbatim, the first u16 is the message type */
    RF_OP_SLEEP,        /* usleep(min(a, RF_MAX_SLEEP_US)), to perturb timing */
    RF_OP__COUNT
} rf_op_kind;

typedef struct {
    uint8_t  kind;
    uint8_t  flags;   /* bits 0..1: connection slot */
    uint16_t len;     /* payload length */
    uint32_t a;
    uint32_t b;
    uint8_t *data;    /* malloc'd, len bytes (NULL when len == 0) */
} rf_op;

typedef struct {
    uint16_t nops;
    rf_op    ops[RF_MAX_OPS];
} rf_tc;

void   rf_tc_init(rf_tc *tc);
void   rf_tc_free(rf_tc *tc);
int    rf_tc_copy(rf_tc *dst, const rf_tc *src);      /* dst must be initialised/freed */
/* Append an op (payload copied). Returns index or -1. */
int    rf_tc_append(rf_tc *tc, uint8_t kind, uint8_t flags, uint32_t a, uint32_t b,
                    const void *data, size_t len);
int    rf_tc_insert(rf_tc *tc, unsigned idx, const rf_op *op); /* deep copy */
void   rf_tc_remove(rf_tc *tc, unsigned idx);
int    rf_op_set_payload(rf_op *op, const void *data, size_t len);

size_t rf_tc_serialized_size(const rf_tc *tc);
/* Returns bytes written, or 0 if cap is too small. */
size_t rf_tc_serialize(const rf_tc *tc, uint8_t *buf, size_t cap);
/* 0 on success, -1 on malformed input (tc left empty). */
int    rf_tc_parse(rf_tc *tc, const uint8_t *buf, size_t len);
uint64_t rf_tc_hash(const rf_tc *tc);

const char *rf_op_kind_name(uint8_t kind);
/* Human-readable one-line-per-op dump (for crash reports). */
void   rf_tc_dump(const rf_tc *tc, FILE *out);

int    rf_tc_save(const rf_tc *tc, const char *path);
int    rf_tc_load(rf_tc *tc, const char *path);

#endif
