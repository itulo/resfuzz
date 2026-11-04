# SPDX-License-Identifier: Apache-2.0
"""Crash/hang/deadline triage: group agent events, attach test cases,
minimised reproducers and gdb backtraces, and rank the result."""
from __future__ import annotations

import hashlib
import json
import re
from dataclasses import dataclass, field
from pathlib import Path

from .testcase import FormatError, TestCase

SIGNALS = {4: "SIGILL", 5: "SIGTRAP", 6: "SIGABRT", 7: "SIGBUS", 8: "SIGFPE", 9: "SIGKILL",
           10: "SIGUSR1", 11: "SIGSEGV", 13: "SIGPIPE"}  # Linux numbering; QNX differs - shown as numbers there
FINDING_TYPES = ("crash", "hang", "deadline")


@dataclass
class Frame:
    index: int
    func: str
    addr: str | None = None
    file: str | None = None
    line: int | None = None


@dataclass
class Finding:
    type: str
    bucket: str
    op: str = ""
    dcmd: str = ""
    dcmd_name: str = ""
    errno: int = 0
    errno_name: str = ""
    signal: int = 0
    max_latency_us: int = 0
    reproducible: bool | None = None
    count: int = 0
    first_t_ms: int = 0
    files: list[str] = field(default_factory=list)
    testcase: TestCase | None = None
    minimized: TestCase | None = None
    frames: list[Frame] = field(default_factory=list)

    @property
    def signal_name(self) -> str:
        return SIGNALS.get(self.signal, str(self.signal)) if self.signal else ""

    @property
    def severity(self) -> str:
        if self.type == "crash":
            return "high" if self.signal in (7, 11) or self.signal == 0 else "medium"
        if self.type == "hang":
            return "medium"
        return "low" if self.reproducible is False else "medium"

    @property
    def title(self) -> str:
        where = f"{self.op} {self.dcmd_name or self.dcmd}".strip()
        extra = f" ({self.signal_name})" if self.signal else ""
        if self.type == "deadline":
            extra = f" ({self.max_latency_us} us)"
        top = next((f.func for f in self.frames if f.func and f.func != "??"), None)
        return f"{self.type.upper()} in {where}{extra}" + (f" at {top}" if top else "")


def load_events(path: str | Path) -> tuple[list[dict], int]:
    """Returns (events, malformed_line_count)."""
    events, bad = [], 0
    p = Path(path)
    if not p.exists():
        return [], 0
    for line in p.read_text(errors="replace").splitlines():
        if not line.strip():
            continue
        try:
            ev = json.loads(line)
            if isinstance(ev, dict):
                events.append(ev)
            else:
                bad += 1
        except json.JSONDecodeError:
            bad += 1
    return events, bad


def load_dict_names(paths) -> dict[int, str]:
    names: dict[int, str] = {}
    for p in paths or []:
        for line in Path(p).read_text().splitlines():
            parts = line.split()
            if len(parts) >= 3 and parts[0] == "dcmd":
                try:
                    names[int(parts[2], 0)] = parts[1]
                except ValueError:
                    pass
    return names


def _resolve(file: str, out_dir: Path) -> Path:
    """Agent paths are target-side; map them into the fetched out_dir."""
    p = Path(file)
    if p.exists():
        return p
    for anchor in ("crashes", "hangs", "deadlines"):
        if anchor in p.parts:
            i = p.parts.index(anchor)
            return out_dir.joinpath(*p.parts[i:])
    return out_dir / p.name


def group_findings(events: list[dict], out_dir: str | Path, dcmd_names: dict[int, str] | None = None) -> list[Finding]:
    out_dir = Path(out_dir)
    dcmd_names = dcmd_names or {}
    by_bucket: dict[str, Finding] = {}
    for ev in events:
        t = ev.get("type")
        if t not in FINDING_TYPES:
            continue
        key = f"{t}:{ev.get('bucket', '?')}"
        f = by_bucket.get(key)
        if f is None:
            dcmd = ev.get("dcmd", "")
            try:
                name = dcmd_names.get(int(dcmd, 16), "") if dcmd else ""
            except ValueError:
                name = ""
            f = Finding(type=t, bucket=str(ev.get("bucket", "?")), op=ev.get("op", ""), dcmd=dcmd,
                        dcmd_name=name, errno=int(ev.get("errno", 0) or 0), errno_name=ev.get("errno_name", ""),
                        signal=int(ev.get("signal", 0) or 0), first_t_ms=int(ev.get("t_ms", 0) or 0))
            by_bucket[key] = f
        f.count += 1
        f.max_latency_us = max(f.max_latency_us, int(ev.get("max_latency_us", 0) or 0))
        rep = ev.get("reproducible")
        if rep is True:
            f.reproducible = True
        elif rep is False and f.reproducible is None:
            f.reproducible = False
        if ev.get("file"):
            f.files.append(ev["file"])
    for f in by_bucket.values():
        for file in f.files:
            try:
                f.testcase = TestCase.load(_resolve(file, out_dir))
                break
            except (OSError, FormatError):
                continue
    order = {"crash": 0, "hang": 1, "deadline": 2}
    return sorted(by_bucket.values(),
                  key=lambda f: (order[f.type], f.reproducible is False, -f.count, f.bucket))


# "#0  0x0000000000401136 in func (a=1) at buggy.c:42" | "#1  func () at x.c:3" | "#2  0x... in ?? ()"
_FRAME = re.compile(
    r"^#(?P<idx>\d+)\s+(?:(?P<addr>0x[0-9a-fA-F]+)\s+in\s+)?(?P<func>[^\s(]+)\s*\(.*?\)"
    r"(?:\s+(?:at|from)\s+(?P<file>[^\s:]+)(?::(?P<line>\d+))?)?\s*$")


def parse_backtrace(text: str) -> list[Frame]:
    frames = []
    for line in text.splitlines():
        m = _FRAME.match(line.strip())
        if m:
            frames.append(Frame(int(m["idx"]), m["func"], m["addr"], m["file"],
                                int(m["line"]) if m["line"] else None))
    return frames


def stack_hash(frames: list[Frame], top: int = 3) -> str:
    """Address-independent signature of the top frames (for cross-run dedupe)."""
    sig = "|".join(f"{f.func}@{Path(f.file).name if f.file else ''}" for f in frames[:top])
    return hashlib.sha1(sig.encode()).hexdigest()[:16]


def gdb_argv(program: str, core: str, gdb: str = "gdb") -> list[str]:
    """Batch backtrace of a core. On the host for QNX cores use the SDP's
    cross gdb (name such as ntox86_64-gdb is UNVERIFIED - docs/TODO-QNX.md);
    on the target `gdb program core` is the documented dumper workflow
    (https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.utilities/topic/d/dumper.html)."""
    return [gdb, "-batch", "-nx", "-ex", "set pagination off", "-ex", "bt", program, core]


def summarize(findings: list[Finding]) -> dict:
    s = {t: 0 for t in FINDING_TYPES}
    for f in findings:
        s[f.type] += 1
    s["reproducible"] = sum(1 for f in findings if f.reproducible)
    return s
