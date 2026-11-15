import io

import pytest

from resfuzz.config import ImageConfig, SshConfig
from resfuzz.control import SerialTarget, SshTarget
from resfuzz.qemu import MkQnxImage, SdkMissing, check_sdk
from resfuzz.runner import RecordingRunner, Result, run_subprocess


def test_check_sdk_missing():
    with pytest.raises(SdkMissing):
        check_sdk(env={})


def test_mkqnximage_argv(tmp_path):
    rr = RecordingRunner()
    img = MkQnxImage(ImageConfig(workdir=str(tmp_path), ssh_ident="/k.pub", assumed_ip="10.0.2.15"), runner=rr)
    img.build(clean=True)
    img.start()
    argv0, kw0 = rr.calls[0]["argv"], rr.calls[0]
    assert argv0[:3] == ["mkqnximage", "--type=qemu", "--arch=x86_64"]
    assert "--ssh-ident=/k.pub" in argv0 and "--build" in argv0 and "--clean" in argv0
    assert kw0["cwd"] == str(tmp_path)
    assert "--run=-h" in rr.calls[1]["argv"]


def test_getip_parses_and_falls_back(tmp_path):
    rr = RecordingRunner([(("--getip",), Result(0, "booting...\n192.168.122.45\n", ""))])
    assert MkQnxImage(ImageConfig(workdir=str(tmp_path)), runner=rr).get_ip() == "192.168.122.45"
    assert MkQnxImage(ImageConfig(workdir=str(tmp_path), assumed_ip="10.1.1.1"), runner=rr).get_ip() == "10.1.1.1"
    rr2 = RecordingRunner([(("--getip",), Result(1, "", "err"))])
    with pytest.raises(RuntimeError):
        MkQnxImage(ImageConfig(workdir=str(tmp_path)), runner=rr2).get_ip()


def test_ssh_argv_quoting():
    rr = RecordingRunner()
    t = SshTarget("10.0.0.5", SshConfig(port=2222), "/id", rr)
    t.run("echo 'hi there'")
    t.put("a b.bin", "/data/x")
    t.get("/data/out/.", "local", recursive=True)
    ssh, scp_put, scp_get = (c["argv"] for c in rr.calls)
    assert ssh[0] == "ssh" and "-p" in ssh and "2222" in ssh and ssh[-1] == "echo 'hi there'"
    assert "root@10.0.0.5" in ssh and "-i" in ssh
    assert scp_put[0] == "scp" and "-P" in scp_put and scp_put[-2:] == ["a b.bin", "root@10.0.0.5:/data/x"]
    assert "-r" in scp_get


def test_serial_wrap_unwrap():
    raw = SerialTarget.wrap("ls /dev", "RF1")
    assert raw == b'ls /dev; echo RF""1:$?\n' and b"RF1:" not in raw
    # console echoes the command, prints output, then the sentinel with status
    r = SerialTarget.unwrap(raw + b"null\r\nbuggy\r\nRF1:3\r\n", "RF1")
    assert r.returncode == 3 and "buggy" in r.stdout and "echo" not in r.stdout
    assert SerialTarget.unwrap(b"partial output", "RF1").returncode == 124


def test_run_subprocess_missing_and_timeout():
    assert run_subprocess(["/nonexistent/binary"]).returncode == 127
    assert run_subprocess(["sleep", "5"], timeout=0.2).returncode == 124
