import shutil
import subprocess
from pathlib import Path

import pytest

from resfuzz.dictgen import (build_dictionary, parse_header, parse_resolved_dict, parse_suggestions, resolver_source,
                             seeds_to_testcases, suggest_seeds, token_lines)
from resfuzz.testcase import KIND

REPO = Path(__file__).resolve().parents[2]
HEADER = REPO / "target" / "buggy_rm" / "buggy_rm.h"
DICT = REPO / "target" / "examples" / "buggy_rm.dict"


def norm(text):
    return sorted(" ".join(l.split()) for l in text.splitlines() if l.startswith("dcmd "))


def test_parse_buggy_header():
    info = parse_header(HEADER.read_text())
    assert [c.name for c in info.commands] == [
        "DCMD_BUGGY_SET_MODE", "DCMD_BUGGY_SET_NAME", "DCMD_BUGGY_GET_NAME",
        "DCMD_BUGGY_GET_STATS", "DCMD_BUGGY_COMPUTE", "DCMD_BUGGY_RESET"]
    assert info.commands[3].direction == "tofrom" and info.commands[3].type == "struct buggy_stats"
    assert info.commands[5].type is None
    assert info.numbers["BUGGY_MAGIC_ADMIN"] == 0x4E4D4441
    assert info.strings["BUGGY_PATH"] == "/dev/buggy"
    assert 'buggy_magic_admin="ADMN"' in token_lines(info)


def test_line_continuations_and_comments():
    info = parse_header('#define DCMD_X __DIOTF(_DCMD_MISC, \\\n   7, struct x) /* the x */\n'
                        '#define DCMD_Y __DION(_DCMD_MISC, 8) // y\n#define NOT_A_CMD foo(1)\n')
    assert [(c.name, c.cmd, c.comment) for c in info.commands] == [("DCMD_X", "7", "the x"), ("DCMD_Y", "8", "y")]


@pytest.mark.skipif(not shutil.which("gcc"), reason="gcc not available")
def test_resolver_matches_hand_dictionary(tmp_path):
    """The generated resolver, compiled against the Linux stub <devctl.h>,
    reproduces the hand-written dictionary exactly."""
    info = parse_header(HEADER.read_text())
    src = tmp_path / "resolver.c"
    src.write_text(resolver_source("buggy_rm.h", info))
    exe = tmp_path / "resolver"
    r = subprocess.run(["gcc", "-Wall", "-Werror", "-I", str(REPO / "target/platform/linux_compat"),
                        "-I", str(HEADER.parent), "-o", str(exe), str(src)], capture_output=True, text=True)
    assert r.returncode == 0, r.stderr
    out = subprocess.run([str(exe)], capture_output=True, text=True, check=True).stdout
    assert norm(out) == norm(DICT.read_text())
    d = build_dictionary(HEADER.read_text(), out)
    assert norm(d) == norm(DICT.read_text()) and 'buggy_magic_admin="ADMN"' in d


class FakeLLM:
    def __init__(self, reply):
        self.reply, self.prompts = reply, []

    def complete(self, system, user):
        self.prompts.append((system, user))
        return self.reply


def test_ai_suggestions_are_validated():
    info = parse_header(HEADER.read_text())
    reply = """Sure! ```json
    {"seeds": [
      {"sequence": [{"dcmd": "DCMD_BUGGY_SET_MODE", "payload_hex": "41444d4e02000000"},
                    {"dcmd": "DCMD_BUGGY_SET_NAME", "payload_hex": "41", "claimed_len": 64}],
       "rationale": "admin then long name"},
      {"sequence": [{"dcmd": "DCMD_MADE_UP", "payload_hex": "00"}]},
      {"sequence": [{"dcmd": "DCMD_BUGGY_RESET", "payload_hex": "zz"}]},
      {"sequence": [{"dcmd": "DCMD_BUGGY_GET_STATS", "payload_hex": "07000000"}]}
    ]}```"""
    llm = FakeLLM(reply)
    seqs = suggest_seeds(HEADER.read_text(), info, llm)
    assert "DCMD_BUGGY_COMPUTE" in llm.prompts[0][1]
    assert [[s.dcmd for s in q] for q in seqs] == [["DCMD_BUGGY_SET_MODE", "DCMD_BUGGY_SET_NAME"],
                                                  ["DCMD_BUGGY_GET_STATS"]]
    tcs = seeds_to_testcases(seqs, parse_resolved_dict(DICT.read_text()))
    first = tcs[0].ops
    assert first[0].kind == KIND["DEVCTL"] and first[0].a == 0x40080D40 and first[0].data == b"ADMN\x02\0\0\0"
    assert first[1].kind == KIND["DEVCTL_RAW"] and first[1].b == 64
    assert parse_suggestions("no json here", info) == []
    for bad in ('{"seeds": 5}', '{"seeds": [{"sequence": 3}]}', '{"seeds": [{"sequence": [7]}]}', '[1]'):
        assert parse_suggestions(bad, info) == []
