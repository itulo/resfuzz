/* SPDX-License-Identifier: Apache-2.0 */
/* Execute one test case against the target and classify the outcome. */
#ifndef RF_EXEC_H
#define RF_EXEC_H
#include "rf_testcase.h"
#include "rf_platform.h"

typedef enum {
    RF_RES_OK = 0,
    RF_RES_CRASH,      /* server died (MsgSend -> ESRCH, child signalled, ...) */
    RF_RES_HANG,       /* call exceeded hang timeout */
    RF_RES_DEADLINE,   /* completed, but some reply latency > deadline */
    RF_RES_SETUP_FAIL  /* could not even open the target */
} rf_result_kind;

typedef struct {
    const char *target;
    uint64_t    deadline_ns;   /* real-time oracle threshold per call */
    uint64_t    hang_ns;       /* hard timeout per call */
    pid_t       target_pid;    /* >0 if spawned by the agent (enables signal oracle) */
    /* Optional watchdog hooks (agent_main.c): begin() before each blocking
     * call, end() after it; end() returns 1 if the watchdog fired meanwhile. */
    void      (*wd_begin)(void *ctx);
    int       (*wd_end)(void *ctx);
    void       *wd_ctx;
} rf_exec_cfg;

typedef struct {
    rf_result_kind kind;
    int      op_index;       /* op that triggered the finding (-1 if n/a) */
    int      err;            /* positive errno of the triggering op */
    int      signal;         /* if the spawned target was signalled */
    int      target_reaped;  /* exec reaped the spawned target (it must be respawned) */
    uint64_t max_latency_ns;
    int      max_latency_op;
    uint64_t total_ns;
    unsigned ops_ok, ops_err;
    /* Reply signatures (op kind, dcmd, errno class) per executed op: cheap
     * client-side feedback used when no coverage instrumentation exists. */
    uint32_t sigs[RF_MAX_OPS];
    unsigned nsigs;
} rf_result;

int  rf_exec(const rf_exec_cfg *cfg, const rf_tc *tc, rf_result *res);
const char *rf_result_name(rf_result_kind k);
/* Is a per-call error a sign that the server is gone? (pure, unit-tested) */
int  rf_err_means_server_gone(int err);
/* Pure helper: cycles -> ns without overflow. */
uint64_t rf_cycles_to_ns(uint64_t cycles, uint64_t cps);

#endif
