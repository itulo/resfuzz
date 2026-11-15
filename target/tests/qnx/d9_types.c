/* SPDX-License-Identifier: Apache-2.0 */
/*
 * TODO-QNX D9: does the kernel or libc stop any message type before it
 * reaches a server? Sends every 16-bit type with MsgSend() to name_srv (a
 * plain name_attach() channel, no resmgr layer), which echoes the message
 * (or MsgError(ENOSYS) for _IO_* types, EOK for _IO_CONNECT). Any other
 * outcome means the message was rejected on the way.
 *   qcc -Vgcc_ntox86_64 -g -Wall -o build-qnx/d9_types tests/qnx/d9_types.c
 */
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/dispatch.h>
#include <sys/iomsg.h>
#include <sys/neutrino.h>

int main(int argc, char **argv)
{
    const char *name = argc > 1 ? argv[1] : "resfuzz_echo";
    int coid = name_open(name, 0);
    if (coid < 0) { perror("name_open"); return 1; }
    unsigned echoed = 0, enosys = 0, connect_ok = 0, other = 0;
    for (unsigned t = 0; t <= 0xffff; t++) {
        uint8_t m[16] = {0}, r[16];
        uint16_t ty = (uint16_t)t;
        memcpy(m, &ty, 2);
        memcpy(m + 2, "probe!", 6);
        long st = MsgSend(coid, m, sizeof m, r, sizeof r);
        if (st == (long)sizeof m && memcmp(r, m, sizeof m) == 0) echoed++;
        else if (st == -1 && errno == ENOSYS && t > _IO_BASE && t <= _IO_MAX) enosys++;
        else if (st == 0 && t == _IO_CONNECT) connect_ok++;
        else { other++; if (other <= 20) printf("type 0x%04x: st=%ld errno=%d %s\n", t, st, errno, strerror(errno)); }
    }
    printf("types=65536 echoed=%u enosys_from_server=%u io_connect_ok=%u other=%u\n", echoed, enosys, connect_ok, other);
    name_close(coid);
    return other ? 2 : 0;
}
