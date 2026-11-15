#!/usr/bin/env python3
"""Print selected fields of resfuzz stats.json in two narrow columns."""
import json, sys
d = json.load(sys.stdin)
keys = ["platform", "elapsed_s", "execs", "execs_per_s", "edges", "crashes", "hangs", "deadline_misses", "respawns", "buckets"]
for a, b in zip(keys[0::2], keys[1::2]):
    print(f"  {a:16}{str(d[a]):>10}     {b:16}{str(d[b]):>10}")
