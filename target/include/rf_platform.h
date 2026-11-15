/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Platform layer: the ONLY place that knows about QNX.
 *   platform/platform_qnx.c   - QNX 8.0 (open/name_open, devctl, MsgSend,
 *                               TimerTimeout, ClockCycles). Needs the SDP.
 *   platform/platform_linux.c - Linux stub: talks to mock_rm over an
 *                               AF_UNIX SOCK_SEQPACKET socket so the whole
 *                               agent can be built and tested with gcc.
 * All functions return >= 0 on success and -errno on failure (never set errno
 * as the result channel), so callers can classify ESRCH/ETIMEDOUT uniformly.
 */
#ifndef RF_PLATFORM_H
#define RF_PLATFORM_H
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

typedef struct {
    int fd;          /* QNX: fd or coid (fd is a coid on QNX); Linux: socket */
    int is_name;     /* opened with name_open() */
    int open;
} rf_conn;

typedef struct {
    const char *name;            /* "qnx" or "linux-stub" */
    int         has_raw_msgsend; /* MsgSend with crafted headers */
    int         kernel_timeouts; /* per-call kernel timeouts available */
} rf_platform_info;

const rf_platform_info *rf_plat_info(void);
int      rf_plat_init(void);

/* Timing: free-running cycle counter + its frequency. */
uint64_t rf_plat_cycles(void);
uint64_t rf_plat_cycles_per_sec(void);
uint64_t rf_plat_now_ns(void);

/* Per-call timeout for the next blocking call (0 = none). */
void     rf_plat_set_call_timeout_ns(uint64_t ns);

/* Open targets with O_NONBLOCK, so reads on character devices (ptys, serial
 * ports) with no data return EAGAIN instead of looking like a hang. */
void     rf_plat_set_nonblock(int on);

/* target: "/dev/foo" (path) or "name:foo" (name_attach'ed server). */
int      rf_plat_open(rf_conn *c, const char *target, int access_mode /*0 r,1 w,2 rw*/);
int      rf_plat_close(rf_conn *c);
long     rf_plat_read(rf_conn *c, void *buf, size_t n);
long     rf_plat_write(rf_conn *c, const void *buf, size_t n);
long     rf_plat_lseek(rf_conn *c, int64_t off, int whence);
/* Returns the devctl() status (0 = EOK) as -errno; *info gets dev_info. */
int      rf_plat_devctl(rf_conn *c, int dcmd, void *data, size_t n, int *info);
/* Hand-built _IO_DEVCTL header with a possibly lying nbytes field. */
long     rf_plat_devctl_raw(rf_conn *c, int dcmd, uint32_t claimed_nbytes,
                            const void *data, size_t len, void *reply, size_t rlen);
long     rf_plat_msgsend_raw(rf_conn *c, const void *smsg, size_t slen, void *reply, size_t rlen);

/* Process control (POSIX on both). env_extra: NULL-terminated "K=V" list. */
pid_t    rf_plat_spawn(char *const argv[], char *const env_extra[]);
/* 0 = running, 1 = exited (status in *st), -1 = error. */
int      rf_plat_poll_child(pid_t pid, int *st);
void     rf_plat_kill(pid_t pid);

#endif
