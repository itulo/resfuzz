# QNX campaign against the SDP's own `devf-ram` (no planted bugs)

- **Binary:** unmodified `~/qnx800/target/qnx/x86_64/sbin/devf-ram` from SDP 8.0
  (copied to the guest as `/data/resfuzz/devf-ram`). Do not redistribute.
- **Spawn:** a private 4 MiB RAM-backed flash array, not a system partition:
  `rmwrap --wait /dev/fs0p0 --name devf-ram -- /data/resfuzz/devf-ram -s0x0,4M`.
  `devf-ram` daemonizes, so `rmwrap` (`target/examples/rmwrap.c`) waits for
  `/dev/fs0p0` and adopts the surviving server pid.
- **Target path:** `/dev/fs0p0`.
- **Dictionary:** `target/examples/dcmd_f3s.qnx.dict` (F3S dcmds from
  `<sys/dcmd_f3s.h>`, values resolved on the guest) plus `dcmd_all.qnx.dict`.
- **Flags:** `coverage = false` (closed binary), `--respawn-every 0` (one
  instance unless the agent has to recover the path).
- **Config:** `host/examples/qemu-devf-ram.toml`.
- **Date:** 15 Nov 2026, QEMU TCG, QNX 8.0.0. Seed `53065146652`.
- **Host runner:** `scripts/run-existing.py` with `--resmgr` pointing at this
  box's SDP path (`/home/box/qnx800/...`).

## Result (240 s, TCG, single instance)

| Metric | Value |
|---|---|
| Executions | 9,379 (~38.8/s over 241.5 s) |
| Crashes | **0** |
| Hangs | **0** |
| Deadline findings | 4 unique buckets, all marked reproducible by the agent |
| Setup failures / respawns | 1 / 1 (see below) |
| Reply signatures | 2,858 |
| Max latency | 3,621,479 us |
| Deadline budget | 100,000 us |

The lower exec rate versus `devb-ram` (~165/s) is consistent with flash
erase / format / reclaim work under TCG: the campaign slowed from ~179/s
early on to the mid-20s/s once erase traffic started, and `max_latency_us`
reached 3.62 s.

## Deadline buckets (not claimed as defects)

All four buckets replayed as deadline misses on a fresh server. They are
slow, successful (errno 0) flash operations under QEMU TCG, not crashes or
hangs:

| Bucket | Op | dcmd | Worst latency | Hits |
|---|---|---|---|---|
| `f25b61b869adea69` | DEVCTL | `DCMD_F3S_ERASE` (`0x800c0900`) | 715 ms | 3 |
| `b5eb9a655241e50c` | DEVCTL_RAW | `DCMD_F3S_ERASE` (`0x800c0900`) | 708 ms | 3 |
| `e1dab1e1194643f5` | WRITE | — | 1.80 s | 3 |
| `e3273701493ed989` | DEVCTL_RAW | `DCMD_F3S_FORMAT` (`0x801c0901`) | 285 ms | 1 |

A 100 ms realtime budget on TCG is far tighter than these inherent flash
ops. Without a comparison on real flash hardware (or a much looser budget)
they are **not** treated as driver defects. Saved cases live in
`docs/results/qnx-devf-ram1/deadlines/`.

## The one setup_failure + respawn

`--respawn-every 0` was set, so the single restart was recovery, not a
periodic bounce. Mid-run stats at t≈47 s still showed 0 respawns; the
restart happened later. The FORMAT deadline case at t=180.0 s ends with
`DCMD_F3S_UMOUNT` (`0x00000908`) after `DCMD_F3S_FORMAT` (`0x801c0901`).
Either dcmd can take the partition node away, so the next `open("/dev/fs0p0")`
is a `setup_fail` and the agent respawns `rmwrap`/`devf-ram`. That is a
valid F3S control path, not a crash: no core, no hang, no `crashes/` or
`hangs/` entry. After the official 240 s window the leftover server was
idle and healthy (`SIGWAITINFO` / `RECEIVE`).

Honest reading: no replayable crash or hang in the SDP's `devf-ram` in 240 s.
The campaign still exercises F3S dictgen, `rmwrap` around a daemonizing
flash driver, and the deadline oracle on slow erase/format/write paths.
