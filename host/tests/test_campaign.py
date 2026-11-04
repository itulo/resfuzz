import json
import shutil
from pathlib import Path

from resfuzz.campaign import LocalCampaign, QemuCampaign, agent_args
from resfuzz.cli import main
from resfuzz.config import Config
from resfuzz.runner import RecordingRunner, Result
from resfuzz.testcase import KIND, Op, TestCase

REPO = Path(__file__).resolve().parents[2]


def test_agent_args():
    a = agent_args("/dev/buggy", "/o", spawn="/x y", cov=True, dicts=["d1"], seconds=5, seed=9, no_raw=True)
    assert a[:2] == ["--target", "/dev/buggy"] and "--cov" in a and "--no-raw" in a
    assert a[a.index("--spawn") + 1] == "/x y" and a[a.index("--seed") + 1] == "9"


def test_local_campaign_end_to_end(built_target, tmp_path):
    camp = LocalCampaign(REPO, tmp_path / "out", seconds=8, seed=1, coverage=True)
    res = camp.run_all(build=False)
    assert res.stats["execs"] > 1000
    crashes = [f for f in res.findings if f.type == "crash"]
    assert crashes, "expected the GET_STATS NULL deref within 8 s"
    assert any(f.dcmd_name == "DCMD_BUGGY_GET_STATS" and f.signal == 11 for f in crashes)
    assert any(f.minimized is not None and len(f.minimized.ops) <= 2 for f in crashes)
    assert res.report_html.exists() and "GET_STATS" in res.report_md.read_text()


class FakeQnx:
    """Pretends to be the ssh target: records commands, serves canned results."""

    def __init__(self, tmp):
        self.cmds, self.puts, self.gets, self.tmp = [], [], [], tmp

    def run(self, cmd, timeout=None):
        self.cmds.append(cmd)
        return Result(10 if "--replay" in cmd else 0)

    def put(self, local, remote):
        self.puts.append((local, remote))
        return Result(0)

    def get(self, remote, local, recursive=False):
        self.gets.append(remote)
        local = Path(local)
        if remote.endswith("/out/."):
            (local / "crashes").mkdir(parents=True, exist_ok=True)
            TestCase([Op(KIND["DEVCTL"], 0, 1, 0)]).save(local / "crashes" / "crash_ab_0.rfz")
            ev = {"type": "crash", "bucket": "ab", "op": "DEVCTL", "dcmd": "00000001", "signal": 11,
                  "file": "/data/resfuzz/out/crashes/crash_ab_0.rfz", "reproducible": True}
            (local / "events.jsonl").write_text(json.dumps(ev) + "\n")
            (local / "stats.json").write_text('{"execs": 5}')
        elif "cores" in remote:
            (local / "buggy_rm.core").write_bytes(b"core")
        elif remote.endswith(".min"):
            TestCase([Op(KIND["DEVCTL"], 0, 1, 0)]).save(local)
        return Result(0)


def test_qemu_campaign_with_fakes(tmp_path):
    cfg = Config()
    cfg.out_dir = str(tmp_path / "res")
    cfg.target.dicts = [str(REPO / "target/examples/buggy_rm.dict")]
    cfg.fuzz.seconds = 1
    cfg.image.workdir = str(tmp_path / "img")
    bt = "#0  0x1 in buggy_devctl (x=1) at buggy_logic.c:10\n"
    rr = RecordingRunner([(("--getip",), Result(0, "10.0.2.15\n")), (("-batch",), Result(0, bt))])
    fake = FakeQnx(tmp_path)
    camp = QemuCampaign(cfg, target=fake, runner=rr, gdb="ntox86_64-gdb", check_env=False)
    res = camp.run_all()
    argvs = [c["argv"] for c in rr.calls]
    assert any("--build" in a for a in argvs) and any("--run=-h" in a for a in argvs)
    assert fake.cmds[0].startswith("mkdir -p /data/resfuzz")
    assert any("resfuzz-agent" in c and "--cov" in c and "--spawn" in c for c in fake.cmds)
    assert any(c.startswith("dumper -d /data/resfuzz/cores") for c in fake.cmds)
    assert [Path(p[0]).name for p in fake.puts] == ["resfuzz-agent", "buggy_rm", "buggy_rm.dict"]
    # cores are fetched before the minimiser runs (minimisation crashes the server again)
    core_i = next(i for i, g in enumerate(fake.gets) if "cores" in g)
    min_i = next(i for i, g in enumerate(fake.gets) if g.endswith(".min"))
    assert core_i < min_i
    f = res.findings[0]
    assert f.frames[0].func == "buggy_devctl" and f.minimized is not None
    assert any(a[0] == "ntox86_64-gdb" for a in argvs)
    assert "buggy_devctl" in res.report_md.read_text()


def test_cli_without_sdk_fails_cleanly(monkeypatch, capsys):
    monkeypatch.delenv("QNX_HOST", raising=False)
    monkeypatch.delenv("QNX_TARGET", raising=False)
    assert main(["image", "ip"]) == 2
    assert "QNX SDP 8.0" in capsys.readouterr().err


def test_cli_triage_report_decode(tmp_path, capsys):
    out = tmp_path / "o"
    (out / "crashes").mkdir(parents=True)
    TestCase([Op(KIND["READ"], 0, 0, 16)]).save(out / "crashes" / "c.rfz")
    (out / "events.jsonl").write_text(json.dumps({"type": "crash", "bucket": "z", "op": "READ", "signal": 11,
                                                  "file": "crashes/c.rfz"}) + "\n")
    assert main(["triage", str(out)]) == 0
    assert "CRASH in READ" in capsys.readouterr().out
    assert main(["report", str(out), "--target", "/dev/buggy"]) == 0
    assert (out / "report.html").exists()
    assert main(["decode", str(out / "crashes" / "c.rfz")]) == 0


def test_cli_dictgen(tmp_path, capsys):
    hdr = REPO / "target/buggy_rm/buggy_rm.h"
    assert main(["dictgen", str(hdr), "--resolver-out", str(tmp_path / "r.c"), "--out", str(tmp_path / "d")]) == 0
    assert "get_device_size(DCMD_BUGGY_COMPUTE)" in (tmp_path / "r.c").read_text()
    assert "resolver on the target" in (tmp_path / "d").read_text()
    if shutil.which("gcc"):
        pass  # full resolver compile is covered in test_dictgen
