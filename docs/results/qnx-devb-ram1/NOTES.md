# QNX campaign against the SDP's own `devb-ram` (no planted bugs)

- **Binary:** unmodified `~/qnx800/target/qnx/x86_64/sbin/devb-ram` from SDP 8.0
  (copied to the guest as `/data/resfuzz/devb-ram`). Do not redistribute.
- **Spawn:** a private 1 MiB RAM disk, not the system disk (`/dev/hd0`):
  `rmwrap --wait /dev/hd1 --name devb-ram -- /data/resfuzz/devb-ram ram capacity=2048 blk cache=512k,auto=none`.
  `devb-ram` daemonizes, so `rmwrap` (`target/examples/rmwrap.c`) waits for `/dev/hd1`
  and adopts the surviving server pid so the agent can restart it.
- **Target path:** `/dev/hd1`.
- **Dictionary:** `target/examples/dcmd_blk.qnx.dict` (29 dcmds from `<sys/dcmd_blk.h>`,
  values resolved on the guest) plus `target/examples/dcmd_all.qnx.dict`.
- **Flags:** `coverage = false` (closed binary), `--respawn-every 0` (one instance
  for the whole run, so a mid-run restart cannot leave two drivers on one node).
- **Config:** `host/examples/qemu-devb-ram.toml`.
- **Date:** 15 Nov 2026, QEMU TCG, QNX 8.0.0.

## Result (120 s, TCG, single instance)

| Metric | Value |
|---|---|
| Executions | 20,234 (~165/s) |
| Crashes | **0** |
| Hangs | 1 unique bucket, **0 reproducible** |
| Deadline findings | 8 unique buckets, **0 reproducible** |
| Reply signatures | 3,095 |

The hang (`hangs/hang_3b954408d18d2385_0.rfz`) was a `READ` that returned
`ETIMEDOUT` after 4.78 s at t=122.5 s, with a 1,025-case server history that the
agent itself marked not reproducible on a fresh server. Host-side replay of the
16-op case hit the orchestrator's 120 s SSH timeout (exit 124) because
`devb-ram` daemonizes and the session waited on the leftover server; a later
`pidin` of that leftover process showed a healthy idle driver (threads in
`RECEIVE` / `SIGWAITINFO`), not a deadlock. Deadline events include
`DCMD_BLK_FORCE_RELEARN` and a few raw dcmds at 110 ms–1.3 s; none replayed.
Under QEMU TCG with a 100 ms budget those are treated as flaky/TCG or
state-dependent noise, not defects.

## Earlier smoke (not a claimed bug)

A first 25 s attempt used the default `--respawn-every 1000`. The agent
restarted mid-run and left several `devb-ram` processes. After that, `open` /
`ls` on `/dev/hd1` blocked in `SEND`/`REPLY` to one of those servers, and
`pidin` showed a `devb-ram` thread in `sync_mutex_lock` waiting on a mutex owned
by the same process (`docs/results/qnx-devb-ram1/notes-smoke/thread-dump.txt`).
`slay` did not clear it; the guest was rebooted. That hang is **not** claimed as
a driver bug: it was observed only with multiple overlapping instances of the
same block driver. The official 120 s run above used a single instance.

Honest reading: no replayable crash or hang in the SDP's `devb-ram` in 120 s.
The campaign still exercises dictgen (block-layer headers), spawning a
daemonizing driver through `rmwrap`, and the same host path used on `buggy_rm`.
