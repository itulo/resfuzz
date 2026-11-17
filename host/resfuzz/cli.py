# SPDX-License-Identifier: Apache-2.0
"""resfuzz command line."""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

from . import __version__
from .campaign import LocalCampaign, QemuCampaign
from .config import Config, ConfigError, load
from .control import SshTarget
from .dictgen import (OpenAICompatClient, build_dictionary, parse_header, parse_resolved_dict, resolver_source,
                      seeds_to_testcases, suggest_seeds, write_seeds)
from .qemu import MkQnxImage, SdkMissing, check_sdk
from .report import write_reports
from .testcase import TestCase
from .triage import group_findings, load_dict_names, load_events, summarize

REPO = Path(__file__).resolve().parents[2]


def _cfg(args) -> Config:
    return load(args.config) if args.config else Config()


def cmd_local(a) -> int:
    camp = LocalCampaign(REPO, a.out, seconds=a.seconds, coverage=not a.no_cov, seed=a.seed,
                         deadline_us=a.deadline_us, hang_ms=a.hang_ms, minimize=not a.no_minimize,
                         dicts=a.dict if a.dict else None, corpus=a.corpus)
    res = camp.run_all(build=not a.no_build)
    print(json.dumps({"summary": summarize(res.findings), "execs": res.stats.get("execs"),
                      "report": str(res.report_html)}, indent=2))
    return 0


def cmd_image(a) -> int:
    cfg = _cfg(a)
    img = MkQnxImage(cfg.image)
    check_sdk(img.exe)
    if a.action == "ip":
        print(img.get_ip())
        return 0
    r = {"build": lambda: img.build(clean=a.clean), "run": img.start, "stop": img.stop}[a.action]()
    sys.stdout.write(r.stdout)
    sys.stderr.write(r.stderr)
    return r.returncode


def cmd_deploy(a) -> int:
    cfg = _cfg(a)
    camp = QemuCampaign(cfg)
    ip = cfg.ssh.host or MkQnxImage(cfg.image).get_ip()
    camp.target = SshTarget(ip, cfg.ssh, cfg.ssh.identity)
    camp.deploy()
    print("\n".join(camp.log))
    return 0


def cmd_run(a) -> int:
    cfg = _cfg(a)
    res = QemuCampaign(cfg).run_all(build_image=not a.no_build)
    print(json.dumps({"summary": summarize(res.findings), "report": str(res.report_html)}, indent=2))
    return 0


def _findings(a):
    events, bad = load_events(Path(a.out) / "events.jsonl")
    return group_findings(events, a.out, load_dict_names(a.dict)), bad


def cmd_triage(a) -> int:
    findings, bad = _findings(a)
    for f in findings:
        rep = {True: "repro", False: "flaky", None: "?"}[f.reproducible]
        print(f"{f.severity:<6} {rep:<5} x{f.count:<5} {f.title}")
    if bad:
        print(f"({bad} malformed event lines ignored)", file=sys.stderr)
    return 0


def cmd_report(a) -> int:
    findings, _ = _findings(a)
    try:
        stats = json.loads((Path(a.out) / "stats.json").read_text())
    except (OSError, json.JSONDecodeError):
        stats = {}
    md, ht = write_reports(a.out, findings, stats, {"target": a.target, "spawn": a.spawn})
    print(md)
    print(ht)
    return 0


def cmd_dictgen(a) -> int:
    text = Path(a.header).read_text()
    info = parse_header(text)
    if a.resolver_out:
        Path(a.resolver_out).write_text(resolver_source(a.header_include or Path(a.header).name, info, a.include))
        print(f"wrote {a.resolver_out}: compile for the target, run it, pass its output via --resolved")
    resolved = Path(a.resolved).read_text() if a.resolved else None
    d = build_dictionary(text, resolved)
    if a.out:
        Path(a.out).write_text(d)
        print(f"wrote {a.out}")
    else:
        sys.stdout.write(d)
    if a.ai:
        client = OpenAICompatClient()
        if not client.configured:
            print("AI step skipped: set RESFUZZ_LLM_BASE_URL and RESFUZZ_LLM_MODEL (and RESFUZZ_LLM_API_KEY)",
                  file=sys.stderr)
            return 1
        if not resolved:
            print("AI step needs --resolved (dcmd values come from the target, never from the model)",
                  file=sys.stderr)
            return 1
        seqs = suggest_seeds(text, info, client)
        paths = write_seeds(seeds_to_testcases(seqs, parse_resolved_dict(resolved)), a.seeds_out)
        print(f"{len(paths)} validated AI seeds in {a.seeds_out}")
    return 0


