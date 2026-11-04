# SPDX-License-Identifier: Apache-2.0
"""Talking to the target: ssh/scp (primary), serial console (fallback), local (Linux stub).

mkqnximage images run sshd, permit root login and accept scp/sftp
(https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.utilities/topic/m/mkqnximage.html,
"Connecting to an image" and "Image security"). Commands we run on the
target are deliberately plain POSIX-shell (ksh on QNX); anything else is
listed in docs/TODO-QNX.md.
"""
from __future__ import annotations

import shlex
import time
import uuid
from typing import Protocol

from .config import SshConfig
from .runner import Result, Runner, run_subprocess


class Target(Protocol):
    def run(self, cmd: str, timeout: float | None = None) -> Result: ...
    def put(self, local: str, remote: str) -> Result: ...
    def get(self, remote: str, local: str, recursive: bool = False) -> Result: ...


class SshTarget:
    def __init__(self, host: str, cfg: SshConfig, ident: str | None = None, runner: Runner = run_subprocess):
        self.host, self.cfg, self.ident, self.runner = host, cfg, ident, runner

    def _common(self) -> list[str]:
        a = list(self.cfg.options)
        if self.ident:
            a += ["-i", self.ident]
        return a

    def ssh_argv(self, cmd: str) -> list[str]:
        return ["ssh", "-p", str(self.cfg.port)] + self._common() + [f"{self.cfg.user}@{self.host}", cmd]

    def scp_put_argv(self, local: str, remote: str) -> list[str]:
        return ["scp", "-P", str(self.cfg.port)] + self._common() + [local, f"{self.cfg.user}@{self.host}:{remote}"]

    def scp_get_argv(self, remote: str, local: str, recursive: bool = False) -> list[str]:
        return (["scp", "-P", str(self.cfg.port)] + (["-r"] if recursive else []) + self._common() +
                [f"{self.cfg.user}@{self.host}:{remote}", local])

    def run(self, cmd: str, timeout: float | None = None) -> Result:
        return self.runner(self.ssh_argv(cmd), timeout=timeout)

    def put(self, local: str, remote: str) -> Result:
        return self.runner(self.scp_put_argv(local, remote), timeout=300)

    def get(self, remote: str, local: str, recursive: bool = False) -> Result:
        return self.runner(self.scp_get_argv(remote, local, recursive), timeout=600)

    def wait_up(self, attempts: int = 60, delay: float = 2.0) -> bool:
        for _ in range(attempts):
            if self.run("true", timeout=15).ok:
                return True
            time.sleep(delay)
        return False


class SerialTarget:
    """Fallback when networking is unavailable: drive the QEMU console.

    `stream` is any object with write(bytes)/read_until(marker: bytes, timeout) ->
    bytes (e.g. a small wrapper around a pty or QEMU's -serial unix socket).
    Commands are wrapped with a unique sentinel so output and exit status can
    be recovered. File transfer is not supported: bake binaries into the image
    via mkqnximage snippets instead (local/snippets/data_files.custom; syntax
    is mkqnx6fsimg buildfile format - UNVERIFIED, see docs/TODO-QNX.md).
    """

    def __init__(self, stream):
        self.stream = stream

    @staticmethod
    def wrap(cmd: str, tag: str) -> bytes:
        return f"{cmd}; echo {tag}:$?\n".encode()

    @staticmethod
    def unwrap(raw: bytes, tag: str) -> Result:
        text = raw.decode(errors="replace")
        idx = text.rfind(f"{tag}:")
        if idx < 0:
            return Result(124, text, "sentinel not found (timeout?)")
        tail = text[idx + len(tag) + 1:].split()
        rc = int(tail[0]) if tail and tail[0].lstrip("-").isdigit() else 1
        body = text[:idx]
        # Drop the echoed command line if the console echoes input.
        lines = [ln for ln in body.splitlines() if f"echo {tag}" not in ln]
        return Result(rc, "\n".join(lines).strip("\n"), "")

    def run(self, cmd: str, timeout: float | None = None) -> Result:
        tag = "RFZ" + uuid.uuid4().hex[:12]
        self.stream.write(self.wrap(cmd, tag))
        raw = self.stream.read_until(f"{tag}:".encode(), timeout or 30)
        raw += self.stream.read_until(b"\n", 2)
        return self.unwrap(raw, tag)

    def put(self, local: str, remote: str) -> Result:
        raise NotImplementedError("serial transfer unsupported: include files in the image instead")

    def get(self, remote: str, local: str, recursive: bool = False) -> Result:
        raise NotImplementedError("serial transfer unsupported")


class LocalTarget:
    """Runs everything on this Linux host (linux-stub platform, mock_rm)."""

    def __init__(self, runner: Runner = run_subprocess):
        self.runner = runner

    def run(self, cmd: str, timeout: float | None = None) -> Result:
        return self.runner(["sh", "-c", cmd], timeout=timeout)

    def put(self, local: str, remote: str) -> Result:
        return self.runner(["cp", local, remote])

    def get(self, remote: str, local: str, recursive: bool = False) -> Result:
        return self.runner(["cp"] + (["-r"] if recursive else []) + [remote, local])


def q(s: str) -> str:
    return shlex.quote(s)
