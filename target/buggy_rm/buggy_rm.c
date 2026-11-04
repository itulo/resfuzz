/* SPDX-License-Identifier: Apache-2.0 */
/*
 * buggy_rm - a deliberately vulnerable QNX 8.0 resource manager at /dev/buggy.
 * NOT YET COMPILED (SDP not installed). Structure follows the QNX 8.0
 * "Writing a Resource Manager" samples:
 *  - main loop / attach: "Sample code for handling _IO_READ messages"
 *    https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.resmgr/topic/read_write_Sample_IO_READ.html
 *  - io_write:  https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.resmgr/topic/read_write_Sample_IO_WRITE.html
 *  - io_devctl: https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.resmgr/topic/messages_Sample_IO_DEVCTL.html
 * The planted bugs live in buggy_logic.c (shared with the Linux mock).
 *
 * Build (UNVERIFIED until the SDP is installed):
 *   qcc -Vgcc_ntox86_64 -g -O1 -fsanitize-coverage=trace-pc,trace-cmp -I../include \
 *       -c buggy_rm.c buggy_logic.c
 *   qcc -Vgcc_ntox86_64 -g -O1 -I../include -c ../covrt/cov_rt.c
 *   qcc -Vgcc_ntox86_64 -o buggy_rm buggy_rm.o buggy_logic.o cov_rt.o
 */
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/dispatch.h>
#include <sys/iofunc.h>

#include "buggy_logic.h"
#include "buggy_rm.h"

static resmgr_connect_funcs_t connect_funcs;
static resmgr_io_funcs_t      io_funcs;
static iofunc_attr_t          attr;
static char                   status_buf[128];

static int io_read(resmgr_context_t *ctp, io_read_t *msg, RESMGR_OCB_T *ocb)
{
    int status;
    if ((status = iofunc_read_verify(ctp, msg, ocb, NULL)) != EOK) return status;
    if ((msg->i.xtype & _IO_XTYPE_MASK) != _IO_XTYPE_NONE) return ENOSYS;

    if (ocb->offset == 0) attr.nbytes = buggy_status_text(status_buf, sizeof status_buf);
    size_t nleft = ocb->attr->nbytes > ocb->offset ? ocb->attr->nbytes - ocb->offset : 0;
    size_t nbytes = min(_IO_READ_GET_NBYTES(msg), nleft);
    if (nbytes > 0) {
        SETIOV(ctp->iov, status_buf + ocb->offset, nbytes);
        _IO_SET_READ_NBYTES(ctp, nbytes);
        ocb->offset += nbytes;
        return _RESMGR_NPARTS(1);
    }
    _IO_SET_READ_NBYTES(ctp, 0);
    return _RESMGR_NPARTS(0);
}

static int io_write(resmgr_context_t *ctp, io_write_t *msg, RESMGR_OCB_T *ocb)
{
    int status;
    uint8_t buf[64];
    if ((status = iofunc_write_verify(ctp, msg, ocb, NULL)) != EOK) return status;
    if ((msg->i.xtype & _IO_XTYPE_MASK) != _IO_XTYPE_NONE) return ENOSYS;

    size_t nbytes = _IO_WRITE_GET_NBYTES(msg);
    /* The docs' filter against writes that claim more than they carry. */
    if (nbytes > (size_t)ctp->info.srcmsglen - (size_t)ctp->offset - sizeof(io_write_t)) return EBADMSG;
    _IO_SET_WRITE_NBYTES(ctp, nbytes);
    size_t n = nbytes < sizeof buf ? nbytes : sizeof buf;
    resmgr_msgget(ctp, buf, n, sizeof(msg->i));
    buggy_write(buf, n);
    return _RESMGR_NPARTS(0);
}

static int io_devctl(resmgr_context_t *ctp, io_devctl_t *msg, RESMGR_OCB_T *ocb)
{
    static uint8_t in[2048], out[2048];
    int status;
    if ((status = iofunc_devctl_default(ctp, msg, ocb)) != _RESMGR_DEFAULT) return status;

    /* Bytes that really arrived after the header (same arithmetic as the
     * io_write sample). UNVERIFIED for devctl until tested on target. */
    size_t hdr = sizeof(msg->i);
    size_t avail = (size_t)ctp->info.srcmsglen > (size_t)ctp->offset + hdr
                 ? (size_t)ctp->info.srcmsglen - (size_t)ctp->offset - hdr : 0;
    size_t claimed = msg->i.nbytes;          /* what the client SAYS it sent */
    if (avail > sizeof in) avail = sizeof in;
    /* Copy out of the receive buffer: the reply reuses the same memory. */
    memcpy(in, _IO_INPUT_PAYLOAD(msg), avail <= ctp->msg_max_size - hdr ? avail : ctp->msg_max_size - hdr);

    size_t outlen = 0;
    size_t outcap = ctp->msg_max_size - sizeof(msg->o);
    if (outcap > sizeof out) outcap = sizeof out;
    /* NOTE: `in` is passed with `claimed`: buggy_logic's BUG 1 reads up to
     * `claimed` bytes from it, i.e. beyond what arrived. */
    int err = buggy_devctl(msg->i.dcmd, in, claimed, avail, out, outcap, &outlen);
    if (err) return err;

    memset(&msg->o, 0, sizeof(msg->o));
    msg->o.ret_val = 0;
    msg->o.nbytes = outlen;
    if (outlen) memcpy(_IO_OUTPUT_PAYLOAD(msg), out, outlen);
    return _RESMGR_PTR(ctp, &msg->o, sizeof(msg->o) + outlen);
}

int main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : BUGGY_PATH;
    resmgr_attr_t resmgr_attr;
    dispatch_t *dpp;
    dispatch_context_t *ctp;

    buggy_init();
    if ((dpp = dispatch_create()) == NULL) {
        fprintf(stderr, "%s: Unable to allocate dispatch handle.\n", argv[0]);
        return EXIT_FAILURE;
    }
    memset(&resmgr_attr, 0, sizeof resmgr_attr);
    resmgr_attr.nparts_max = 1;
    resmgr_attr.msg_max_size = 2048;

    iofunc_func_init(_RESMGR_CONNECT_NFUNCS, &connect_funcs, _RESMGR_IO_NFUNCS, &io_funcs);
    io_funcs.read = io_read;
    io_funcs.read64 = io_read;   /* as in the QNX 8.0 read sample */
    io_funcs.write = io_write;
    io_funcs.devctl = io_devctl;

    iofunc_attr_init(&attr, S_IFNAM | 0666, 0, 0);
    if (resmgr_attach(dpp, &resmgr_attr, path, _FTYPE_ANY, 0, &connect_funcs, &io_funcs, &attr) == -1) {
        fprintf(stderr, "%s: Unable to attach name %s.\n", argv[0], path);
        return EXIT_FAILURE;
    }
    ctp = dispatch_context_alloc(dpp);
    printf("buggy_rm: ready on %s\n", path);
    fflush(stdout);
    for (;;) {
        if ((ctp = dispatch_block(ctp)) == NULL) {
            fprintf(stderr, "block error\n");
            return EXIT_FAILURE;
        }
        dispatch_handler(ctp);
    }
}
