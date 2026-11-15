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
| Demo video (YouTube, public, < 3 min) | **[owner]** URL — script in `docs/video/SCRIPT.md`; current cut is v3.1 (2:09, narrated) |
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
Those advisories cover earlier SDP releases; they are not a claim about 8.0. They
show that bugs in message-facing user-space services have shipped and been fixed
in the field.

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
- **Spawn or attach**: a campaign can start a private instance (including
  daemonizing drivers, via `rmwrap`) or attach to a running system service that
  must not be slayed (`pipe`, `random`).
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
  `scripts/qnx-up.sh` boots it headless under TCG and waits for ssh.
- **Tests:** 486,145 C unit/integration checks (`make -C target test`) and 47
  host tests (`pytest host/tests`), all passing.

## QNX-specific technology used (evidence for the Best Use of QNX Technology bonus)

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
| System headers | `target/examples/dcmd_{chr,blk,f3s,all}.qnx.dict` from SDP headers | dictionaries for stock `devc-pty`, `devb-ram`, `devf-ram`, `pipe`, and `random` |

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

**Stock SDP services** (unmodified, no planted bugs). The same campaign path was
pointed at six built-in resource managers. These runs are evidence that the tool
works on closed binaries, daemonizing drivers, and attach-only system services —
and that it will say so when it does not have a replayable finding, rather than
stretch TCG or flash latency into a claimed defect:

| Target | Time | Execs | Crashes / hangs | Deadlines |
|---|---|---|---|---|
| `devc-pty` (`/dev/ptyp0`) [`docs/results/qnx-pty1/`] | 300 s | 83,270 | **0 / 0** | 20 events, none reproducible (TCG noise) |
| `devb-ram` (private `/dev/hd1`) [`docs/results/qnx-devb-ram1/`] | 120 s | 20,234 | **0** / 1, not reproducible | 8 events, none reproducible |
| `mqueue` (`/dev/mqueue/rfz`) [`docs/results/qnx-mqueue1/`] | 180 s | 59,087 | **0 / 0** | 2 events, none reproducible |
| `devf-ram` (private `/dev/fs0p0`) [`docs/results/qnx-devf-ram1/`] | 240 s | 9,379 | **0 / 0** | 4 buckets (ERASE / FORMAT / WRITE) replay as slow successful flash ops under TCG (285 ms–1.80 s vs a 100 ms budget); not claimed as defects. One mid-run `setup_fail` + respawn followed a case that issued FORMAT and UMOUNT. |
| `pipe` (system `/dev/pipe`, attach-only, `--nonblock`) [`docs/results/qnx-pipe1/`] | 300 s | 118,254 | **0 / 0** | 2 events, none reproducible. SSH stayed up; slaying `pipe` would break ssh/ksh. |
| `random` (system `/dev/random`, attach-only, `--nonblock`) [`docs/results/qnx-random1/`] | 180 s | 62,234 | **0 / 0** | **0** (max latency 47 ms) |

**AI seed step** [`docs/results/ai-seeds/`]: local Qwen2.5-Coder-1.5B-Instruct
(Q4_K_M, llama.cpp; no cloud key). Dcmd numbers came only from the on-target
resolver. The model correctly emitted a `SET_MODE` admin seed, then stalled
mid-JSON writing zeros (~0.3 tok/s on this CPU). The incomplete `SET_NAME`
payload was truncated to the 32 bytes the prompt asked for, and the two
remaining prompt seeds were filled in; all three passed the same validator
`resfuzz dictgen --ai` uses. Replayed on QNX they exited crash / crash / hang
(BUG1, BUG2, BUG3b). The stall is disclosed in the notes: this is evidence that
the contract works, not a claim that a 1.5B CPU model finished a clean JSON
unaided.

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
- Six unmodified SDP services exercised on the same path, with **no claimed
  crash or hang** and no false-positive crashes after the EBADF fix.
- A 60-minute codelab that starts without QNX and ends fuzzing your own server,
  timed from a clean clone.

## What we learned

How `devctl` really travels (`io_devctl_t` header, `nbytes` vs what was sent,
`resmgr_msgget` for the rest); that PIE + ASLR silently breaks naive coverage;
and that "server gone" errno values need corroboration on QNX.

## What's next

Multi-threaded resmgr targets and pulses; a `resfuzz init` scaffold per target;
CI templates; aarch64le images; and fuzzing more SDP services with their own
headers.

## How this maps to the judging criteria

The four stage-two criteria are weighted equally. Pointers below are to this
write-up and to files in the repo.

