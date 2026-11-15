# Devpost submission draft — resfuzz

> Draft for the QNX Global Hackathon form. Every number below comes from files in this
> repo (paths in brackets). Fields marked **[owner]** need Italo's action before submitting.
> Deadline: **15 Dec 2026, 14:00 ET = 19:00 Helsinki.**

## Form fields

| Field | Value |
|---|---|
| Project name | resfuzz |
| Elevator pitch (≤200 chars) | A coverage-guided fuzzer for QNX 8.0 resource managers: it talks native message passing on the target, catches crashes, hangs and real-time deadline misses, and hands back minimal replayable bugs. |
| Category (choose one) | **Developer Tooling & Host Tools** |
| Public repository URL | **[owner]** GitHub URL (Apache-2.0 must show in the About section) |
| Demo video (YouTube, public, < 3 min) | **[owner]** URL — script in `docs/video/SCRIPT.md` |
| Best Codelab/Tutorial URL | **[owner]** published URL of `docs/codelab/README.md` |
| Bonus prizes entered | Best Codelab/Tutorial; Best Use of QNX-specific Technologies |
| Built with (tags) | c, python, qnx, qnx-sdp-8.0, qcc, qemu, mkqnximage, gdb, sanitizercoverage, llama.cpp (optional) |
| QNX version / target / toolchain | QNX SDP 8.0 (OS build 8.0.0 2026/06/05), `mkqnximage --type=qemu --arch=x86_64` under QEMU TCG, `qcc -Vgcc_ntox86_64` (gcc 12.2.0), `ntox86_64-gdb` |

---

## Inspiration

QNX runs things where "it crashed" is not an acceptable bug report: cars, medical
devices, industrial controllers. On QNX almost every driver and service is a
**resource manager** — a user-space server that every client reaches by message
passing. The microkernel keeps a faulty server from taking the system down, but
it can't stop the server from trusting a bad length field, dereferencing a
missing table entry, or spending half a second on its only thread. We wanted a
tool that attacks that interface the way a hostile or buggy client would, on the
real OS, and hands a developer a 1–2 message reproducer and a backtrace.

