/* SPDX-License-Identifier: Apache-2.0
 * Start a QNX resource manager that may call procmgr_daemon(), wait until a
 * device path is openable, then adopt the surviving server process so the
 * resfuzz agent can restart it. Usage:
 *   rmwrap --wait /dev/hd1 --name devb-ram -- /data/resfuzz/devb-ram ram capacity=2048 ...
 */
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static pid_t server = -1;

static void fwd(int sig)
{
    if (server > 0) {
        kill(server, sig);
        for (int i = 0; i < 50 && kill(server, 0) == 0; i++)
            usleep(10000);
        if (kill(server, 0) == 0)
            kill(server, SIGKILL);
    }
    if (sig == SIGABRT) {
        signal(SIGABRT, SIG_DFL);
        raise(SIGABRT);
    }
}

static int read_proc_file(pid_t pid, const char *leaf, char *buf, size_t n)
{
    char path[64];
    snprintf(path, sizeof path, "/proc/%d/%s", (int)pid, leaf);
    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return -1;
    ssize_t r = read(fd, buf, n - 1);
    close(fd);
    if (r < 0)
        return -1;
    buf[r] = 0;
    return (int)r;
}

static int name_matches(pid_t pid, const char *name)
{
    char buf[512];
    if (read_proc_file(pid, "exefile", buf, sizeof buf) > 0 && strstr(buf, name))
        return 1;
    if (read_proc_file(pid, "cmdline", buf, sizeof buf) > 0 && strstr(buf, name))
        return 1;
    return 0;
}

/* Collect pids whose exefile/cmdline contain name (name=NULL → all). Returns count. */
static int collect_pids(const char *name, pid_t *out, int max)
{
    DIR *d = opendir("/proc");
    if (!d)
        return 0;
    int n = 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL && n < max) {
        char *end = NULL;
        long pid = strtol(e->d_name, &end, 10);
        if (!end || *end || pid <= 0)
            continue;
        if (name && !name_matches((pid_t)pid, name))
            continue;
        out[n++] = (pid_t)pid;
    }
    closedir(d);
    return n;
}

static int pid_in(pid_t p, const pid_t *list, int n)
{
    for (int i = 0; i < n; i++)
        if (list[i] == p)
            return 1;
    return 0;
}

static pid_t find_new(const char *name, const pid_t *before, int nbefore, pid_t self, pid_t child)
{
    pid_t cur[256];
    int n = collect_pids(name, cur, 256);
    for (int i = 0; i < n; i++) {
        if (cur[i] == self || cur[i] == child)
            continue;
        if (!pid_in(cur[i], before, nbefore))
            return cur[i];
    }
    /* Child may itself be the server (no daemonize). */
    if (child > 0 && kill(child, 0) == 0 && (!name || name_matches(child, name)))
        return child;
    return -1;
}

static int path_ready(const char *path)
{
    int fd = open(path, O_RDONLY | O_NONBLOCK);
    if (fd >= 0) {
        close(fd);
        return 1;
    }
    /* Some nodes are write-only; try write-only too. */
    fd = open(path, O_WRONLY | O_NONBLOCK);
    if (fd >= 0) {
        close(fd);
        return 1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    const char *wait_path = NULL, *name = NULL;
    int i = 1;
    while (i < argc) {
        if (strcmp(argv[i], "--wait") == 0 && i + 1 < argc) {
            wait_path = argv[++i]; i++;
        } else if (strcmp(argv[i], "--name") == 0 && i + 1 < argc) {
            name = argv[++i]; i++;
        } else if (strcmp(argv[i], "--") == 0) {
            i++;
            break;
        } else {
            break;
        }
    }
    if (!wait_path || i >= argc) {
        fprintf(stderr, "usage: rmwrap --wait PATH [--name SUBSTR] -- CMD [ARGS...]\n");
        return 2;
    }
    if (!name) {
        name = strrchr(argv[i], '/');
        name = name ? name + 1 : argv[i];
    }

    /* Drop leftover instances from a previous run so we never attach two
     * copies of the same block/flash driver to one node. */
    {
        pid_t live[256];
        int nlive = collect_pids(name, live, 256);
        for (int k = 0; k < nlive; k++) {
            if (live[k] == getpid())
                continue;
            kill(live[k], SIGTERM);
        }
        usleep(300000);
        nlive = collect_pids(name, live, 256);
        for (int k = 0; k < nlive; k++) {
            if (live[k] == getpid())
                continue;
            kill(live[k], SIGKILL);
        }
        usleep(100000);
    }

    pid_t self = getpid();
    pid_t before[256];
    int nbefore = collect_pids(name, before, 256);

    pid_t child = fork();
    if (child < 0) {
        perror("fork");
        return 1;
    }
    if (child == 0) {
        execv(argv[i], &argv[i]);
        perror("execv");
        _exit(127);
    }

    /* Wait for the device node, even if the first process daemonizes and exits. */
    int started = 0;
    for (int t = 0; t < 1500; t++) { /* 15 s */
        int st = 0;
        pid_t w = waitpid(child, &st, WNOHANG);
        if (w == child)
            started = 1; /* first process exited (typical daemonize) */
        if (path_ready(wait_path))
            break;
        if (t == 1499) {
            fprintf(stderr, "rmwrap: %s did not appear\n", wait_path);
            if (!started)
                kill(child, SIGKILL);
            return 1;
        }
        usleep(10000);
    }

    server = find_new(name, before, nbefore, self, started ? -1 : child);
    if (server < 0) {
        fprintf(stderr, "rmwrap: could not find a live '%s' process after %s appeared\n",
                name, wait_path);
        return 1;
    }
    fprintf(stderr, "rmwrap: server pid %d for %s\n", (int)server, wait_path);

    signal(SIGTERM, fwd);
    signal(SIGINT, fwd);
    signal(SIGHUP, fwd);
    signal(SIGABRT, fwd);

    int st = 0;
    if (waitpid(server, &st, 0) < 0) {
        /* If server is not our child (daemonized, reparented to 1), poll. */
        while (kill(server, 0) == 0)
            usleep(100000);
        return 0;
    }
    if (WIFEXITED(st))
        return WEXITSTATUS(st);
    if (WIFSIGNALED(st)) {
        int sig = WTERMSIG(st);
        signal(sig, SIG_DFL);
        raise(sig);
    }
    return 1;
}