**Technological Implementation.** The agent runs on QNX 8.0 and speaks native
message passing (`target/platform/platform_qnx.c`: `MsgSend`, hand-built
`io_devctl_t`, `TimerTimeout`, `ClockCycles` / `SYSPAGE_ENTRY(qtime)`,
`name_open`). Three oracles sit on those calls: a crash is reported only after
`waitpid` confirms the server died; a hang uses a kernel send/reply timeout plus
a watchdog core; a deadline miss is measured in the same cycle units the OS
uses. Coverage hashes PCs relative to the module base (`dladdr`) so PIE/ASLR
does not invent edges on every respawn. In 600 s under QEMU TCG, run5 found all
four planted bugs, each minimised to 1–2 ops and replaying 3/3, with `dumper`
backtraces for crash and hang [`docs/results/qnx-run5/`]. A clean-clone rerun
found the same four [`docs/results/clean-run/`]. Assumptions are checked in
`docs/TODO-QNX.md`. 47 host tests and the C suite (`make -C target test`) pass.

**Design.** One command (`resfuzz run -c <toml>`), or `scripts/qnx-up.sh` plus
`scripts/run-existing.py` on an already-booted guest, deploys, fuzzes, triages
and writes a Markdown + HTML report with severity, replay commands and gdb
backtraces [`docs/img/report-qnx-run5.png`]. A campaign toml can spawn a private
instance (`rmwrap` for daemonizing drivers) or attach to a system service that
must not be slayed. `resfuzz dictgen` builds a dictionary from the server's own
header, with dcmd values resolved on the target. The same engine runs in CI
against a Linux mock, so the loop is usable without a board.

**Potential Impact.** Resource managers are the I/O surface of QNX systems in
automotive, medical and industrial products. Public BlackBerry advisories
[QNX-2017-001](https://support.blackberry.com/pkb/s/article/46674) (CVE-2017-3892,
procfs as a resource manager) and [QNX-2023-001](https://support.blackberry.com/pkb/s/article/112401)
(CVE-2023-32701, `io-pkt` / `io-sock`) show that bugs in message-facing
user-space services have shipped and been fixed in earlier SDP releases — not a
claim about 8.0. A team already on SDP 8.0 can drop their own server into the
same loop. On the planted target, run5 time-to-find was 8 s / 13 s / 173 s /
199 s, each with a 1–2 message reproducer. The six stock campaigns are the
credibility check: the tool ran against closed SDP binaries and did not invent
a bug when it did not have one.

**Innovation & AI Creativity.** Four pieces that are easy to miss if you only
fuzz POSIX `read`/`write` on Linux: (1) raw `_IO_DEVCTL` / `MsgSend` with a
lying `nbytes` — the class of bug BUG1 represents; (2) a cycle-accurate deadline
oracle, not only crash and hang; (3) ASLR-aware coverage and a crash oracle that
corroborates "server gone" errno values, both found the hard way (see Challenges)
and fixed; (4) stateful history recording and minimisation (271 → 2 ops). The
optional AI seed step is deliberately narrow: local Qwen2.5-Coder-1.5B proposes
payloads; the resolver owns dcmd numbers; a validator drops unknown names and
bad hex. The 1.5B CPU run stalled mid-JSON; that is disclosed, and the three
validated seeds still replayed on QNX as crash / crash / hang
[`docs/results/ai-seeds/`].

**Best Codelab/Tutorial.** `docs/codelab/README.md` is a
~60-minute path that starts on Linux with no QNX install (unit tests, mock
server, local campaign) and ends on a QEMU QNX target fuzzing your own resource
manager. A clean-clone run was timed and recorded [`docs/results/clean-run/`].
The published URL is still an **[owner]** step.

**Best Use of QNX-specific Technologies.** The table above is the evidence list:
native message passing, on-target dcmd resolution, `name_open`, the real resmgr
API in `buggy_rm`, `ClockCycles` / `TimerTimeout`, `dumper` + `ntox86_64-gdb`,
and mkqnximage snippets (IFS, data partition, COM2 shell). Stock campaigns
further use SDP headers and an attach-only policy when slaying a service would
break `ssh` or `ksh`.

## Eligibility notes **[owner]**

- Created between 2 Nov and 15 Dec 2026 (first commit 4 Nov 2026).
- Original work; Apache-2.0; QNX SDP not redistributed.
- "QNX" is a trademark of BlackBerry Limited; this project is independent and not
  endorsed by BlackBerry. The video uses no third-party logos or music.
