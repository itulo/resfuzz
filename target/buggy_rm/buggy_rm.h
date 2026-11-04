/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Public devctl interface of buggy_rm, the deliberately vulnerable demo
 * resource manager (/dev/buggy). This is the kind of header resfuzz's
 * dictionary generator (host/resfuzz/dictgen.py) consumes.
 *
 * Commands are built with the QNX __DIO* macros from <devctl.h>
 * (https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.lib_ref/topic/d/devctl.html),
 * following the pattern in "Sample code for handling _IO_DEVCTL messages"
 * (https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.resmgr/topic/messages_Sample_IO_DEVCTL.html).
 *
 * DO NOT SHIP: every bug in buggy_logic.c is intentional.
 */
#ifndef BUGGY_RM_H
#define BUGGY_RM_H

#include <devctl.h>
#include <stdint.h>

#define BUGGY_PATH        "/dev/buggy"
#define BUGGY_MAGIC_ADMIN 0x4E4D4441u   /* "ADMN" when stored little-endian */
#define BUGGY_LEVEL_USER  1u
#define BUGGY_LEVEL_ADMIN 2u
#define BUGGY_FLAG_SLOW   0x00000001u   /* "precise" mode: no iteration cap */
#define BUGGY_NAME_LEN    32

struct buggy_mode    { uint32_t magic; uint32_t level; };
struct buggy_name    { char name[BUGGY_NAME_LEN]; };
struct buggy_stats   { uint32_t index; uint32_t reads; uint32_t writes; uint32_t devctls; };
struct buggy_compute { uint32_t iterations; uint32_t flags; uint32_t result; };

#define BUGGY_CMD_BASE        0x40
#define DCMD_BUGGY_SET_MODE   __DIOT (_DCMD_MISC, BUGGY_CMD_BASE + 0, struct buggy_mode)
#define DCMD_BUGGY_SET_NAME   __DIOT (_DCMD_MISC, BUGGY_CMD_BASE + 1, struct buggy_name)
#define DCMD_BUGGY_GET_NAME   __DIOF (_DCMD_MISC, BUGGY_CMD_BASE + 2, struct buggy_name)
#define DCMD_BUGGY_GET_STATS  __DIOTF(_DCMD_MISC, BUGGY_CMD_BASE + 3, struct buggy_stats)
#define DCMD_BUGGY_COMPUTE    __DIOTF(_DCMD_MISC, BUGGY_CMD_BASE + 4, struct buggy_compute)
#define DCMD_BUGGY_RESET      __DION (_DCMD_MISC, BUGGY_CMD_BASE + 5)

#endif
