# SPDX-License-Identifier: Apache-2.0
"""RFZ1 test-case codec, byte-for-byte compatible with target/src/testcase.c.

Layout (little-endian), see docs/DESIGN.md:
    header  : b"RFZ1" | u16 nops | u16 reserved(=0)
    op      : u8 kind | u8 flags | u16 len | u32 a | u32 b | payload[len]
"""
from __future__ import annotations

import struct
from dataclasses import dataclass, field

MAGIC = b"RFZ1"
HDR = struct.Struct("<4sHH")
OPH = struct.Struct("<BBHII")
MAX_OPS = 64
MAX_PAYLOAD = 4096
OP_KINDS = ["OPEN", "CLOSE", "READ", "WRITE", "DEVCTL", "DEVCTL_RAW", "LSEEK", "MSG_RAW", "SLEEP"]
KIND = {name: i for i, name in enumerate(OP_KINDS)}


class FormatError(ValueError):
    pass


@dataclass
class Op:
    kind: int
    flags: int = 0
    a: int = 0
    b: int = 0
    data: bytes = b""

    @property
    def kind_name(self) -> str:
        return OP_KINDS[self.kind] if 0 <= self.kind < len(OP_KINDS) else f"?{self.kind}"

    @property
    def slot(self) -> int:
        return self.flags & 3

    def describe(self) -> str:
        s = f"slot{self.slot} {self.kind_name:<10} a=0x{self.a:08x} b=0x{self.b:08x} len={len(self.data)}"
        if self.data:
            hx = self.data[:32].hex()
            s += f" data={hx}{'...' if len(self.data) > 32 else ''}"
        return s


@dataclass
class TestCase:
    __test__ = False  # not a pytest class
    ops: list[Op] = field(default_factory=list)

    def serialize(self) -> bytes:
        if len(self.ops) > MAX_OPS:
            raise FormatError("too many ops")
        out = [HDR.pack(MAGIC, len(self.ops), 0)]
        for op in self.ops:
            if len(op.data) > MAX_PAYLOAD:
                raise FormatError("payload too large")
            out.append(OPH.pack(op.kind, op.flags, len(op.data), op.a & 0xFFFFFFFF, op.b & 0xFFFFFFFF))
            out.append(op.data)
        return b"".join(out)

    @classmethod
    def parse(cls, buf: bytes) -> "TestCase":
        if len(buf) < HDR.size:
            raise FormatError("truncated header")
        magic, nops, reserved = HDR.unpack_from(buf, 0)
        if magic != MAGIC:
            raise FormatError("bad magic")
        if nops > MAX_OPS or reserved != 0:
            raise FormatError("bad header")
        off = HDR.size
        ops = []
        for _ in range(nops):
            if len(buf) - off < OPH.size:
                raise FormatError("truncated op header")
            kind, flags, plen, a, b = OPH.unpack_from(buf, off)
            off += OPH.size
            if kind >= len(OP_KINDS) or plen > MAX_PAYLOAD or len(buf) - off < plen:
                raise FormatError("bad op")
            ops.append(Op(kind, flags, a, b, bytes(buf[off:off + plen])))
            off += plen
        if off != len(buf):
            raise FormatError("trailing bytes")
        return cls(ops)

    @classmethod
    def load(cls, path) -> "TestCase":
        with open(path, "rb") as f:
            return cls.parse(f.read())

    def save(self, path) -> None:
        with open(path, "wb") as f:
            f.write(self.serialize())

    def describe(self) -> str:
        return "\n".join(f"#{i:<3} {op.describe()}" for i, op in enumerate(self.ops))


SEQ_MAGIC = b"RFS1"
SEQ_MAX = 4096


@dataclass
class Sequence:
    """RFS1: the test cases one server instance ran, in order, ending with the
    failing one (target/include/rf_testcase.h). Replayed as a whole against a
    fresh server, this reproduces findings that depend on server state."""
    __test__ = False
    cases: list[TestCase] = field(default_factory=list)

    def serialize(self) -> bytes:
        if not self.cases or len(self.cases) > SEQ_MAX:
            raise FormatError("bad case count")
        out = [SEQ_MAGIC, struct.pack("<I", len(self.cases))]
        for tc in self.cases:
            b = tc.serialize()
            out += [struct.pack("<I", len(b)), b]
        return b"".join(out)

    @classmethod
    def parse(cls, buf: bytes) -> "Sequence":
        if len(buf) < 8 or buf[:4] != SEQ_MAGIC:
            raise FormatError("bad sequence magic")
        (n,) = struct.unpack_from("<I", buf, 4)
        if n == 0 or n > SEQ_MAX:
            raise FormatError("bad case count")
        off, cases = 8, []
        for _ in range(n):
            if len(buf) - off < 4:
                raise FormatError("truncated sequence")
            (sz,) = struct.unpack_from("<I", buf, off)
            off += 4
            if sz > len(buf) - off:
                raise FormatError("truncated case")
            cases.append(TestCase.parse(buf[off:off + sz]))
            off += sz
        if off != len(buf):
            raise FormatError("trailing bytes")
        return cls(cases)

    @classmethod
    def load(cls, path) -> "Sequence":
        with open(path, "rb") as f:
            return cls.parse(f.read())

    def save(self, path) -> None:
        with open(path, "wb") as f:
            f.write(self.serialize())

    @property
    def ops(self) -> list[Op]:
        return [op for tc in self.cases for op in tc.ops]

    def describe(self, tail: int = 3) -> str:
        n = len(self.cases)
        parts = [f"sequence of {n} test cases (one server instance); last {min(tail, n)}:"]
        for i in range(max(0, n - tail), n):
            parts.append(f"-- case {i}" + (" (fails)" if i == n - 1 else ""))
            parts.append(self.cases[i].describe())
        return "\n".join(parts)


def load_any(path) -> "TestCase | Sequence":
    with open(path, "rb") as f:
        buf = f.read()
    return Sequence.parse(buf) if buf[:4] == SEQ_MAGIC else TestCase.parse(buf)
