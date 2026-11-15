#!/bin/bash
# backtrace from the real QNX dumper core (recorded in run5)
. "$(dirname "$0")/common.sh"; cd $R
say "ntox86_64-gdb buggy_rm buggy_rm.core -batch -ex bt   # run5"
grep '^#' docs/results/qnx-run5/backtraces/38d435f06c3e3629.bt.txt | awk '!seen[$0]++' \
 | sed -E 's/ \(.*\) at / at /; s/^(#[0-9]) +(0x[0-9a-f]+ in )?/\1  /; s|/builds/workspace/QNXOS_805/build_x86_64/lib/c/|libc/|; s|buggy_rm/||' | cut -c1-72 | head -8
sleep 1
