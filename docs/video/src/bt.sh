#!/bin/bash
cd /workspace/qnx-hackathon/resfuzz
P='\033[1;32m$\033[0m '
echo -e "${P}ntox86_64-gdb buggy_rm buggy_rm.core -batch -ex bt     # dumper core from QNX"
sleep 0.8
grep '^#' docs/results/qnx-run5/backtraces/38d435f06c3e3629.bt.txt | awk '!seen[$0]++' | sed -E 's/ \(.*\) at / at /; s|/builds/workspace/QNXOS_805/build_x86_64/|<qnx-libc>/|' | cut -c1-118 | head -9
sleep 4
