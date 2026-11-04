/* SPDX-License-Identifier: Apache-2.0 */
/* Platform-independent request handling shared by buggy_rm (QNX) and mock_rm (Linux). */
#ifndef BUGGY_LOGIC_H
#define BUGGY_LOGIC_H
#include <stddef.h>
#include <stdint.h>

void   buggy_init(void);
/* devctl: in = payload as received; claimed = header nbytes; avail = bytes
 * actually present. Writes reply payload to out (cap outcap), *outlen.
 * Returns 0 or a positive errno. */
int    buggy_devctl(int dcmd, const uint8_t *in, size_t claimed, size_t avail,
                    uint8_t *out, size_t outcap, size_t *outlen);
size_t buggy_status_text(char *buf, size_t cap);     /* for read() */
int    buggy_write(const uint8_t *data, size_t len); /* for write() */

#endif
