#!/bin/bash
# BUG1: 271-case history -> 2-op reproducer -> replay 3/3 on QNX
. "$(dirname "$0")/common.sh"; cd $R
M=docs/results/qnx-run5/min/crash_38d435f06c3e3629_2.seq
say "resfuzz decode bug1.seq | head -1"
.venv/bin/resfuzz decode $M 2>&1 | head -1
say "resfuzz decode bug1.seq.min        # minimised"
.venv/bin/resfuzz decode $M.min 2>&1 | sed -E 's/ +slot0 / /; s/(data=[0-9a-f]{16})[0-9a-f]+/\1…/; s/  +/ /g; s/…\.+$/…/' | cut -c1-72
scp -q $K $M.min root@$IP:/data/resfuzz/bug1.min
say "for i in 1 2 3; do resfuzz-agent --replay bug1.min; done  # QNX"
ssh $K root@$IP 'cd /data/resfuzz; for i in 1 2 3; do ./resfuzz-agent --target /dev/buggy --spawn "/data/resfuzz/buggy_rm /dev/buggy" --out /tmp/rp --replay bug1.min >/dev/null 2>&1; echo "  replay $i: exit $?  (10 = crash)"; done'
sleep 1
