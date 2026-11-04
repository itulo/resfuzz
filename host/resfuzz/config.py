# SPDX-License-Identifier: Apache-2.0
"""Campaign configuration (TOML). See examples/campaign.toml."""
from __future__ import annotations

import tomllib
from dataclasses import dataclass, field
from pathlib import Path


class ConfigError(ValueError):
    pass


@dataclass
class ImageConfig:
    workdir: str = "qnx-image"          # mkqnximage operates on the current directory
    arch: str = "x86_64"                # mkqnximage --arch = x86_64 | aarch64le
    ssh_ident: str | None = None        # mkqnximage --ssh-ident=path
    assumed_ip: str | None = None       # mkqnximage --assumed-ip=ip
    extra_args: list[str] = field(default_factory=list)


@dataclass
class TargetConfig:
    resmgr: str = "target/build-qnx/buggy_rm"      # host path of the server binary to deploy
    agent: str = "target/build-qnx/resfuzz-agent"  # host path of the agent binary
    path: str = "/dev/buggy"                        # what the agent opens on the target
    spawn: str | None = "/data/resfuzz/buggy_rm"   # agent restarts the server itself
    deploy_dir: str = "/data/resfuzz"              # /data is always read-write on mkqnximage images
    dicts: list[str] = field(default_factory=list)
    dump_dir: str = "/data/resfuzz/cores"          # where dumper is told to write (-d)


@dataclass
class FuzzConfig:
    seconds: int = 600
    deadline_us: int = 5000
    hang_ms: int = 1000
    coverage: bool = True
    seed: int | None = None
    no_raw: bool = False


@dataclass
class SshConfig:
    user: str = "root"
    host: str | None = None             # None: ask mkqnximage --getip
    port: int = 22
    identity: str | None = None         # private key for ssh -i (mkqnximage --ssh-ident is separate)
    options: list[str] = field(default_factory=lambda: [
        "-o", "StrictHostKeyChecking=no", "-o", "UserKnownHostsFile=/dev/null", "-o", "LogLevel=ERROR"])


@dataclass
class Config:
    image: ImageConfig = field(default_factory=ImageConfig)
    target: TargetConfig = field(default_factory=TargetConfig)
    fuzz: FuzzConfig = field(default_factory=FuzzConfig)
    ssh: SshConfig = field(default_factory=SshConfig)
    out_dir: str = "resfuzz-results"


_SECTIONS = {"image": ImageConfig, "target": TargetConfig, "fuzz": FuzzConfig, "ssh": SshConfig}


def from_dict(d: dict) -> Config:
    cfg = Config()
    for key, value in d.items():
        if key == "out_dir":
            cfg.out_dir = str(value)
            continue
        if key not in _SECTIONS:
            raise ConfigError(f"unknown section [{key}]")
        if not isinstance(value, dict):
            raise ConfigError(f"[{key}] must be a table")
        section = getattr(cfg, key)
        for k, v in value.items():
            if not hasattr(section, k):
                raise ConfigError(f"unknown key {key}.{k}")
            default = getattr(section, k)
            if isinstance(default, bool) and not isinstance(v, bool):
                raise ConfigError(f"{key}.{k} must be a boolean")
            if isinstance(default, int) and not isinstance(default, bool) and not isinstance(v, int):
                raise ConfigError(f"{key}.{k} must be an integer")
            setattr(section, k, v)
    if cfg.image.arch not in ("x86_64", "aarch64le"):
        raise ConfigError("image.arch must be x86_64 or aarch64le (mkqnximage --arch)")
    if cfg.fuzz.coverage and not cfg.target.spawn:
        raise ConfigError("fuzz.coverage needs target.spawn (the server maps the coverage shm at start)")
    return cfg


def load(path: str | Path) -> Config:
    with open(path, "rb") as f:
        return from_dict(tomllib.load(f))
