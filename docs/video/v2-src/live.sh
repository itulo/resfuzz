#!/bin/bash
# live campaign on QNX: event stream, then the stats line
. "$(dirname "$0")/common.sh"
say "tail -f out/events.jsonl            # live, on QNX 8.0"
timeout 8 ssh $K root@$IP 'tail -n 3 -f /data/resfuzz/out/events.jsonl' | python3 $D/fmt_events.py
say "cat out/stats.json"
ssh $K root@$IP 'cat /data/resfuzz/out/stats.json' | python3 $D/fmt_stats.py
sleep 1
