# QNX campaign against the SDP's own `random` (no planted bugs)

- **Binary:** the image's running system service
  (`/system/bin/random -U 22:22 -s /data/var/random/rnd-seed`, pid
  unchanged for the whole run). The SDP copy at
  `~/qnx800/target/qnx/x86_64/usr/sbin/random` was deployed for the
  record only and was **not** spawned. Do not redistribute the QNX binary.
- **Mode:** attach-only (`spawn` unset). `random` daemonizes and other
  services may read `/dev/random` / `/dev/urandom`, so this run never
  slayed it.
- **Target path:** `/dev/random`.
- **Dictionary:** `target/examples/dcmd_all.qnx.dict`.
- **Flags:** `coverage = false`, `--nonblock` (a blocking entropy read
  must not look like a hang), no respawn.
- **Config:** `host/examples/qemu-random.toml`.
- **Date:** 15 Nov 2026, QEMU TCG, QNX 8.0.0. Seed `266739770449`.

## Result (180 s, TCG, attach-only)

| Metric | Value |
|---|---|
| Executions | 62,234 (~345.7/s over 180.0 s) |
| Crashes | **0** |
| Hangs | **0** |
| Deadline findings | **0** |
| Setup failures / respawns | 0 / 0 |
| Reply signatures | 8,185 |
| Max latency | 46,903 us (under the 100,000 us budget) |

`events.jsonl` contains only the start and done records. The system
`random` process (pid 126986) was still running after the campaign, and
SSH stayed up for the full window.

Honest reading: no crash, hang, or deadline miss in the SDP's system
`random` in 180 s.
