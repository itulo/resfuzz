/* SPDX-License-Identifier: Apache-2.0 */
/*
 * TODO-QNX D5: a minimal name_attach() server for fuzzing a "name:" target.
 * Not a resource manager: it receives raw messages on the channel that
 * name_attach() creates (lib_ref/topic/n/name_attach.html), answers the
 * _IO_CONNECT that name_open() sends, handles the disconnect pulse, echoes
 * everything else, and has one planted bug: a message whose first u32 after
 * the 16-bit type is 0xDEADC0DE dereferences NULL.
 *   qcc -Vgcc_ntox86_64 -g -Wall -o build-qnx/name_srv tests/qnx/name_srv.c
 *   resfuzz-agent --target name:resfuzz_echo --spawn /data/resfuzz/name_srv ...
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
    name_attach_t *att = name_attach(NULL, name, 0);
    if (!att) { perror("name_attach"); return 1; }
    printf("name_srv: attached %s\n", name);
    fflush(stdout);
    static union { struct _pulse pulse; uint16_t type; uint8_t raw[4096]; } msg;
    for (;;) {
        struct _msg_info info;
        int rcvid = MsgReceive(att->chid, &msg, sizeof msg, &info);
        if (rcvid == -1) continue;
        if (rcvid == 0) {                       /* pulse */
            if (msg.pulse.code == _PULSE_CODE_DISCONNECT) ConnectDetach(msg.pulse.scoid);
            continue;
        }
        if (msg.type == _IO_CONNECT) { MsgReply(rcvid, EOK, NULL, 0); continue; }  /* from name_open() */
        if (msg.type > _IO_BASE && msg.type <= _IO_MAX) { MsgError(rcvid, ENOSYS); continue; }
        size_t n = info.msglen < sizeof msg ? info.msglen : sizeof msg;
        uint32_t tag = 0;
        if (n >= 6) memcpy(&tag, msg.raw + 2, 4);
        if (tag == 0xDEADC0DEu) { volatile int *p = NULL; *p = 1; }   /* planted bug */
        MsgReply(rcvid, (long)n, msg.raw, n);
    }
}
