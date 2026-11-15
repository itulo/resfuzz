# QNX campaign against the SDP's own `mqueue` (no planted bugs)

- **Binary:** unmodified `~/qnx800/target/qnx/x86_64/sbin/mqueue` from SDP 8.0
  (copied to the guest as `/data/resfuzz/mqueue`). Do not redistribute.
- **Spawn:** the image's `mqueue` is slayed first, then the agent starts a private
  instance via `mqwrap` (`target/examples/mqwrap.c`), which runs `mqueue -d` and
  creates `/rfz` so the path `/dev/mqueue/rfz` exists after every restart.
- **Target path:** `/dev/mqueue/rfz`.
- **Dictionary:** `target/examples/dcmd_misc.qnx.dict` (MQGETATTR / MQSETATTR /
  MQSETCLOSEMSG and related, values resolved on the guest) plus
  `target/examples/dcmd_all.qnx.dict`.
- **Flags:** `coverage = false` (closed binary), `--respawn-every 0` (one
  instance for the whole run).
- **Config:** `host/examples/qemu-mqueue.toml`.
- **Date:** 15 Nov 2026, QEMU TCG, QNX 8.0.0.

## Result (180 s, TCG, single instance)

| Metric | Value |
|---|---|
| Executions | 59,087 (~328/s) |
| Crashes | **0** |
| Hangs | **0** |
| Deadline findings | 2 unique buckets, **0 reproducible** (122 ms and 127 ms vs a 100 ms budget) |
| Reply signatures | 8,179 |
| Server restarts | 0 |

Honest reading: no crash or hang in the SDP's POSIX message-queue manager in
180 s. The two deadline events are just over the 100 ms TCG budget and did not
replay on a fresh server; they are treated as flaky/TCG noise, not defects.
The system `mqueue` was restarted after the campaign. The same host path
(dictgen → deploy → oracles → report) used on `buggy_rm` was exercised against
a second closed SDP binary.
