/* SPDX-License-Identifier: Apache-2.0 */
/*
 * LINUX STUB ONLY. A tiny imitation of QNX message passing over AF_UNIX
 * SOCK_SEQPACKET (which, like MsgSend, preserves message boundaries), used
 * by platform_linux.c and buggy_rm/mock_rm.c so the full agent loop can run
 * on Linux. The request header deliberately mirrors the documented layout of
 * struct _io_devctl (type, combine_len, dcmd, nbytes, zero) from
 * https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.lib_ref/topic/i/iofunc_devctl.html
 * but the *type values below are our own* and are NOT QNX's _IO_* constants.
 */
#ifndef RF_STUB_WIRE_H
#define RF_STUB_WIRE_H
#include <stdint.h>

enum {
    STUB_IO_READ   = 0x0101,
    STUB_IO_WRITE  = 0x0102,
    STUB_IO_DEVCTL = 0x0103,
    STUB_IO_LSEEK  = 0x0104,
};

struct stub_req {           /* same field order/sizes as struct _io_devctl */
    uint16_t type;
    uint16_t combine_len;
    int32_t  dcmd;          /* devctl: command; lseek: whence */
    uint32_t nbytes;        /* devctl/write: payload length CLAIMED; read: requested */
    int32_t  zero;          /* lseek: offset */
};

struct stub_reply {         /* same field order/sizes as struct _io_devctl_reply */
    int32_t  status;        /* 0 or a positive errno (like MsgError) */
    int32_t  ret_val;
    uint32_t nbytes;
    int32_t  zero2;
};

#define STUB_MAX_MSG 8192

#endif