def cmd_decode(a) -> int:
    from .testcase import load_any
    print(load_any(a.file).describe())    # RFZ1 test case or RFS1 sequence
    return 0


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(prog="resfuzz", description="Fuzz QNX 8.0 resource managers from a Linux host")
    p.add_argument("--version", action="version", version=__version__)
    sub = p.add_subparsers(dest="cmd", required=True)

    s = sub.add_parser("local", help="run the whole loop on Linux against mock_rm (no QNX needed)")
    s.add_argument("--out", default="resfuzz-out")
    s.add_argument("--seconds", type=int, default=30)
    s.add_argument("--seed", type=int)
    s.add_argument("--deadline-us", type=int, default=5000)
    s.add_argument("--hang-ms", type=int, default=1000)
    s.add_argument("--dict", action="append")
    s.add_argument("--no-cov", action="store_true")
    s.add_argument("--no-build", action="store_true")
    s.add_argument("--no-minimize", action="store_true")
    s.add_argument("--corpus", help="directory of .rfz/.seq seeds to load before mutating")
    s.set_defaults(fn=cmd_local)

    for name, fn, hlp in (("deploy", cmd_deploy, "copy agent + resource manager to the QNX target"),
                          ("run", cmd_run, "full QEMU campaign: image, deploy, fuzz, triage, report")):
        s = sub.add_parser(name, help=hlp)
        s.add_argument("--config", "-c")
        if name == "run":
            s.add_argument("--no-build", action="store_true", help="reuse the existing image")
        s.set_defaults(fn=fn)

    s = sub.add_parser("image", help="mkqnximage wrapper")
    s.add_argument("action", choices=["build", "run", "stop", "ip"])
    s.add_argument("--config", "-c")
    s.add_argument("--clean", action="store_true")
    s.set_defaults(fn=cmd_image)

    for name, fn in (("triage", cmd_triage), ("report", cmd_report)):
        s = sub.add_parser(name)
        s.add_argument("out", help="agent output directory (events.jsonl, stats.json, crashes/ ...)")
        s.add_argument("--dict", action="append", default=[])
        if name == "report":
            s.add_argument("--target", default="?")
            s.add_argument("--spawn")
        s.set_defaults(fn=fn)

    s = sub.add_parser("dictgen", help="dictionary (+ optional AI seeds) from a devctl header")
    s.add_argument("header")
    s.add_argument("--resolver-out", help="write the C resolver program here")
    s.add_argument("--header-include", help='how the resolver includes the header, e.g. "<sys/dcmd_chr.h>"')
    s.add_argument("--include", action="append", default=[],
                   help="extra header the resolver includes first (repeatable), e.g. termios.h")
    s.add_argument("--resolved", help="output of the resolver run on the target")
    s.add_argument("--out", help="dictionary file to write (default stdout)")
    s.add_argument("--ai", action="store_true", help="ask an OpenAI-compatible model for seed sequences")
    s.add_argument("--seeds-out", default="ai-seeds")
    s.set_defaults(fn=cmd_dictgen)

    s = sub.add_parser("decode", help="print a .rfz test case")
    s.add_argument("file")
    s.set_defaults(fn=cmd_decode)
    return p


def main(argv=None) -> int:
    a = build_parser().parse_args(argv)
    try:
        return a.fn(a)
    except (SdkMissing, ConfigError, RuntimeError) as e:
        print(f"resfuzz: {e}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
