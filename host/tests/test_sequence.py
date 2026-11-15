# SPDX-License-Identifier: Apache-2.0
"""RFS1 sequences: stateful findings replayed against a fresh server (TODO-QNX F3)."""
import json
import struct
import subprocess
from pathlib import Path

import pytest

from resfuzz.testcase import KIND, FormatError, Op, Sequence, TestCase, load_any
from resfuzz.triage import group_findings

REPO = Path(__file__).resolve().parents[2]
BUILD = REPO / "target" / "build"

# Linux-stub dcmd values (target/examples/buggy_rm.dict)
SET_MODE, SET_NAME, GET_NAME = 0x40080D40, 0x40200D41, 0x80200D42


def _bug1_sequence() -> Sequence:
    admin = TestCase([Op(KIND["DEVCTL"], 0, SET_MODE, 0, struct.pack("<II", 0x4E4D4441, 2))])
    noise = [TestCase([Op(KIND["READ"], 0, 16, 0)]), TestCase([Op(KIND["WRITE"], 0, 0, 0, b"hello")]),
             TestCase([Op(KIND["DEVCTL"], 0, SET_NAME, 0, b"bob")])]
    overflow = TestCase([Op(KIND["DEVCTL_RAW"], 0, SET_NAME, 0x100, b"A" * 64),
                         Op(KIND["DEVCTL"], 0, GET_NAME, 32, b"")])
    return Sequence(noise[:2] + [admin] + [noise[2]] * 5 + [overflow])


def test_sequence_roundtrip_and_rejects(tmp_path):
    seq = _bug1_sequence()
    p = tmp_path / "s.seq"
    seq.save(p)
    back = load_any(p)
    assert isinstance(back, Sequence) and back.serialize() == seq.serialize()
    assert len(back.ops) == sum(len(c.ops) for c in seq.cases)
    assert "case 8 (fails)" in back.describe()
    raw = p.read_bytes()
    for cut in range(0, len(raw), 5):
        with pytest.raises(FormatError):
            Sequence.parse(raw[:cut])
    tc = seq.cases[0]
    tc.save(tmp_path / "one.rfz")
    assert isinstance(load_any(tmp_path / "one.rfz"), TestCase)


def test_group_findings_prefers_reproducing_sequence(tmp_path):
    evs = [{"type": "start", "detail": "platform=qnx"},
           {"type": "crash", "bucket": "b1", "op": "DEVCTL_RAW", "dcmd": "0x80200541", "signal": 11,
            "file": "/x/crashes/c_0.rfz", "reproducible": False,
            "sequence": "/x/crashes/c_0.seq", "sequence_cases": 412, "sequence_reproducible": True}]
    f = group_findings(evs, tmp_path)[0]
    assert f.deterministic and f.replay_file == "/x/crashes/c_0.seq"
    assert f.repro_label == "yes, stateful (412-case sequence)"


@pytest.fixture(scope="module")
def agent():
    r = subprocess.run(["make", "-C", str(REPO / "target"), "linux"], capture_output=True, text=True)
    if r.returncode != 0:
        pytest.skip("cannot build the Linux agent: " + r.stderr[-300:])
    return BUILD / "resfuzz-agent"


def _agent(agent, tmp_path, *args):
    sock = str(tmp_path / "b.sock")
    argv = [str(agent), "--target", sock, "--spawn", f"{BUILD / 'mock_rm_blind'} {sock}",
            "--out", str(tmp_path / "o"), *args]
    return subprocess.run(argv, capture_output=True, text=True, timeout=120)


def test_bug1_is_stateful_and_minimises_to_two_ops(agent, tmp_path):
    seq = _bug1_sequence()
    seq.save(tmp_path / "bug1.seq")
    seq.cases[-1].save(tmp_path / "last.rfz")
    assert _agent(agent, tmp_path, "--replay", str(tmp_path / "last.rfz")).returncode == 0   # alone: harmless
    assert _agent(agent, tmp_path, "--replay", str(tmp_path / "bug1.seq")).returncode == 10  # in sequence: crash
    m = _agent(agent, tmp_path, "--minimize", str(tmp_path / "bug1.seq"), "--min-out", str(tmp_path / "m.rfz"))
    assert m.returncode == 0, m.stderr
    summary = json.loads(next(ln for ln in m.stdout.splitlines() if ln.startswith("{")))
    assert summary["output"] == "testcase" and summary["cases_after"] == 2
    mini = load_any(tmp_path / "m.rfz")
    assert isinstance(mini, TestCase) and len(mini.ops) == 2
    assert [op.a for op in mini.ops] == [SET_MODE, SET_NAME]
    for _ in range(3):   # deterministic on a fresh server every time
        assert _agent(agent, tmp_path, "--replay", str(tmp_path / "m.rfz")).returncode == 10


def test_replay_file_prefers_solo_reproducible(tmp_path):
    from resfuzz.triage import group_findings
    evs = [
        {"type": "hang", "bucket": "b", "op": "DEVCTL_RAW", "dcmd": "0xc00c0544", "t_ms": 1,
         "file": "/x/hangs/hang_b_0.rfz", "reproducible": False,
         "sequence": "/x/hangs/hang_b_0.seq", "sequence_reproducible": True, "sequence_cases": 813},
        {"type": "hang", "bucket": "b", "op": "DEVCTL_RAW", "dcmd": "0xc00c0544", "t_ms": 2,
         "file": "/x/hangs/hang_b_1.rfz", "reproducible": True},
    ]
    (f,) = group_findings(evs, tmp_path)
    assert f.reproducible and f.replay_file == "/x/hangs/hang_b_1.rfz"
    assert f.first_t_ms == 1 and f.repro_label == "yes"
