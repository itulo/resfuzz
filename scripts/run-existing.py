#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Run a resfuzz campaign on an already-booted QNX QEMU target.

Does not rebuild the image or restart QEMU (so you can chain campaigns).
Optional --pre / --post run on the guest around deploy+fuzz.
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO / "host"))

from resfuzz.campaign import QemuCampaign  # noqa: E402
from resfuzz.config import load  # noqa: E402
from resfuzz.control import SshTarget  # noqa: E402
from resfuzz.triage import summarize  # noqa: E402


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("-c", "--config", required=True)
    p.add_argument("--host", required=True, help="guest IPv4 (from qnx-up.sh)")
    p.add_argument("--identity", default=str(Path.home() / ".ssh/qnx_resfuzz_ed25519"))
    p.add_argument("--seconds", type=int, default=None, help="override fuzz.seconds")
    p.add_argument("--pre", default=None, help="shell command on the guest before deploy")
    p.add_argument("--post", default=None, help="shell command on the guest after triage")
    p.add_argument("--extra", action="append", default=[], help="extra host file to scp into deploy_dir")
    args = p.parse_args()

    cfg = load(args.config)
    if args.seconds is not None:
        cfg.fuzz.seconds = args.seconds
    cfg.ssh.host = args.host
    cfg.ssh.identity = args.identity
    cfg.image.assumed_ip = args.host

    camp = QemuCampaign(cfg)
    camp.target = SshTarget(args.host, cfg.ssh, args.identity)
    t = camp.target

    if args.pre:
        r = t.run(args.pre, timeout=60)
        print(f"pre: rc={r.returncode} {r.stdout[-400:]}{r.stderr[-200:]}", flush=True)
        if not r.ok:
            print("pre failed", file=sys.stderr)
            return 1

    camp.deploy()
    for extra in args.extra:
        dest = f"{cfg.target.deploy_dir}/{Path(extra).name}"
        r = t.put(extra, dest)
        if not r.ok:
            print(f"scp extra {extra} failed: {r.stderr}", file=sys.stderr)
            return 1
        t.run(f"chmod 755 {dest}")
        camp.log.append(f"extra {Path(extra).name}")

    print("fuzz:", camp.agent_cmd(), flush=True)
    r = camp.fuzz()
    camp.log.append(f"agent exit {r.returncode}")
    print(f"agent exit {r.returncode}\n{r.stdout[-2000:]}\n{r.stderr[-1000:]}", flush=True)
    camp.fetch()
    res = camp.report(camp.triage())
    if args.post:
        pr = t.run(args.post, timeout=60)
        print(f"post: rc={pr.returncode} {pr.stdout[-400:]}{pr.stderr[-200:]}", flush=True)

    print("\n".join(camp.log))
    print(json.dumps({"summary": summarize(res.findings), "report": str(res.report_html),
                      "out": str(res.out_dir), "stats": res.stats}, indent=2, default=str))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
