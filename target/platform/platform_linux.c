/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Linux stub platform. The "target" string is the path of mock_rm's
 * AF_UNIX SOCK_SEQPACKET socket; "name:foo" maps to $RESFUZZ_STUB_DIR/foo.
 * Server death shows up as EPIPE/ECONNRESET/EOF and is reported as -ESRCH,
 * which is what QNX MsgSend() returns when "the server died while the
 * calling thread was SEND-blocked or REPLY-blocked" (MsgSend docs).
 */
#include "rf_platform.h"
#include "stub_wire.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

static uint64_t g_timeout_ns;

static const rf_platform_info g_info = { "linux-stub", 1, 1 };
const rf_platform_info *rf_plat_info(void) { return &g_info; }

int rf_plat_init(void) { return 0; }

uint64_t rf_plat_now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}
uint64_t rf_plat_cycles(void) { return rf_plat_now_ns(); }
uint64_t rf_plat_cycles_per_sec(void) { return 1000000000ull; }

void rf_plat_set_call_timeout_ns(uint64_t ns) { g_timeout_ns = ns; }

static int map_err(int e)
{
    if (e == EPIPE || e == ECONNRESET || e == ECONNREFUSED || e == ENOTCONN) return -ESRCH;
    if (e == EAGAIN || e == EWOULDBLOCK) return -ETIMEDOUT;
    return -e;
}

static void apply_timeout(int fd)
{
    struct timeval tv = { 0, 0 };
    if (g_timeout_ns) {
        tv.tv_sec = (time_t)(g_timeout_ns / 1000000000ull);
        tv.tv_usec = (suseconds_t)((g_timeout_ns % 1000000000ull) / 1000ull);
    }
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
}

int rf_plat_open(rf_conn *c, const char *target, int access_mode)
{
    (void)access_mode;
    struct sockaddr_un sa;
    memset(&sa, 0, sizeof sa);
    sa.sun_family = AF_UNIX;
    if (strncmp(target, "name:", 5) == 0) {
        const char *dir = getenv("RESFUZZ_STUB_DIR");
        if (snprintf(sa.sun_path, sizeof sa.sun_path, "%s/%s", dir ? dir : "/tmp", target + 5) >=
            (int)sizeof sa.sun_path)
            return -ENAMETOOLONG;
        c->is_name = 1;
    } else {
        if (strlen(target) >= sizeof sa.sun_path) return -ENAMETOOLONG;
        strcpy(sa.sun_path, target);
        c->is_name = 0;
    }
    int fd = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
    if (fd < 0) return -errno;
    if (connect(fd, (struct sockaddr *)&sa, sizeof sa) != 0) {
        int e = errno;
        close(fd);
        return (e == ECONNREFUSED || e == ENOENT) ? -ENOENT : -e;
    }
    c->fd = fd;
    c->open = 1;
    return 0;
}

int rf_plat_close(rf_conn *c)
{
    if (!c->open) return -EBADF;
    close(c->fd);
    c->open = 0;
    c->fd = -1;
    return 0;
}

/* One request/reply round trip. Returns reply.ret_val-or-nbytes semantics
 * decided by caller; here: 0 on transport success, -errno otherwise. */
static int xact(rf_conn *c, const void *msg, size_t mlen, struct stub_reply *rep, void *rdata, size_t rcap,
                size_t *rlen)
{
    if (!c->open) return -EBADF;
    apply_timeout(c->fd);
    ssize_t w = send(c->fd, msg, mlen, MSG_NOSIGNAL);
    if (w < 0) return map_err(errno);
    uint8_t buf[STUB_MAX_MSG + sizeof(struct stub_reply)];
    ssize_t r = recv(c->fd, buf, sizeof buf, 0);
    if (r < 0) return map_err(errno);
    if (r == 0) return -ESRCH;                    /* peer closed: server gone */
    if ((size_t)r < sizeof *rep) return -EBADMSG;
    memcpy(rep, buf, sizeof *rep);
    size_t dl = (size_t)r - sizeof *rep;
    if (dl > rcap) dl = rcap;
    if (rdata && dl) memcpy(rdata, buf + sizeof *rep, dl);
    if (rlen) *rlen = dl;
    return 0;
}

static long simple(rf_conn *c, uint16_t type, int32_t dcmd, uint32_t nbytes, int32_t zero,
                   const void *data, size_t len, void *rdata, size_t rcap, size_t *rlen, int *ret_val)
{
    uint8_t msg[sizeof(struct stub_req) + STUB_MAX_MSG];
    if (len > STUB_MAX_MSG) return -E2BIG;
    struct stub_req h = { type, (uint16_t)sizeof h, dcmd, nbytes, zero };
    memcpy(msg, &h, sizeof h);
    if (len) memcpy(msg + sizeof h, data, len);
    struct stub_reply rep;
    int rc = xact(c, msg, sizeof h + len, &rep, rdata, rcap, rlen);
    if (rc < 0) return rc;
    if (rep.status) return -rep.status;
    if (ret_val) *ret_val = rep.ret_val;
    return (long)rep.ret_val;
}

long rf_plat_read(rf_conn *c, void *buf, size_t n)
{
    size_t got = 0;
    long r = simple(c, STUB_IO_READ, 0, (uint32_t)n, 0, NULL, 0, buf, n, &got, NULL);
    return r < 0 ? r : (long)got;
}

long rf_plat_write(rf_conn *c, const void *buf, size_t n)
{
    return simple(c, STUB_IO_WRITE, 0, (uint32_t)n, 0, buf, n, NULL, 0, NULL, NULL);
}

long rf_plat_lseek(rf_conn *c, int64_t off, int whence)
{
    return simple(c, STUB_IO_LSEEK, whence, 0, (int32_t)off, NULL, 0, NULL, 0, NULL, NULL);
}

int rf_plat_devctl(rf_conn *c, int dcmd, void *data, size_t n, int *info)
{
    long r = simple(c, STUB_IO_DEVCTL, dcmd, (uint32_t)n, 0, data, n, data, n, NULL, info);
    return r < 0 ? (int)r : 0;
}

long rf_plat_devctl_raw(rf_conn *c, int dcmd, uint32_t claimed, const void *data, size_t len, void *reply,
                        size_t rlen)
{
    return simple(c, STUB_IO_DEVCTL, dcmd, claimed, 0, data, len, reply, rlen, NULL, NULL);
}

long rf_plat_msgsend_raw(rf_conn *c, const void *smsg, size_t slen, void *reply, size_t rlen)
{
    struct stub_reply rep;
    if (slen == 0) return -EINVAL;
    int rc = xact(c, smsg, slen, &rep, reply, rlen, NULL);
    if (rc < 0) return rc;
    return rep.status ? -rep.status : rep.ret_val;
}
