#!/bin/bash
R=/workspace/qnx-hackathon/resfuzz; cd $R
K="-i /tmp/rfclean/home/.ssh/qnx_resfuzz_ed25519 -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR"
P='\033[1;32m$\033[0m '
say(){ echo -e "${P}$1"; sleep 0.8; }
say "resfuzz decode docs/results/qnx-run5/min/crash_38d435f06c3e3629_2.seq | head -4   # recorded history"
.venv/bin/resfuzz decode docs/results/qnx-run5/min/crash_38d435f06c3e3629_2.seq 2>&1 | head -4 | cut -c1-110
sleep 2
say "resfuzz decode docs/results/qnx-run5/min/crash_38d435f06c3e3629_2.seq.min        # minimised"
.venv/bin/resfuzz decode docs/results/qnx-run5/min/crash_38d435f06c3e3629_2.seq.min 2>&1 | cut -c1-110
sleep 2.5
scp -q $K docs/results/qnx-run5/min/crash_38d435f06c3e3629_2.seq.min root@172.31.1.188:/data/resfuzz/bug1.min
say "for i in 1 2 3; do resfuzz-agent --target /dev/buggy --spawn buggy_rm --replay bug1.min; echo exit:\$?; done   # on QNX"
ssh $K root@172.31.1.188 'cd /data/resfuzz; for i in 1 2 3; do o=$(./resfuzz-agent --target /dev/buggy --spawn "/data/resfuzz/buggy_rm /dev/buggy" --out /tmp/rp --replay bug1.min 2>/dev/null); rc=$?; echo "$o" | cut -c1-70; echo "exit:$rc  (10 = crash)"; done' 2>&1 | grep -v ready
sleep 3
