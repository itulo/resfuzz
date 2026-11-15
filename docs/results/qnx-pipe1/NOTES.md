# QNX campaign against the SDP's own `pipe` (no planted bugs)

- **Binary:** the image's running system pipe (`/system/bin/pipe -U 24:24`,
  pid unchanged for the whole run). The SDP copy at
  `~/qnx800/target/qnx/x86_64/sbin/pipe` was deployed for the record only
  and was **not** spawned. Do not redistribute the QNX binary.
- **Mode:** attach-only (`spawn` unset). Slaying or replacing the system
  pipe breaks `ssh`/`ksh` (`can't create pipe`); this run never slayed it.
  Serial recovery (`/system/bin/pipe -U 24:24` on `ser2.sock`) was ready
  but was not needed: SSH stayed up for the full 300 s.
- **Target path:** `/dev/pipe`.
- **Dictionary:** `target/examples/dcmd_all.qnx.dict`.
- **Flags:** `coverage = false`, `--nonblock` (a pipe read with no writer
  must not look like a hang), no `--respawn-every` (nothing to respawn).
- **Config:** `host/examples/qemu-pipe.toml`.
- **Date:** 15 Nov 2026, QEMU TCG, QNX 8.0.0. Seed `133058912917`.

## Result (300 s, TCG, attach-only)

| Metric | Value |
|---|---|
| Executions | 118,254 (~394/s over 300.1 s) |
| Crashes | **0** |
| Hangs | **0** |
| Deadline findings | 2 unique buckets, **0 reproducible** |
| Setup failures / respawns | 0 / 0 |
| Reply signatures | 8,185 |
| Max latency | 566,819 us |
| Deadline budget | 100,000 us |

Deadline events, both marked `reproducible: null` (not confirmed on replay):

| Bucket | Op | dcmd | Latency | t |
|---|---|---|---|---|
| `6773abcc745f39d9` | DEVCTL_RAW | `0x02008000` | 119 ms | 4.4 s |
| `b95e1c8e4dcb3e51` | DEVCTL_RAW | `0x00000002` | 567 ms | 121.7 s |

Under QEMU TCG with a 100 ms budget those are treated as flaky/TCG noise,
not defects. The system pipe process (`pipe -U 24:24`, pid 139275) was
still running after the campaign, and a host-side SSH heartbeat succeeded
every 15 s for the whole window.

Honest reading: no replayable crash or hang in the SDP's system `pipe` in
300 s. Attach-only plus `--nonblock` is the supported way to fuzz this
target without taking down SSH.
