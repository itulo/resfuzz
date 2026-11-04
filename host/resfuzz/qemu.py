# SPDX-License-Identifier: Apache-2.0
"""QEMU image control through mkqnximage (QNX SDP 8.0).

Every option used here is from the QNX 8.0 utilities reference:
https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.utilities/topic/m/mkqnximage.html
  --type=qemu, --arch=x86_64|aarch64le, --build, --clean, --run, --run=-h
  (headless; console goes to output/qemu.out), --stop, --getip, --assumed-ip,
  --ssh-ident, --wipe, --options. mkqnximage always works on the image in
  the *current directory*, hence cwd=workdir on every call.
Supported QEMU host per that page: Ubuntu. NOT YET RUN (SDP not installed).
"""
from __future__ import annotations

import os
import re
import shutil
from pathlib import Path

from .config import ImageConfig
from .runner import Result, Runner, run_subprocess

IPV4 = re.compile(r"\b(\d{1,3}(?:\.\d{1,3}){3})\b")


class SdkMissing(RuntimeError):
    pass


def check_sdk(exe: str = "mkqnximage", env=os.environ) -> None:
    """mkqnximage needs a sourced SDP environment (qnxsdp-env.sh sets QNX_HOST/QNX_TARGET)."""
    missing = [v for v in ("QNX_HOST", "QNX_TARGET") if not env.get(v)]
    if missing or not shutil.which(exe):
        raise SdkMissing(
            "QNX SDP 8.0 environment not found (" + (", ".join(missing) or f"{exe} not on PATH") +
            "). Install the SDP and `source ~/qnx800/qnxsdp-env.sh` first; see docs/TODO-QNX.md.")


class MkQnxImage:
    def __init__(self, cfg: ImageConfig, runner: Runner = run_subprocess, exe: str = "mkqnximage"):
        self.cfg = cfg
        self.runner = runner
        self.exe = exe
        self.workdir = Path(cfg.workdir)

    # ---- argv builders (pure, unit-tested) ----
    def base_args(self) -> list[str]:
        a = [self.exe, "--type=qemu", f"--arch={self.cfg.arch}"]
        if self.cfg.ssh_ident:
            a.append(f"--ssh-ident={self.cfg.ssh_ident}")
        if self.cfg.assumed_ip:
            a.append(f"--assumed-ip={self.cfg.assumed_ip}")
        return a + list(self.cfg.extra_args)

    def build_argv(self, clean: bool = False) -> list[str]:
        return self.base_args() + ["--build"] + (["--clean"] if clean else [])

    def run_argv(self, headless: bool = True) -> list[str]:
        # Same options as the build so mkqnximage does not decide to rebuild.
        return self.base_args() + (["--run=-h"] if headless else ["--run"])

    def stop_argv(self) -> list[str]:
        return [self.exe, "--stop"]

    def getip_argv(self) -> list[str]:
        return [self.exe, "--getip"]

    def options_argv(self) -> list[str]:
        return [self.exe, "--options"]

    # ---- actions ----
    def _run(self, argv, timeout=None) -> Result:
        self.workdir.mkdir(parents=True, exist_ok=True)
        return self.runner(argv, cwd=str(self.workdir), timeout=timeout)

    def build(self, clean: bool = False) -> Result:
        return self._run(self.build_argv(clean), timeout=3600)

    def start(self, headless: bool = True) -> Result:
        return self._run(self.run_argv(headless), timeout=600)

    def stop(self) -> Result:
        return self._run(self.stop_argv(), timeout=120)

    def get_ip(self, timeout: float = 300) -> str:
        """--getip "waits for the system image to be up and running before it returns"."""
        if self.cfg.assumed_ip:
            return self.cfg.assumed_ip
        r = self._run(self.getip_argv(), timeout=timeout)
        m = IPV4.search(r.stdout or "")
        if not r.ok or not m:
            raise RuntimeError(f"mkqnximage --getip failed (rc={r.returncode}): {r.stderr.strip() or r.stdout.strip()}")
        return m.group(1)

    @property
    def console_log(self) -> Path:
        """Headless console output location per the mkqnximage docs."""
        return self.workdir / "output" / "qemu.out"
