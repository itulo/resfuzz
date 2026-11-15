# resfuzz

**Fuzz QNX 8.0 resource managers through message passing, from a Linux host.**

> Status: entry for the QNX Global Hackathon (Developer Tooling & Host Tools).
> Built with QNX SDP 8.0 (`qcc`, gcc 12.2.0) and run on a `mkqnximage` QEMU x86_64 target from a Linux host.
> Every QNX-specific assumption and how it was checked on the target is in [docs/TODO-QNX.md](docs/TODO-QNX.md);
> the submission status is in [docs/SUBMISSION-CHECKLIST.md](docs/SUBMISSION-CHECKLIST.md).

In QNX, drivers and services are *resource managers*: user-space servers that
clients reach through `open()`, `read()`, `write()` and `devctl()`, all carried
as messages by `MsgSend()`. A bug in one of them, such as a trusted length field,
a missing bounds check or an unbounded loop, can crash or stall a system that is
supposed to be real-time. resfuzz sends those messages for you, automatically and in volume:

- **Stateful test cases.** Each case is a sequence of operations: open, devctl
  (well-formed or with a deliberately wrong length), read, write, lseek, raw message, or sleep.
- **Three oracles:**
  - **crash**: the server died (signal attributed to this case, core written by `dumper`);
  - **hang**: no reply within the hang limit, caught by `TimerTimeout` plus a watchdog;
  - **deadline miss**: the reply was slower than a real-time budget, measured with `ClockCycles()`.
- **Coverage-guided** when the server is built with `-fsanitize-coverage`, and
  **blind** (driven by reply signatures) otherwise. Compare operands (cmplog)
  feed magic values and devctl codes back into the mutator.
- **Reproducible findings, including stateful ones.** Every finding is replayed on a freshly restarted server.
  If it only fails after earlier messages, the server's history is saved as a sequence (RFS1) and replayed as a
  whole; minimisation shrinks the history and flattens it into one short test case where possible.
  Crashes and hangs are delta-minimised, on a fresh server for each attempt.
- **Host orchestrator** (`resfuzz` Python CLI):
  - builds and boots a QEMU image with `mkqnximage`, deploys over ssh, runs the campaign;
  - fetches findings and cores, gets gdb backtraces, and writes a Markdown and HTML report.
- **Dictionary from headers.** `resfuzz dictgen` reads a driver's devctl header.
  - It generates a resolver program that prints the real `dcmd` values on the target.
  - An optional LLM step can propose meaningful seed sequences. The model never invents dcmd values, and its suggestions are validated.

## Layout

```
target/            C, runs on QNX (and on Linux with a stub platform layer)
  include/ src/    portable engine: RFZ1 test cases, mutator, corpus, minimiser, coverage, executor, agent
  covrt/           SanitizerCoverage runtime linked into the server under test
  platform/        platform_qnx.c (QNX calls, doc-cited) | platform_linux.c (+ mock wire protocol)
  buggy_rm/        demo resource manager with 3 planted bugs + Linux mock_rm
  tests/           C unit tests (ASan/UBSan)
host/              Python 3.11+ orchestrator, no runtime dependencies
docs/              DESIGN.md, TODO-QNX.md, codelab/OUTLINE.md
```

## Quick start (Linux, no QNX needed)

```sh
make -C target test                       # C unit tests under ASan/UBSan
make -C target linux                      # agent + mock_rm (coverage) + mock_rm_blind
python3 -m venv .venv && .venv/bin/pip install -e host pytest
.venv/bin/python -m pytest host           # host tests, including a local end-to-end campaign
.venv/bin/resfuzz local --seconds 60      # fuzz mock_rm, then open resfuzz-out/report.html
```

Tutorial: the step-by-step codelab is in [docs/codelab/README.md](docs/codelab/README.md) (~60 min).

## Potential Impact

QNX ships in systems where a resource-manager bug is not a crash dialog — it is a
missed frame, a stalled actuator, or a device that never comes back. Automotive
gateways, medical monitors and industrial controllers put device I/O behind
resource managers precisely because the microkernel keeps faults isolated; that
isolation only helps if the server itself is robust against the messages it accepts.

