# QNX campaign against the SDP's own `devc-pty` (no planted bugs)

- **Binary:** unmodified `~/qnx800/target/qnx/x86_64/sbin/devc-pty` from SDP 8.0
  (copied to the guest as `/data/resfuzz/devc-pty`).
- **Spawn:** `devc-pty -n 1 -o nodaemon` after `slay`ing the image's own instance.
- **Target path:** `/dev/ptyp0`.
- **Dictionary:** `target/examples/dcmd_chr.qnx.dict` — 43 dcmds from
  `<sys/dcmd_chr.h>`, values resolved on the QNX guest by the dictgen resolver
  (`--header-include '<sys/dcmd_chr.h>' --include termios.h --include sys/ioctl.h`).
- **Flags:** `coverage = false` (closed binary), `agent_args = ["--nonblock"]`
  so a read with no data returns EAGAIN instead of looking like a hang.
- **Config:** `host/examples/qemu-devc-pty.toml`.

## Result (300 s, TCG, 15 Nov 2026)

| Metric | Value |
|---|---|
| Executions | 83,270 (~322/s) |
| Crashes | **0** |
| Hangs | **0** |
| Deadline findings | 20 unique buckets, **0 reproducible** |
| Reply signatures | 8,148 |

Honest reading: under QEMU TCG with a 100 ms deadline, character-device ops
occasionally cross the budget; none of those deadline events replayed on a
fresh server, so they are treated as flaky/TCG noise, not defects. No crash or
hang was observed in the SDP driver in this run. That is the expected outcome
for a real, uninstrumented resource manager over a few minutes — and it
exercises the same host path (dictgen → deploy → oracles → report) used on
`buggy_rm`.
