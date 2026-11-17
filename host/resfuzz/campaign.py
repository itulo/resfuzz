# SPDX-License-Identifier: Apache-2.0
"""Campaign drivers.

LocalCampaign  - Linux only: builds the agent + mock_rm (linux-stub platform) and
                 runs the whole loop on this host. Used for development, CI and
                 the codelab's first section.
QemuCampaign   - the real thing: mkqnximage QEMU image, ssh deploy, run the
                 agent on QNX, fetch results/cores, minimise, report.
                 Run end to end against QNX 8.0 x86_64 under QEMU (TCG); results
                 and target-side findings are in docs/TODO-QNX.md.
"""
from __future__ import annotations

import json
import shlex
import tempfile
from dataclasses import dataclass
from pathlib import Path

from .config import Config
from .control import SshTarget, Target
from .qemu import MkQnxImage, check_sdk
from .report import write_reports
from .runner import Result, Runner, run_subprocess
from .testcase import FormatError, TestCase, load_any
from .triage import Finding, gdb_argv, group_findings, load_dict_names, load_events, parse_backtrace

EXIT_CODES = {0: "ok", 10: "crash", 11: "hang", 12: "deadline", 2: "setup_fail"}


def agent_args(target: str, out: str, *, spawn: str | None = None, cov: bool = False, dicts=(),
               seconds: int | None = None, deadline_us: int = 5000, hang_ms: int = 1000,
               seed: int | None = None, no_raw: bool = False, corpus: str | None = None,
               quiet: bool = True) -> list[str]:
    a = ["--target", target, "--out", out, "--deadline-us", str(deadline_us), "--hang-ms", str(hang_ms)]
    if spawn:
        a += ["--spawn", spawn]
    if cov:
        a.append("--cov")
    for d in dicts:
        a += ["--dict", d]
    if seconds:
        a += ["--seconds", str(seconds)]
    if seed is not None:
        a += ["--seed", str(seed)]
    if no_raw:
        a.append("--no-raw")
    if corpus:
        a += ["--corpus", corpus]
    if quiet:
        a.append("--quiet")
    return a


def _load_stats(out: Path) -> dict:
    try:
        return json.loads((out / "stats.json").read_text())
    except (OSError, json.JSONDecodeError):
        return {}


@dataclass
class CampaignResult:
    out_dir: Path
    findings: list[Finding]
    stats: dict
    report_md: Path | None = None
    report_html: Path | None = None
    malformed_events: int = 0


def _resolve_local(path: str, out: Path) -> Path:
    """Event paths are agent paths; locally they usually are already valid."""
    p = Path(path)
    if p.exists():
        return p
    for anc in ("crashes", "hangs", "deadlines"):
        if anc in p.parts:
            return out / Path(*p.parts[p.parts.index(anc):])
    return p


