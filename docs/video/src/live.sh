#!/bin/bash
K="-i /tmp/rfclean/home/.ssh/qnx_resfuzz_ed25519 -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR"
P='\033[1;32m$\033[0m '
echo -e "${P}ssh root@qnx-target 'tail -f /data/resfuzz/out/events.jsonl'   # live, on QNX 8.0"
timeout 14 ssh $K root@172.31.1.188 'tail -n 4 -f /data/resfuzz/out/events.jsonl' | cut -c1-118
echo -e "${P}ssh root@qnx-target 'cat /data/resfuzz/out/stats.json'"
ssh $K root@172.31.1.188 'cat /data/resfuzz/out/stats.json' | python3 -c "import json,sys;d=json.load(sys.stdin);[print(f'  {k:16} {d[k]}') for k in ['platform','target','elapsed_s','execs','execs_per_s','edges','crashes','hangs','deadline_misses','respawns']]"
sleep 3
