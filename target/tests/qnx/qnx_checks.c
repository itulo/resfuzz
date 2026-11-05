/* SPDX-License-Identifier: Apache-2.0 */
/*
 * qnx_checks: on-target probes for docs/TODO-QNX.md (D1-D4, D9, E1, E3, C3).
 * Build: qcc -Vgcc_ntox86_64 -Wall -I../../buggy_rm -o qnx_checks qnx_checks.c
 * Run:   ./qnx_checks [/dev/buggy]   (the devctl/MsgSend probes need buggy_rm running)
 */
#include <assert.h>
#include <devctl.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/iofunc.h>
#include <sys/mman.h>
#include <sys/neutrino.h>
#include <sys/syspage.h>
#include <time.h>
#include <unistd.h>

#include "buggy_rm.h"

/* D1: layout as used by platform_qnx.c */
static_assert(sizeof(struct _io_devctl) == 16, "_io_devctl size");
static_assert(offsetof(struct _io_devctl, type) == 0, "type");
static_assert(offsetof(struct _io_devctl, combine_len) == 2, "combine_len");
static_assert(offsetof(struct _io_devctl, dcmd) == 4, "dcmd");
static_assert(offsetof(struct _io_devctl, nbytes) == 8, "nbytes");
static_assert(offsetof(struct _io_devctl, zero) == 12, "zero");
static_assert(sizeof(struct _io_devctl_reply) == 16, "_io_devctl_reply size");
static_assert(offsetof(struct _io_devctl_reply, ret_val) == 4, "ret_val");
static_assert(offsetof(struct _io_devctl_reply, nbytes) == 8, "reply nbytes");

static uint64_t now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static void e1_timertimeout(uint64_t ns)
{
    int chid = ChannelCreate(0);
    char buf[16];
    uint64_t t = ns;
    int tr = TimerTimeout(CLOCK_MONOTONIC, _NTO_TIMEOUT_RECEIVE, NULL, &t, NULL);
    int tr_errno = tr == -1 ? errno : 0;
    uint64_t t0 = now_ns();
    int r = MsgReceive(chid, buf, sizeof buf, NULL);
    int e = errno;
    uint64_t dt = now_ns() - t0;
    printf("E1 TimerTimeout(%llu ns): ret=%d errno=%d(%s); MsgReceive ret=%d errno=%d(%s) after %.3f s\n",
           (unsigned long long)ns, tr, tr_errno, tr_errno ? strerror(tr_errno) : "-", r, e,
           r == -1 ? strerror(e) : "-", dt / 1e9);
    ChannelDestroy(chid);
}

static void e3_clock(void)
{
    uint64_t cps = SYSPAGE_ENTRY(qtime)->cycles_per_sec;
    printf("E3 cycles_per_sec=%llu\n", (unsigned long long)cps);
    for (int i = 0; i < 5; i++) {
        uint64_t c0 = ClockCycles(), n0 = now_ns();
        struct timespec d = { 0, 200 * 1000000 };
        nanosleep(&d, NULL);
        uint64_t c1 = ClockCycles(), n1 = now_ns();
        double meas = (double)(c1 - c0) / ((n1 - n0) / 1e9);
        printf("E3 sample %d: wall %.3f ms, cycles->%.3f ms, implied freq %.0f Hz (%.3f%% off)\n", i,
               (n1 - n0) / 1e6, (c1 - c0) * 1e3 / (double)cps, meas, 100.0 * (meas - (double)cps) / (double)cps);
    }
}

static void c3_shm(void)
{
    char name[64];
    snprintf(name, sizeof name, "/resfuzz_cov_%d", (int)getpid());
    int fd = shm_open(name, O_CREAT | O_RDWR | O_EXCL, 0600);
    int e = errno;
    printf("C3 shm_open(%s) -> %d %s\n", name, fd, fd < 0 ? strerror(e) : "ok");
    if (fd >= 0) {
        int tr = ftruncate(fd, 65536);
        void *p = mmap(NULL, 65536, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        printf("C3 ftruncate=%d mmap=%s\n", tr, p == MAP_FAILED ? strerror(errno) : "ok");
        close(fd);
        shm_unlink(name);
    }
}

static void devctl_probes(const char *path)
{
    int fd = open(path, O_RDWR);
    if (fd < 0) { printf("D* open(%s): %s (skipping devctl probes)\n", path, strerror(errno)); return; }
    struct buggy_name nm;
    int info = -12345;
    memset(&nm, 0, sizeof nm);
    int r = devctl(fd, DCMD_BUGGY_GET_NAME, &nm, sizeof nm, &info);
    printf("D3 devctl(GET_NAME) returned %d (EOK=%d), dev_info=%d, name='%.*s'\n", r, EOK, info,
           (int)sizeof nm.name, nm.name);
    r = devctl(fd, 0x0000ffee, NULL, 0, &info);
    printf("D3 devctl(unknown dcmd 0xffee) returned %d (%s) - errno is %d\n", r, strerror(r), errno);

    /* D4: hand-built _IO_DEVCTL via MsgSend on the fd */
    struct { struct _io_devctl h; } m;
    struct { struct _io_devctl_reply h; char d[64]; } rep;
    memset(&m, 0, sizeof m); memset(&rep, 0, sizeof rep);
    m.h.type = _IO_DEVCTL; m.h.combine_len = sizeof m.h; m.h.dcmd = DCMD_BUGGY_GET_NAME; m.h.nbytes = sizeof nm;
    long st = MsgSend(fd, &m, sizeof m.h, &rep, sizeof rep);
    printf("D4 MsgSend(fd, _IO_DEVCTL GET_NAME) -> %ld errno=%d ret_val=%d nbytes=%u data='%.32s'\n", st,
           st == -1 ? errno : 0, rep.h.ret_val, rep.h.nbytes, rep.d);

    /* D9: arbitrary first u16 */
    int rej = 0;
    for (unsigned t = 0; t < 0x140; t++) {
        if (t == _IO_CONNECT) continue;   /* connect messages need a full _io_connect body */
        uint16_t msg[8] = { (uint16_t)t, 16 };
        char rb[64];
        long s = MsgSend(fd, msg, sizeof msg, rb, sizeof rb);
        if (s == -1) {
            int e = errno;
            if (e != ENOSYS && e != EINVAL && e != EBADMSG && e != EBADF && e != EPERM) {
                printf("D9 type 0x%03x -> errno %d (%s)\n", t, e, strerror(e));
                rej++;
            }
        }
    }
    printf("D9 done (%d types returned an errno other than ENOSYS/EINVAL/EBADMSG/EBADF/EPERM)\n", rej);
    close(fd);
}

int main(int argc, char **argv)
{
    printf("D1 sizeof(_io_devctl)=%zu sizeof(_io_devctl_reply)=%zu (static_asserts passed)\n",
           sizeof(struct _io_devctl), sizeof(struct _io_devctl_reply));
    e1_timertimeout(2000000000ull);
    e1_timertimeout(500000000ull);
    e3_clock();
    c3_shm();
    devctl_probes(argc > 1 ? argv[1] : BUGGY_PATH);
    return 0;
}