class LocalCampaign:
    def __init__(self, repo_root: str | Path, out_dir: str | Path, *, seconds: int = 30, coverage: bool = True,
                 deadline_us: int = 5000, hang_ms: int = 1000, seed: int | None = None, dicts=None,
                 minimize: bool = True, corpus: str | None = None, runner: Runner = run_subprocess):
        self.root = Path(repo_root)
        self.tdir = self.root / "target"
        self.build_dir = self.tdir / "build"
        self.out = Path(out_dir)
        self.seconds, self.coverage, self.deadline_us, self.hang_ms = seconds, coverage, deadline_us, hang_ms
        self.seed, self.minimize, self.runner = seed, minimize, runner
        self.corpus = corpus
        self.dicts = [str(d) for d in (dicts if dicts is not None else [self.tdir / "examples" / "buggy_rm.dict"])]
        self.sock = str(Path(tempfile.gettempdir()) / f"resfuzz_mock_{abs(hash(str(self.out))) % 10**8}.sock")

    @property
    def agent(self) -> str:
        return str(self.build_dir / "resfuzz-agent")

    @property
    def spawn(self) -> str:
        mock = self.build_dir / ("mock_rm" if self.coverage else "mock_rm_blind")
        return f"{mock} {self.sock}"

    def build(self) -> Result:
        return self.runner(["make", "-C", str(self.tdir), "linux"], timeout=600)

    def fuzz(self) -> Result:
        self.out.mkdir(parents=True, exist_ok=True)
        argv = [self.agent] + agent_args(self.sock, str(self.out), spawn=self.spawn, cov=self.coverage,
                                         dicts=self.dicts, seconds=self.seconds, deadline_us=self.deadline_us,
                                         hang_ms=self.hang_ms, seed=self.seed, corpus=self.corpus)
        return self.runner(argv, timeout=self.seconds + 120)

    def minimize_finding(self, f: Finding) -> None:
        if not f.files or f.type not in ("crash", "hang") or not f.deterministic:
            return
        src = str(_resolve_local(f.replay_file, self.out))
        dst = str(Path(src).with_suffix(".min.rfz"))
        argv = [self.agent, "--target", self.sock, "--spawn", self.spawn, "--out", str(self.out / "minimize"),
                "--hang-ms", str(self.hang_ms), "--minimize", src, "--min-out", dst]
        r = self.runner(argv, timeout=300)
        if r.ok:
            try:
                f.minimized = load_any(dst)
            except (OSError, FormatError):
                pass

    def triage(self) -> CampaignResult:
        events, bad = load_events(self.out / "events.jsonl")
        findings = group_findings(events, self.out, load_dict_names(self.dicts))
        if self.minimize:
            for f in findings:
                self.minimize_finding(f)
        return CampaignResult(self.out, findings, _load_stats(self.out), malformed_events=bad)

    def report(self, res: CampaignResult) -> CampaignResult:
        res.report_md, res.report_html = write_reports(
            self.out, res.findings, res.stats, {"target": self.sock, "spawn": self.spawn})
        return res

    def run_all(self, build: bool = True) -> CampaignResult:
        if build:
            b = self.build()
            if not b.ok:
                raise RuntimeError(f"build failed:\n{b.stdout}\n{b.stderr}")
        r = self.fuzz()
        if r.returncode not in (0,):
            raise RuntimeError(f"agent failed rc={r.returncode}:\n{r.stderr[-2000:]}")
        return self.report(self.triage())


