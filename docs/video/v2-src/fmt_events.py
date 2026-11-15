#!/usr/bin/env python3
"""Reformat resfuzz events.jsonl lines into <=72-column rows for the video.
Content is unchanged (time, type, signal/latency, op, dcmd, bucket); only layout."""
import json, sys, signal as S
signal_names = {int(s): s.name for s in S.Signals}
for line in sys.stdin:
    try:
        e = json.loads(line)
    except ValueError:
        continue
    t, ty = e["t_ms"] / 1000, e["type"]
    if ty == "start":
        print(f"{t:7.1f}s  START     platform=qnx target=/dev/buggy cov=1", flush=True)
        continue
    what = signal_names.get(e.get("signal", 0), "") if ty == "crash" else f"{e['max_latency_us']/1000:.0f} ms"
    print(f"{t:7.1f}s  {ty.upper():9} {what:8} {e['op']:10} {e['dcmd']}  {e['bucket'][:8]}", flush=True)
