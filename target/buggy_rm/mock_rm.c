/* SPDX-License-Identifier: Apache-2.0 */
/*
 * mock_rm - LINUX STUB of buggy_rm. Serves the same buggy_logic.c over an
 * AF_UNIX SOCK_SEQPACKET socket using platform/stub_wire.h, single-threaded
 * like the QNX sample (one request at a time => a slow request delays all
 * clients, exactly the property the deadline oracle measures).
 *   usage: mock_rm SOCKET_PATH
 */
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "buggy_logic.h"
#include "stub_wire.h"

#define MAX_CLIENTS 16

struct client { int fd; size_t offset; };

static void reply(int fd, int status, int ret_val, const void *data, size_t len)
{
    uint8_t buf[sizeof(struct stub_reply) + STUB_MAX_MSG];
    struct stub_reply r = { status, ret_val, (uint32_t)len, 0 };
    if (len > STUB_MAX_MSG) len = STUB_MAX_MSG;
    memcpy(buf, &r, sizeof r);
    if (len) memcpy(buf + sizeof r, data, len);
    send(fd, buf, sizeof r + len, MSG_NOSIGNAL);
}

static void handle(struct client *cl, const uint8_t *msg, size_t len)
{
    static uint8_t in[STUB_MAX_MSG], out[STUB_MAX_MSG];
    struct stub_req h;
    if (len < sizeof h) { reply(cl->fd, EBADMSG, -1, NULL, 0); return; }
    memcpy(&h, msg, sizeof h);
    size_t avail = len - sizeof h;
    memset(in, 0, sizeof in);           /* stale bytes beyond avail read as zero */
    memcpy(in, msg + sizeof h, avail);
    switch (h.type) {
    case STUB_IO_READ: {
        char text[128];
        size_t n = buggy_status_text(text, sizeof text);
        size_t left = cl->offset < n ? n - cl->offset : 0;
        size_t give = h.nbytes < left ? h.nbytes : left;
        reply(cl->fd, 0, (int)give, text + cl->offset, give);
        cl->offset += give;
        break;
    }
    case STUB_IO_WRITE:
        /* Same filter as the QNX io_write sample. */
        if (h.nbytes > avail) { reply(cl->fd, EBADMSG, -1, NULL, 0); break; }
        buggy_write(in, h.nbytes);
        reply(cl->fd, 0, (int)h.nbytes, NULL, 0);
        break;
    case STUB_IO_LSEEK:
        if (h.zero < 0 || h.dcmd > 2) { reply(cl->fd, EINVAL, -1, NULL, 0); break; }
        cl->offset = (size_t)h.zero;
        reply(cl->fd, 0, h.zero, NULL, 0);
        break;
    case STUB_IO_DEVCTL: {
        size_t outlen = 0;
        int err = buggy_devctl(h.dcmd, in, h.nbytes, avail, out, sizeof out, &outlen);
        if (err) reply(cl->fd, err, -1, NULL, 0);
        else reply(cl->fd, 0, 0, out, outlen);
        break;
    }
    default:
        reply(cl->fd, ENOSYS, -1, NULL, 0);
    }
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: %s SOCKET_PATH\n", argv[0]); return 2; }
    signal(SIGPIPE, SIG_IGN);
    buggy_init();
    int ls = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    struct sockaddr_un sa;
    memset(&sa, 0, sizeof sa);
    sa.sun_family = AF_UNIX;
    if (strlen(argv[1]) >= sizeof sa.sun_path) { fprintf(stderr, "path too long\n"); return 2; }
    strcpy(sa.sun_path, argv[1]);
    unlink(argv[1]);
    if (ls < 0 || bind(ls, (struct sockaddr *)&sa, sizeof sa) != 0 || listen(ls, 16) != 0) {
        perror("mock_rm: bind/listen");
        return 1;
    }
    printf("mock_rm: ready on %s\n", argv[1]);
    fflush(stdout);

    struct client cl[MAX_CLIENTS];
    for (int i = 0; i < MAX_CLIENTS; i++) cl[i].fd = -1;
    for (;;) {
        struct pollfd pf[MAX_CLIENTS + 1];
        int map[MAX_CLIENTS + 1], n = 0;
        pf[n].fd = ls; pf[n].events = POLLIN; map[n++] = -1;
        for (int i = 0; i < MAX_CLIENTS; i++)
            if (cl[i].fd >= 0) { pf[n].fd = cl[i].fd; pf[n].events = POLLIN; map[n++] = i; }
        if (poll(pf, (nfds_t)n, -1) < 0) { if (errno == EINTR) continue; return 1; }
        for (int k = 0; k < n; k++) {
            if (!pf[k].revents) continue;
            if (map[k] < 0) {
                int fd = accept(ls, NULL, NULL);
                if (fd < 0) continue;
                int slot = -1;
                for (int i = 0; i < MAX_CLIENTS; i++) if (cl[i].fd < 0) { slot = i; break; }
                if (slot < 0) { close(fd); continue; }
                cl[slot].fd = fd; cl[slot].offset = 0;
                continue;
            }
            struct client *c = &cl[map[k]];
            uint8_t buf[sizeof(struct stub_req) + STUB_MAX_MSG];
            ssize_t r = recv(c->fd, buf, sizeof buf, 0);
            if (r <= 0) { close(c->fd); c->fd = -1; continue; }
            handle(c, buf, (size_t)r);
        }
    }
}
