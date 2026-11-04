/* SPDX-License-Identifier: Apache-2.0 */
/* POSIX process control shared by both platforms (posix_spawn/waitpid/kill
 * are documented in the QNX 8.0 C library reference; e.g.
 * https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.lib_ref/topic/p/posix_spawn.html ). */
#include "rf_platform.h"

#include <errno.h>
#include <signal.h>
#include <spawn.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

extern char **environ;

pid_t rf_plat_spawn(char *const argv[], char *const env_extra[])
{
    size_t n_env = 0, n_extra = 0;
    while (environ && environ[n_env]) n_env++;
    while (env_extra && env_extra[n_extra]) n_extra++;
    char **env = calloc(n_env + n_extra + 1, sizeof *env);
    if (!env) return -1;
    size_t k = 0;
    for (size_t i = 0; i < n_extra; i++) env[k++] = env_extra[i];
    for (size_t i = 0; i < n_env; i++) {
        int shadowed = 0;
        for (size_t j = 0; j < n_extra; j++) {
            const char *eq = strchr(env_extra[j], '=');
            size_t kl = eq ? (size_t)(eq - env_extra[j]) : strlen(env_extra[j]);
            if (!strncmp(environ[i], env_extra[j], kl) && environ[i][kl] == '=') shadowed = 1;
        }
        if (!shadowed) env[k++] = environ[i];
    }
    pid_t pid;
    int rc = posix_spawnp(&pid, argv[0], NULL, NULL, argv, env);
    free(env);
    return rc == 0 ? pid : -1;
}

int rf_plat_poll_child(pid_t pid, int *st)
{
    int status = 0;
    pid_t r = waitpid(pid, &status, WNOHANG);
    if (r == 0) return 0;
    if (r < 0) return -1;
    if (st) *st = status;
    return 1;
}

void rf_plat_kill(pid_t pid)
{
    if (pid <= 0) return;
    kill(pid, SIGKILL);
    int st;
    while (waitpid(pid, &st, 0) < 0 && errno == EINTR) {}
}
