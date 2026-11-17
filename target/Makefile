# SPDX-License-Identifier: Apache-2.0
# resfuzz target side.
#   make test    portable unit tests + mock_rm integration (gcc, ASan/UBSan)
#   make linux   resfuzz-agent + coverage-instrumented mock_rm for the Linux stub platform
#   make qnx     cross-compile with qcc (QNX SDP 8.0, gcc_ntox86_64; verified on QEMU,
#                see ../docs/TODO-QNX.md)
#   make qnx-checks  on-target check programs behind docs/TODO-QNX.md
CC      ?= gcc
BUILD   ?= build
CFLAGS  ?= -O2 -g
WARN    := -Wall -Wextra -Werror -Wno-unused-parameter
INC     := -Iinclude -Iplatform
SAN     := -fsanitize=address,undefined -fno-omit-frame-pointer
COVFLAGS:= -fsanitize-coverage=trace-pc,trace-cmp

CORE_SRC := src/util.c src/rng.c src/testcase.c src/dict.c src/mutate.c src/corpus.c \
            src/minimize.c src/cov.c src/exec.c
LINUX_PLAT := platform/platform_linux.c platform/platform_posix_common.c
BUGGY_INC  := -Ibuggy_rm -Iplatform/linux_compat

.PHONY: all linux test demo qnx qnx-checks clean
all: linux

$(BUILD):
	mkdir -p $(BUILD)

linux: $(BUILD)/resfuzz-agent $(BUILD)/mock_rm $(BUILD)/mock_rm_blind

$(BUILD)/resfuzz-agent: $(CORE_SRC) $(LINUX_PLAT) src/agent_main.c | $(BUILD)
	$(CC) $(CFLAGS) $(WARN) $(INC) -o $@ $^ -pthread -lrt

# Coverage-instrumented mock server: buggy logic + cov runtime (runtime itself not instrumented).
$(BUILD)/cov_rt.o: covrt/cov_rt.c | $(BUILD)
	$(CC) $(CFLAGS) $(WARN) $(INC) -c -o $@ $<
$(BUILD)/mock_rm: buggy_rm/mock_rm.c buggy_rm/buggy_logic.c $(BUILD)/cov_rt.o | $(BUILD)
	$(CC) $(CFLAGS) $(WARN) $(INC) $(BUGGY_INC) $(COVFLAGS) -o $@ buggy_rm/mock_rm.c buggy_rm/buggy_logic.c $(BUILD)/cov_rt.o -lrt
$(BUILD)/mock_rm_blind: buggy_rm/mock_rm.c buggy_rm/buggy_logic.c | $(BUILD)
	$(CC) $(CFLAGS) $(WARN) $(INC) $(BUGGY_INC) -o $@ $^

$(BUILD)/unit_tests: tests/test_main.c $(CORE_SRC) $(LINUX_PLAT) | $(BUILD)
	$(CC) -O1 -g $(WARN) $(SAN) $(INC) $(BUGGY_INC) -o $@ $^ -pthread -lrt

test: $(BUILD)/unit_tests $(BUILD)/mock_rm_blind
	MOCK_RM=$(BUILD)/mock_rm_blind $(BUILD)/unit_tests

# Linux-only end-to-end: find the planted buggy_rm bugs against mock_rm (no QNX).
# Creates .venv under the repo on first run. Override with DEMO_SECONDS / DEMO_OUT.
DEMO_SECONDS ?= 45
DEMO_SEED    ?= 7
DEMO_OUT     ?= ../resfuzz-results/demo
demo: linux
	@repo=$$(cd .. && pwd); \
	  if [ ! -x $$repo/.venv/bin/resfuzz ]; then \
	    python3 -m venv $$repo/.venv && $$repo/.venv/bin/pip -q install -e $$repo/host; \
	  fi; \
	  $$repo/.venv/bin/resfuzz local --seconds $(DEMO_SECONDS) --seed $(DEMO_SEED) \
	    --corpus $$repo/target/examples/demo_corpus --out $(DEMO_OUT) && \
	  $$repo/.venv/bin/python $$repo/scripts/check-demo.py $(DEMO_OUT)/report.md && \
	  echo "demo: report $(DEMO_OUT)/report.html"

# ---- QNX (verified with SDP 8.0) ----------------------------------------------------
# qcc target selection per https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.utilities/topic/q/qcc.html
QCC      ?= qcc
QNX_V    ?= gcc_ntox86_64
QBUILD   ?= build-qnx
QCFLAGS  := -V$(QNX_V) -O2 -g -Wall -Iinclude -Iplatform
QNX_PLAT := platform/platform_qnx.c platform/platform_posix_common.c
qnx:
	mkdir -p $(QBUILD)
	$(QCC) $(QCFLAGS) -o $(QBUILD)/resfuzz-agent $(CORE_SRC) $(QNX_PLAT) src/agent_main.c
	$(QCC) $(QCFLAGS) -c -o $(QBUILD)/cov_rt.o covrt/cov_rt.c
	$(QCC) $(QCFLAGS) -Ibuggy_rm $(COVFLAGS) -o $(QBUILD)/buggy_rm buggy_rm/buggy_rm.c buggy_rm/buggy_logic.c $(QBUILD)/cov_rt.o
	$(QCC) $(QCFLAGS) -Ibuggy_rm -o $(QBUILD)/buggy_rm_blind buggy_rm/buggy_rm.c buggy_rm/buggy_logic.c

# On-target check programs used for docs/TODO-QNX.md (not needed for fuzzing).
qnx-checks:
	mkdir -p $(QBUILD)
	$(QCC) $(QCFLAGS) -Wextra -Ibuggy_rm -o $(QBUILD)/qnx_checks tests/qnx/qnx_checks.c
	$(QCC) $(QCFLAGS) -Ibuggy_rm -o $(QBUILD)/devctl_len tests/qnx/devctl_len.c
	$(QCC) $(QCFLAGS) -o $(QBUILD)/name_srv tests/qnx/name_srv.c
	$(QCC) $(QCFLAGS) -o $(QBUILD)/d9_types tests/qnx/d9_types.c

clean:
	rm -rf $(BUILD) $(QBUILD)