This is a documented class of issue. BlackBerry advisory
[QNX-2017-001](https://support.blackberry.com/pkb/s/article/46674) describes
CVE-2017-3892 as an information-disclosure vulnerability in procfs, "a resource
manager responsible for managing process information," reached by commands
targeting those resources. [QNX-2023-001](https://support.blackberry.com/pkb/s/article/112401)
(CVE-2023-32701) later recorded improper input validation in the networking stack
(`io-pkt` / `io-sock`) that can cause information disclosure or denial of service.
Those advisories cover earlier SDP releases; they show that bugs in message-facing
user-space services have shipped and been fixed in the field.

## What it does

`resfuzz run -c <campaign.toml>` boots (or reuses) a QNX 8.0 QEMU target, deploys
an agent next to the resource manager under test, fuzzes it for N seconds, then
triages and writes a Markdown + HTML report.

- **Three oracles**, all measured on the target:
  - *crash* — `MsgSend` fails with ESRCH/EBADF **and** `waitpid` confirms the
    server died; a `dumper` core is turned into a gdb backtrace on the host.
  - *hang* — `TimerTimeout` on send/reply plus a watchdog that aborts the server
    (so you still get a core of where it was stuck).
  - *deadline miss* — reply latency from `ClockCycles()` /
    `SYSPAGE_ENTRY(qtime)->cycles_per_sec` above a budget you set.
- **Message-level mutation**: `open`/`read`/`write`/`lseek`/`devctl`, plus raw
  `_IO_DEVCTL` headers whose `nbytes` lies about the payload, and fully raw
  `MsgSend` of any of the 65,536 message types.
- **Dictionary from the real header**: `resfuzz dictgen` emits a C resolver that
  is compiled with `qcc` and run on the target, so dcmd values come from QNX's own
  `<devctl.h>`, not from a host re-implementation.
- **Stateful findings**: if a crash needs earlier messages, the agent saves the
  server's recent history (RFS1 sequence), proves it reproduces on a fresh
  server, and minimises it — in run5 a 271-case history became **2 messages**.
- **Optional AI step**: an OpenAI-compatible model proposes payload sequences
  from the header; a strict validator drops anything that isn't a known dcmd or
  valid hex. The model never decides dcmd numbers.

## How we built it

- **On the target (C, ~4.6k lines incl. tests):** a portable fuzz engine
  (RFZ1/RFS1 formats, mutator, corpus, delta-debugging minimiser, SanitizerCoverage
  runtime with compare logging) behind a small OS interface
  (`target/include/rf_platform.h`). `target/platform/platform_qnx.c` implements it
  with QNX calls; a Linux stub + mock server lets the same engine run in CI.
- **On the host (Python, ~2.3k lines):** mkqnximage wrapper, ssh/serial control,
  deploy/fetch, triage (bucketing, core → backtrace via `ntox86_64-gdb`),
  dictgen, reports.
- **Image customisation:** mkqnximage snippets in `scripts/qnx-image-snippets/`
  bake the binaries into `/data`, enable sshd, and add a root console on COM2.
  `scripts/qnx-up.sh` boots it headless and waits for ssh.
- **Tests:** 486,145 C unit/integration checks (`make -C target test`) and 47
  host tests (`pytest host/tests`), all passing.

## QNX-specific technology used (evidence for judges)

| QNX feature | Where | What it does |
|---|---|---|
| Native message passing | `target/platform/platform_qnx.c` `rf_plat_devctl_raw`, `rf_plat_msgsend_raw` | builds `io_devctl_t` headers by hand and sends with `MsgSend`, including lying `nbytes` |
| `devctl()` / `__DIOT/__DIOF/__DIOTF` | `rf_plat_devctl`; `host/resfuzz/dictgen.py` resolver | dcmds and their sizes resolved by `get_device_size/direction` **on QNX** |
| `name_open` / `name_attach` targets | `rf_plat_open` (`name:` targets); `target/tests/qnx/name_srv.c` | fuzz servers that aren't in the path space |
| Resource manager framework | `target/buggy_rm/buggy_rm.c` (`dispatch_create`, `resmgr_attach`, `iofunc_*`, `resmgr_msgget`) | the demo target, written against the real resmgr API |
| Real-time timing | `ClockCycles`, `SYSPAGE_ENTRY(qtime)` in `platform_qnx.c` | deadline oracle in CPU cycles |
| Kernel timeouts | `TimerTimeout(_NTO_TIMEOUT_SEND|REPLY)` in `arm_timeout()` | hang oracle without blocking forever |
| Post-mortem debugging | `dumper` + `ntox86_64-gdb` (`host/resfuzz/campaign.py` `replay_for_core`) | backtraces with frames in QNX `libc` resmgr code |
| Boot / image customisation | `scripts/qnx-image-snippets/*` (IFS files, data partition, QEMU options, post-start) | reproducible image with the tool baked in and a COM2 shell |
| System headers | `target/examples/dcmd_chr.qnx.dict` from SDP `<sys/dcmd_chr.h>` | fuzzing the SDP's own `devc-pty` |

Every QNX assumption and how it was checked on SDP 8.0 is in `docs/TODO-QNX.md`.

## Results (real runs on QNX 8.0, QEMU x86_64, TCG)

**run5, 600 s, coverage on** [`docs/results/qnx-run5/`]: 136,106 executions
(~227/s), 107 edges, 227 server restarts, all four planted bugs found:

| Bug | First seen | Oracle | Minimised reproducer | Replays |
|---|---|---|---|---|
| BUG3a slow path on the server thread | 7.7 s | deadline (991 ms vs 100 ms) | 2 ops | 3/3 |
| BUG2 NULL deref behind a range check | 12.6 s | crash, SIGSEGV | 1 op | 3/3 |
| BUG1 trusted `nbytes` overflow (stateful) | 173.4 s | crash, SIGSEGV | 271 cases → 2 ops | 3/3 |
| BUG3b `n + 1` wrap → endless loop | 198.5 s | hang | 1 op | 3/3 |

**Independent rerun from a clean clone** [`docs/results/clean-run/`]: 600 s,
241,138 executions (~402/s), 114 edges; BUG3a 3.3 s, BUG2 10.0 s, BUG1 160.3 s,
BUG3b 399.6 s.

**Real SDP drivers** (no planted bugs). We report these honestly rather than claim a bug:

- [`docs/results/qnx-pty1/`]: unmodified `devc-pty`, 300 s, 83,270 executions,
  **0 crashes, 0 hangs**; 20 deadline events, none reproducible (TCG noise).
- [`docs/results/qnx-devb-ram1/`]: unmodified `devb-ram` on a private 1 MiB RAM
  disk (`/dev/hd1`, not the system disk), 120 s, 20,234 executions, **0 crashes**;
  1 hang and 8 deadline events, none reproducible on a fresh server.
- [`docs/results/qnx-mqueue1/`]: unmodified `mqueue` on `/dev/mqueue/rfz`, 180 s,
  59,087 executions, **0 crashes, 0 hangs**; 2 deadline events, none reproducible.
- [`docs/results/qnx-devf-ram1/`]: unmodified `devf-ram` on a private 4 MiB RAM
  flash partition (`/dev/fs0p0`), 240 s, 9,379 executions, **0 crashes, 0 hangs**.
  Four deadline buckets (DCMD_F3S_ERASE, DCMD_F3S_FORMAT, WRITE) replay as slow
  successful flash ops under TCG (285 ms–1.80 s vs a 100 ms budget) and are not
  claimed as defects. One mid-run `setup_fail` + respawn followed a case that
  issued FORMAT and UMOUNT.
- [`docs/results/qnx-pipe1/`]: unmodified system `pipe` (`/dev/pipe`, attach-only,
  `--nonblock`; slaying pipe breaks ssh/ksh), 300 s, 118,254 executions,
  **0 crashes, 0 hangs**; 2 deadline events, none reproducible. SSH stayed up.
- [`docs/results/qnx-random1/`]: unmodified system `random` (`/dev/random`,
  attach-only, `--nonblock`), 180 s, 62,234 executions, **0 crashes, 0 hangs,
  0 deadline misses** (max latency 47 ms).

**AI seed step** [`docs/results/ai-seeds/`]: three validated seeds; replayed on
QNX they exit crash / crash / hang (BUG1-, BUG2-, BUG3b-shaped).

## Challenges we ran into

- **ASLR made coverage lie.** `qcc` links PIE and QNX 8.0 randomises the load
  address per spawn, so hashing absolute PCs counted every restart as new edges
  (9,831 "edges" in one run vs ~100 real). Fix: hash offsets from the module base
  via `dladdr` [`target/covrt/cov_rt.c`, TODO-QNX F4].
- **A live server can reply EBADF.** A raw `_IO_OPENFD` with a garbage handle
  gets EBADF from the resmgr layer while the server is fine; we were calling it a
  crash. Fix: confirm with `waitpid` before reporting [TODO-QNX F5].
- **Stateful bugs don't replay alone.** BUG1 needs admin mode first. We added
  server-history recording, sequence replay and sequence minimisation.
- **TCG only.** KVM crashed this host's kernel, so everything runs under TCG;
  deadline budgets are set to 100 ms and documented.

## Accomplishments we're proud of

- All four planted bugs found on real QNX in 10 minutes, each with a 1–2 op
  reproducer that replays 3/3 and (for crash/hang) a core backtrace.
- A 271-message stateful history minimised automatically to 2 messages.
- Zero false-positive crashes after the EBADF fix; an honest null result on a
  real SDP driver.
- A 60-minute codelab that starts without QNX and ends fuzzing your own server.

## What we learned

How `devctl` really travels (`io_devctl_t` header, `nbytes` vs what was sent,
`resmgr_msgget` for the rest); that PIE + ASLR silently breaks naive coverage;
and that "server gone" errno values need corroboration on QNX.

## What's next

Multi-threaded resmgr targets and pulses; a `resfuzz init` scaffold per target;
CI templates; aarch64le images; and fuzzing more SDP services with their own
headers.

## Criteria map (for judges)

| Criterion | Where to look |
|---|---|
| Technological Implementation | `target/platform/platform_qnx.c`, `target/src/exec.c`, `docs/TODO-QNX.md`, run5 results |
| Design | one-command `resfuzz run`, HTML report (`docs/img/report-qnx-run5.png`), `scripts/qnx-up.sh` |
| Potential Impact | README "Potential Impact"; time-to-bug table above |
| Innovation & AI Creativity | raw `_IO_DEVCTL` with lying lengths, deadline oracle, stateful minimisation, resolver-on-target + validated AI seeds |
| Bonus: Codelab | `docs/codelab/README.md` |
| Bonus: QNX-specific tech | table above |

## Eligibility notes **[owner]**

- Created between 2 Nov and 15 Dec 2026 (first commit 4 Nov 2026).
- Original work; Apache-2.0; QNX SDP not redistributed.
- "QNX" is a trademark of BlackBerry Limited; this project is independent and not
  endorsed by BlackBerry. The video uses no third-party logos or music.
