#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Exit 0 iff a local/QNX demo report found the planted buggy_rm bugs.

Requires at least:
  - a reproducible CRASH naming GET_STATS (BUG2)
  - a reproducible CRASH naming SET_NAME (BUG1, stateful)
  - a reproducible DEADLINE naming COMPUTE (BUG3a)
Hang (BUG3b) is reported when present but not required (it is rarer under short runs).
"""
from __future__ import annotations

import re
import sys
from pathlib import Path


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} <report.md>", file=sys.stderr)
        return 2
    text = Path(sys.argv[1]).read_text(errors="replace")
    rows = []
    for line in text.splitlines():
        if not line.startswith("|") or "Finding" in line or re.match(r"\|[-| ]+\|", line):
            continue
        cells = [c.strip() for c in line.strip("|").split("|")]
        if len(cells) < 5 or not cells[0].isdigit():
            continue
        rows.append({"sev": cells[1], "finding": cells[2], "repro": cells[4]})

    def has(kind: str, name: str) -> bool:
        return any(
            kind in r["finding"] and name in r["finding"] and r["repro"].startswith("yes")
            for r in rows
        )

    ok_bug2 = has("CRASH", "GET_STATS")
    ok_bug1 = has("CRASH", "SET_NAME")
    ok_bug3a = has("DEADLINE", "COMPUTE")
    ok_bug3b = has("HANG", "COMPUTE")
    print(f"BUG2 GET_STATS crash: {'yes' if ok_bug2 else 'NO'}")
    print(f"BUG1 SET_NAME crash (stateful): {'yes' if ok_bug1 else 'NO'}")
    print(f"BUG3a COMPUTE deadline: {'yes' if ok_bug3a else 'NO'}")
    print(f"BUG3b COMPUTE hang (optional): {'yes' if ok_bug3b else 'not in this run'}")
    if ok_bug2 and ok_bug1 and ok_bug3a:
        print("demo OK: planted bugs found")
        return 0
    print("demo FAILED: expected planted bugs not all present", file=sys.stderr)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