class QemuCampaign:
    """Host side of a QNX campaign. `target` may be injected (tests use a fake)."""

    def __init__(self, cfg: Config, *, image: MkQnxImage | None = None, target: Target | None = None,
                 runner: Runner = run_subprocess, gdb: str | None = None, check_env: bool = True):
        self.cfg = cfg
        self.runner = runner
        self.image = image or MkQnxImage(cfg.image, runner=runner)
        self.target = target
        self.gdb = gdb or cfg.target.gdb
        self.check_env = check_env
        self.out = Path(cfg.out_dir)
        self.log: list[str] = []

    # ---- remote paths ----
    @property
    def rdir(self) -> str:
        return self.cfg.target.deploy_dir

    @property
    def rout(self) -> str:
        return f"{self.rdir}/out"

    def remote_dicts(self) -> list[str]:
        return [f"{self.rdir}/{Path(d).name}" for d in self.cfg.target.dicts]

    def _must(self, r: Result, what: str) -> Result:
        self.log.append(f"{what}: rc={r.returncode}")
        if not r.ok:
            raise RuntimeError(f"{what} failed (rc={r.returncode}): {r.stderr.strip()[:500]}")
        return r

    # ---- steps ----
    def bring_up(self, build: bool = True) -> str:
        if self.check_env:
            check_sdk(self.image.exe)
        if build:
            self._must(self.image.build(), "mkqnximage --build")
        self._must(self.image.start(headless=True), "mkqnximage --run=-h")
        ip = self.image.get_ip()
        if self.target is None:
            self.target = SshTarget(self.cfg.ssh.host or ip, self.cfg.ssh, self.cfg.ssh.identity, self.runner)
        return ip

    def deploy(self) -> None:
        t, q = self.target, shlex.quote
        # Fresh result dir: the agent appends to events.jsonl, and /data persists
        # across reboots, so a stale out/ would mix two campaigns (seen on QNX).
        self._must(t.run(f"mkdir -p {q(self.rdir)} {q(self.cfg.target.dump_dir)} && rm -rf {q(self.rout)} "
                         f"&& mkdir -p {q(self.rout)}"), "mkdir")
        files = [self.cfg.target.agent, self.cfg.target.resmgr] + list(self.cfg.target.dicts)
        for f in files:
            self._must(t.put(f, f"{self.rdir}/{Path(f).name}"), f"scp {Path(f).name}")
        self._must(t.run(f"chmod 755 {q(self.rdir)}/{q(Path(self.cfg.target.agent).name)} "
                         f"{q(self.rdir)}/{q(Path(self.cfg.target.resmgr).name)}"), "chmod")
        # dumper writes cores for signalled processes; -d sets the directory
        # (utilities/topic/d/dumper.html). mkqnximage images already run one
        # (writing to /var/dumps); see TODO-QNX T1 for how a second one behaves.
        t.run(f"dumper -d {q(self.cfg.target.dump_dir)} </dev/null >/dev/null 2>&1 &")

    def agent_cmd(self, *extra: str) -> str:
        fz = self.cfg.fuzz
        args = agent_args(self.cfg.target.path, self.rout, spawn=self.cfg.target.spawn, cov=fz.coverage,
                          dicts=self.remote_dicts(), seconds=fz.seconds, deadline_us=fz.deadline_us,
                          hang_ms=fz.hang_ms, seed=fz.seed, no_raw=fz.no_raw)
        agent = f"{self.rdir}/{Path(self.cfg.target.agent).name}"
        return " ".join(shlex.quote(x) for x in [agent] + args + list(fz.agent_args) + list(extra))

    def fuzz(self) -> Result:
        return self.target.run(self.agent_cmd(), timeout=self.cfg.fuzz.seconds + 300)

    def fetch(self) -> None:
        self.out.mkdir(parents=True, exist_ok=True)
        self._must(self.target.get(f"{self.rout}/.", str(self.out), recursive=True), "fetch results")

    def replay_for_core(self, f: Finding) -> None:
        """Replay one finding on a clean dump dir, then fetch whatever core
        appears: attribution by construction, no assumptions about dumper's
        file naming."""
        if not f.files or f.type == "deadline":
            return
        t, q = self.target, shlex.quote
        remote_case = f.replay_file or f.files[0]
        dd = self.cfg.target.dump_dir
        t.run(f"rm -f {q(dd)}/*")
        agent = f"{self.rdir}/{Path(self.cfg.target.agent).name}"
        spawn = ["--spawn", self.cfg.target.spawn] if self.cfg.target.spawn else []
        replay = [agent, "--target", self.cfg.target.path, *spawn, "--out", f"{self.rdir}/replay",
                  "--hang-ms", str(self.cfg.fuzz.hang_ms), *self.cfg.fuzz.agent_args, "--replay", remote_case]
        r = t.run(" ".join(q(x) for x in replay), timeout=120)
        self.log.append(f"replay {f.bucket}: {EXIT_CODES.get(r.returncode, r.returncode)}")
        # fetch cores BEFORE minimising: minimisation crashes the server again
        cores = self.out / "cores" / f.bucket
        cores.mkdir(parents=True, exist_ok=True)
        # A core only belongs to this finding if the replay crashed or hung;
        # otherwise it is the agent's own SIGABRT at shutdown.
        replay_failed = EXIT_CODES.get(r.returncode) in ("crash", "hang")
        if replay_failed and t.get(f"{dd}/.", str(cores), recursive=True).ok:
            for core in sorted(p for p in cores.iterdir() if not p.name.endswith(".bt.txt")):
                bt = self.runner(gdb_argv(self.cfg.target.resmgr, str(core), self.gdb), timeout=120)
                if bt.ok:
                    f.frames = parse_backtrace(bt.stdout)
                    (cores / (core.name + ".bt.txt")).write_text(bt.stdout)
                    break
        if f.type in ("crash", "hang") and self.cfg.target.spawn and f.deterministic:
            mn = remote_case + ".min"
            m = t.run(" ".join(q(x) for x in replay[:-2] + ["--minimize", remote_case, "--min-out", mn]), timeout=900)
            if m.ok:
                local = self.out / "min" / (Path(remote_case).name + ".min")
                local.parent.mkdir(parents=True, exist_ok=True)
                if t.get(mn, str(local)).ok:
                    try:
                        f.minimized = load_any(local)
                    except (OSError, FormatError):
                        pass

    def triage(self) -> CampaignResult:
        events, bad = load_events(self.out / "events.jsonl")
        findings = group_findings(events, self.out, load_dict_names(self.cfg.target.dicts))
        for f in findings:
            self.replay_for_core(f)
        return CampaignResult(self.out, findings, _load_stats(self.out), malformed_events=bad)

    def report(self, res: CampaignResult) -> CampaignResult:
        res.report_md, res.report_html = write_reports(
            self.out, res.findings, res.stats, {"target": self.cfg.target.path, "spawn": self.cfg.target.spawn})
        return res

    def run_all(self, build_image: bool = True) -> CampaignResult:
        self.bring_up(build=build_image)
        self.deploy()
        r = self.fuzz()
        self.log.append(f"agent exit {r.returncode}")
        self.fetch()
        return self.report(self.triage())
