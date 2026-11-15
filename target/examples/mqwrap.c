/* SPDX-License-Identifier: Apache-2.0
 * Spawn mqueue in the foreground and create a named queue so resfuzz can
 * open /dev/mqueue/<name> after every server restart.
 * Usage: mqwrap /path/to/mqueue /rfz
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <mqueue.h>
#include <errno.h>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <dirent.h>
static pid_t child = -1;
static void fwd(int sig) { if (child > 0) kill(child, sig); }
int main(int argc, char **argv) {
    const char *mq = (argc > 1) ? argv[1] : "/data/resfuzz/mqueue";
    const char *qname = (argc > 2) ? argv[2] : "/rfz";
    signal(SIGTERM, fwd); signal(SIGINT, fwd); signal(SIGHUP, fwd);
    /* Reap leftover mqueue processes from a previous wrap that was SIGKILL'd. */
    {
        DIR *d = opendir("/proc");
        if (d) {
            struct dirent *e;
            while ((e = readdir(d)) != NULL) {
                char *end = NULL;
                long pid = strtol(e->d_name, &end, 10);
                if (!end || *end || pid <= 0 || pid == getpid())
                    continue;
                char path[64], buf[256];
                snprintf(path, sizeof path, "/proc/%ld/exefile", pid);
                int fd = open(path, O_RDONLY);
                if (fd < 0)
                    continue;
                ssize_t r = read(fd, buf, sizeof buf - 1);
                close(fd);
                if (r <= 0)
                    continue;
                buf[r] = 0;
                if (strstr(buf, "mqueue"))
                    kill((pid_t)pid, SIGKILL);
            }
            closedir(d);
            usleep(50000);
        }
    }
    child = fork();
    if (child == 0) {
        execl(mq, mq, "-d", (char *)0);
        perror("execl mqueue");
        _exit(127);
    }
    if (child < 0) { perror("fork"); return 1; }
    /* wait until /dev/mqueue exists */
    for (int i = 0; i < 200; i++) {
        if (access("/dev/mqueue", F_OK) == 0) break;
        usleep(10000);
    }
    struct mq_attr a; memset(&a, 0, sizeof a);
    a.mq_maxmsg = 8; a.mq_msgsize = 128;
    mqd_t q = mq_open(qname, O_CREAT|O_RDWR, 0666, &a);
    if (q < 0) fprintf(stderr, "mqwrap: mq_open %s: %s\n", qname, strerror(errno));
    else mq_close(q);
    int st = 0;
    if (waitpid(child, &st, 0) < 0) return 1;
    if (WIFEXITED(st)) return WEXITSTATUS(st);
    if (WIFSIGNALED(st)) {
        /* re-raise so the agent sees the same signal on the wrapper */
        signal(WTERMSIG(st), SIG_DFL);
        raise(WTERMSIG(st));
    }
    return 1;
}
