# SPDX-License-Identifier: Apache-2.0
"""Subprocess runner abstraction so every external command is testable."""
from __future__ import annotations

import subprocess
from dataclasses import dataclass
from typing import Callable, Sequence


@dataclass
class Result:
    returncode: int
    stdout: str = ""
    stderr: str = ""

    @property
    def ok(self) -> bool:
        return self.returncode == 0


Runner = Callable[..., Result]


def run_subprocess(argv: Sequence[str], cwd: str | None = None, timeout: float | None = None,
                   input: str | None = None) -> Result:
    try:
        p = subprocess.run(list(argv), cwd=cwd, timeout=timeout, input=input,
                           capture_output=True, text=True, errors="replace")
        return Result(p.returncode, p.stdout, p.stderr)
    except subprocess.TimeoutExpired as e:
        out = e.stdout.decode(errors="replace") if isinstance(e.stdout, bytes) else (e.stdout or "")
        return Result(124, out, f"timeout after {timeout}s")
    except FileNotFoundError as e:
        return Result(127, "", str(e))


class RecordingRunner:
    """Test double: records argv; replies with the first (needles, Result) whose needles all occur."""

    def __init__(self, replies: list[tuple[tuple[str, ...], Result]] | None = None):
        self.calls: list[dict] = []
        self.replies = replies or []

    def __call__(self, argv, cwd=None, timeout=None, input=None) -> Result:
        argv = list(argv)
        self.calls.append({"argv": argv, "cwd": cwd, "timeout": timeout, "input": input})
        joined = " ".join(argv)
        for needles, res in self.replies:
            if all(n in joined for n in needles):   # every needle appears in the command line
                return res
        return Result(0)