resfuzz attacks the same interface those servers expose to every client:

- **Trusted length fields.** `devctl` carries a claimed `nbytes` that the server
  must reconcile with the bytes actually transferred. BUG1 is the classic
  "copy what the header said" overflow; finding it automatically is a direct
  stand-in for ISO 26262 / IEC 62304 robustness testing of I/O handlers.
- **Unchecked indexes and state.** BUG2 is a NULL dereference behind a
  range-checked but sparsely populated table — the kind of latent fault that
  surfaces only after a rare admin/mode transition.
- **Real-time behaviour.** A server that takes half a second on its only thread
  (BUG3a) or never replies (BUG3b) violates deadlines even when it never
  segfaults. The deadline oracle uses QNX `ClockCycles`, so a miss is measured
  in the same units a running system would care about.

Because the agent runs *on the target* over native message passing — not over a
host-side stub — the findings are the failures a real client can provoke. The
host side is a developer tool: one command boots a QEMU target, deploys, fuzzes,
minimises and writes a report with gdb backtraces from `dumper` cores. Teams
already building with SDP 8.0 can drop their own resource manager in place of
`buggy_rm`, generate a dictionary from its header, and run the same loop in CI
against the Linux mock for every commit.

## On QNX (SDP 8.0, QEMU x86_64; see docs/TODO-QNX.md)

```sh
source ~/qnx800/qnxsdp-env.sh             # QNX SDP 8.0 environment
make -C target qnx                        # qcc build into target/build-qnx
scripts/qnx-up.sh                         # boot the mkqnximage QEMU target headless, wait for ssh
resfuzz run -c host/examples/qemu.toml    # (re)boot, deploy, fuzz, triage, report
```

Results on QNX 8.0 under QEMU TCG (no KVM), coverage + QNX dictionary, deadline 100 ms:

| Run | execs/s | Found (first seen) |
|---|---|---|
| run1, 300 s | ~267 | BUG3a (2 s), BUG2 (7 s), BUG1 (72 s), BUG3b hang (189 s) |
| run3, 300 s | ~425 | BUG3a (4 s), BUG2 (96 s); BUG1 and BUG3b not reached |
| **run5, 600 s** (15 Nov, ASLR-safe coverage, stateful replay, aligned field mutations) | ~227 | BUG3a deadline (8 s), BUG2 (13 s), BUG1 (173 s, stateful: 271-case history minimised to 2 ops), BUG3b hang (199 s) |

Every run5 finding has a minimised reproducer that replays 3/3 on a fresh server, and a
`dumper` core backtrace; the report, reproducers and backtraces are committed in
[docs/results/qnx-run5](docs/results/qnx-run5/report.md). Screenshots: [HTML report](docs/img/report-qnx-run5.png), [qnx-up boot](docs/img/qnx-up-boot.png).
Bring the target up with
`scripts/qnx-up.sh` (`--serial` for a console on COM2, `--stop` to shut it down).

A second campaign against the SDP's own unmodified `devc-pty` (no planted bugs) is documented in
[docs/results/qnx-pty1](docs/results/qnx-pty1/NOTES.md); config: [host/examples/qemu-devc-pty.toml](host/examples/qemu-devc-pty.toml).

Backtraces come from QNX cores via `ntox86_64-gdb`. BUG1 depends on server state, so a
fresh-server replay may not reproduce it (TODO-QNX F3).

## Results so far (Linux mock, single core)

| Mode | execs/s | Found |
|---|---|---|
| blind + dictionary, 10 s | ~5,700 | BUG2 NULL deref, BUG3a deadline miss |
| coverage, no dictionary, 30 s | ~3,500 | BUG2 and BUG3a; dcmd values recovered via cmplog |
| coverage + dictionary, 60 s | ~1,800 | BUG1 gated overflow (needs admin mode), BUG2, BUG3a |

BUG3b, the `UINT32_MAX` iteration hang, is hard to reach by design and was not found in 60 s.

## License

Apache-2.0, see [LICENSE](LICENSE) and [NOTICE](NOTICE). QNX is a trademark of BlackBerry Limited; this project is not affiliated with or endorsed by QNX.
