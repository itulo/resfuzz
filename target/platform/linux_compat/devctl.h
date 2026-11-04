/* SPDX-License-Identifier: Apache-2.0 */
/*
 * LINUX STUB ONLY - NOT QNX's <devctl.h>.
 * Lets buggy_rm.h (which uses the QNX __DIOT/__DIOF/__DIOTF/__DION macros
 * documented at
 * https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.lib_ref/topic/d/devctl.html)
 * compile on Linux for the mock server and unit tests.
 * The bit encoding below is OUR OWN and intentionally not claimed to match
 * QNX's; on QNX the real header is used and dcmd values are resolved on the
 * target by tools/dcmd_resolver (see docs/DESIGN.md).
 */
#ifndef RF_LINUX_COMPAT_DEVCTL_H
#define RF_LINUX_COMPAT_DEVCTL_H
#include <stdint.h>

#define _DCMD_MISC 0x0d   /* stub value */
#define RF_STUB_DIR_NONE   0u
#define RF_STUB_DIR_TO     1u
#define RF_STUB_DIR_FROM   2u
#define RF_STUB_DIR_TOFROM 3u
#define RF_STUB_DCMD(dir, class, cmd, size) \
    ((int)(((uint32_t)(dir) << 30) | (((uint32_t)(size) & 0x3fffu) << 16) | \
           (((uint32_t)(class) & 0xffu) << 8) | ((uint32_t)(cmd) & 0xffu)))
#define __DION(class, cmd)        RF_STUB_DCMD(RF_STUB_DIR_NONE, class, cmd, 0)
#define __DIOT(class, cmd, data)  RF_STUB_DCMD(RF_STUB_DIR_TO, class, cmd, sizeof(data))
#define __DIOF(class, cmd, data)  RF_STUB_DCMD(RF_STUB_DIR_FROM, class, cmd, sizeof(data))
#define __DIOTF(class, cmd, data) RF_STUB_DCMD(RF_STUB_DIR_TOFROM, class, cmd, sizeof(data))
#define get_device_command(cmd)   ((uint32_t)(cmd) & 0xffffu)
#define get_device_size(cmd)      (((uint32_t)(cmd) >> 16) & 0x3fffu)
#define get_device_direction(cmd) ((uint32_t)(cmd) >> 30)
#define DEVDIR_NONE   RF_STUB_DIR_NONE
#define DEVDIR_TO     RF_STUB_DIR_TO
#define DEVDIR_FROM   RF_STUB_DIR_FROM
#define DEVDIR_TOFROM RF_STUB_DIR_TOFROM

#endif
