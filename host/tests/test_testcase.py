import subprocess

import pytest

from resfuzz.testcase import KIND, MAX_PAYLOAD, FormatError, Op, TestCase


def sample():
    return TestCase([Op(KIND["OPEN"], 1), Op(KIND["DEVCTL"], 1, 0xC0100D43, 16, bytes(range(16))),
                     Op(KIND["WRITE"], 0, 0, 0, b"reset\n"), Op(KIND["SLEEP"], 0, 5, 0)])


def test_roundtrip():
    tc = sample()
    assert TestCase.parse(tc.serialize()) == tc


@pytest.mark.parametrize("mutate", [
    lambda b: b"XXXX" + b[4:],                 # bad magic
    lambda b: b[:-1],                          # truncated payload
    lambda b: b + b"\0",                       # trailing garbage
    lambda b: b[:6] + b"\1\0" + b[8:],         # reserved field
    lambda b: b[:3],                           # short header
])
def test_rejects_malformed(mutate):
    with pytest.raises(FormatError):
        TestCase.parse(mutate(sample().serialize()))


def test_rejects_oversize_payload():
    with pytest.raises(FormatError):
        TestCase([Op(KIND["WRITE"], data=b"x" * (MAX_PAYLOAD + 1))]).serialize()


def test_describe_mentions_kinds():
    d = sample().describe()
    assert "DEVCTL" in d and "SLEEP" in d and "0xc0100d43" in d


def test_cross_language_decode(built_target, tmp_path):
    """Python-encoded case must decode identically in the C agent."""
    p = tmp_path / "x.rfz"
    sample().save(p)
    r = subprocess.run([str(built_target / "resfuzz-agent"), "--decode", str(p)], capture_output=True, text=True)
    assert r.returncode == 0, r.stderr
    c_lines = [" ".join(l.split()) for l in r.stdout.splitlines()[1:]]
    py_lines = [" ".join(l.split()) for l in sample().describe().splitlines()]
    assert c_lines == py_lines


def test_c_written_case_parses(built_target, tmp_path):
    """A case produced by the C fuzzer loop parses in Python (and round-trips)."""
    out = tmp_path / "o"
    sock = tmp_path / "s.sock"
    r = subprocess.run([str(built_target / "resfuzz-agent"), "--target", str(sock),
                        "--spawn", f"{built_target / 'mock_rm_blind'} {sock}", "--out", str(out),
                        "--iterations", "300", "--seed", "3", "--quiet"], capture_output=True, text=True, timeout=120)
    assert r.returncode == 0, r.stderr
    files = list((out / "corpus").glob("*.rfz"))
    assert files
    for f in files:
        raw = f.read_bytes()
        assert TestCase.parse(raw).serialize() == raw
