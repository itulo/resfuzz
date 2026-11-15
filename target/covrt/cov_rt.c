/* SPDX-License-Identifier: Apache-2.0 */
/*
 * resfuzz coverage runtime, linked INTO the server under test.
 *
 * Build the server with GCC's SanitizerCoverage hooks:
 *     -fsanitize-coverage=trace-pc,trace-cmp
 * and link this file compiled WITHOUT those flags. GCC then calls
 * __sanitizer_cov_trace_pc() on every basic-block edge and
 * __sanitizer_cov_trace_[const_]cmpN() on comparisons
 * (https://gcc.gnu.org/onlinedocs/gcc/Instrumentation-Options.html).
 *
 * QNX: qcc passes any -f option straight to the compiler (qcc docs,
 * https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.utilities/topic/q/qcc.html,
 * "Handling other options"). VERIFIED: QNX 8.0's gcc 12.2.0 accepts it and emits
 * the trace-pc/cmp/switch hooks (docs/TODO-QNX.md C1, C2). Without it
 * resfuzz falls back to blind (non-coverage) fuzzing; nothing else changes.
 *
 * If $RESFUZZ_SHM is unset or the shm cannot be mapped, hits go to a private
 * dummy map, so an instrumented server still runs normally outside resfuzz.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1          /* dladdr() on glibc */
#endif
#include "rf_cov.h"

#include <dlfcn.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

static rf_cov_shm  rf_dummy;
static rf_cov_shm *rf_shm = &rf_dummy;
static __thread uintptr_t rf_prev;
/* Load address of the instrumented executable. qcc links PIE by default and
 * QNX 8.0 randomises the load address on every spawn (checked on the QEMU
 * target: main at 0x2f46d7d6e0, 0x5e131eb6e0, 0x469684b6e0 in three runs), so
 * hashing absolute return addresses gave every respawned server a fresh set
 * of "new" edges. Edges are hashed from module-relative offsets instead. */
static uintptr_t rf_base;

__attribute__((constructor)) static void rf_cov_rt_init(void)
{
    Dl_info di;
    if (dladdr((void *)&rf_cov_rt_init, &di) && di.dli_fbase) rf_base = (uintptr_t)di.dli_fbase;
    const char *name = getenv("RESFUZZ_SHM");
    if (!name || !*name) return;
    int fd = shm_open(name, O_RDWR, 0);
    if (fd < 0) return;
    void *p = mmap(NULL, sizeof(rf_cov_shm), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (p != MAP_FAILED && ((rf_cov_shm *)p)->magic == RF_SHM_MAGIC) rf_shm = p;
}

static inline uintptr_t rf_hash_pc(uintptr_t pc)
{
    pc ^= pc >> 17;
    pc *= (uintptr_t)0x9E3779B97F4A7C15ull;
    return pc ^ (pc >> 29);
}

void __sanitizer_cov_trace_pc(void)
{
    uintptr_t cur = rf_hash_pc((uintptr_t)__builtin_return_address(0) - rf_base) & (RF_MAP_SIZE - 1);
    uint8_t *slot = &rf_shm->map[(cur ^ rf_prev) & (RF_MAP_SIZE - 1)];
    if (*slot != 0xff) (*slot)++;          /* saturating, avoids wrap to "unseen" */
    rf_prev = cur >> 1;
}

static inline void rf_log_cmp(uint64_t a, uint64_t b, uint8_t size, uint8_t is_const)
{
    if (a == b) return;                    /* only failed comparisons are interesting */
    uint32_t i = __atomic_fetch_add(&rf_shm->cmp_count, 1, __ATOMIC_RELAXED) % RF_CMPLOG_N;
    rf_cmp_entry *e = &rf_shm->cmp[i];
    e->a = a; e->b = b; e->size = size; e->is_const = is_const;
}

void __sanitizer_cov_trace_cmp1(uint8_t a, uint8_t b)   { rf_log_cmp(a, b, 1, 0); }
void __sanitizer_cov_trace_cmp2(uint16_t a, uint16_t b) { rf_log_cmp(a, b, 2, 0); }
void __sanitizer_cov_trace_cmp4(uint32_t a, uint32_t b) { rf_log_cmp(a, b, 4, 0); }
void __sanitizer_cov_trace_cmp8(uint64_t a, uint64_t b) { rf_log_cmp(a, b, 8, 0); }
/* SanitizerCoverage docs: for const_cmp, Arg1 is the compile-time constant
 * (https://clang.llvm.org/docs/SanitizerCoverage.html#tracing-data-flow). */
void __sanitizer_cov_trace_const_cmp1(uint8_t a, uint8_t b)   { rf_log_cmp(a, b, 1, 1); }
void __sanitizer_cov_trace_const_cmp2(uint16_t a, uint16_t b) { rf_log_cmp(a, b, 2, 1); }
void __sanitizer_cov_trace_const_cmp4(uint32_t a, uint32_t b) { rf_log_cmp(a, b, 4, 1); }
void __sanitizer_cov_trace_const_cmp8(uint64_t a, uint64_t b) { rf_log_cmp(a, b, 8, 1); }
void __sanitizer_cov_trace_cmpf(float a, float b)   { (void)a; (void)b; }
void __sanitizer_cov_trace_cmpd(double a, double b) { (void)a; (void)b; }

/* cases[0] = number of cases, cases[1] = bit width, cases[2..] = case values. */
void __sanitizer_cov_trace_switch(uint64_t val, uint64_t *cases)
{
    uint64_t n = cases[0], bits = cases[1];
    for (uint64_t i = 0; i < n && i < 64; i++) rf_log_cmp(cases[2 + i], val, (uint8_t)(bits / 8), 1);  /* constant first, like const_cmp */
}
