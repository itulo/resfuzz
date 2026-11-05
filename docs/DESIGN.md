# resfuzz design

QNX facts used here are cited inline. What was checked on a real SDP 8.0
(QEMU x86_64 target), and what is still open, is in [TODO-QNX.md](TODO-QNX.md).

## 1. Goal and scope

resfuzz finds robustness bugs in QNX 8.0 resource managers by driving their
message interface the way real clients do: `open`/`name_open`, `read`, `write`,
`lseek`, `devctl`, and raw `MsgSend`. It reports three kinds of finding:

| Oracle | Signal | How it is detected |
|---|---|---|
| crash | the server died | `MsgSend` fails with the server gone; the agent reaps the spawned child and records the signal; `dumper` writes a core |
| hang | no reply | `TimerTimeout` on send and reply, plus a watchdog thread (2 × hang limit + 100 ms) that aborts the spawned server, because `TimerTimeout` doesn't unblock a client whose message was already received by a server that set `_NTO_CHF_UNBLOCK` ([timertimeout](https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.lib_ref/topic/t/timertimeout.html)) |
| deadline miss | reply latency above budget | `ClockCycles()` delta / `SYSPAGE_ENTRY(qtime)->cycles_per_sec` ([clockcycles](https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.lib_ref/topic/c/clockcycles.html)) |

Out of scope for the hackathon: kernel fuzzing, multi-node QNET, and
pulses/asynchronous messaging.

## 2. Architecture

```
 Linux host                                   QNX 8.0 guest (QEMU, mkqnximage)
 ───────────────────────────────              ─────────────────────────────────────────
 resfuzz CLI (Python)                         /data/resfuzz/
  ├─ qemu.py    mkqnximage build/run/getip      resfuzz-agent ──MsgSend──▶ resource manager
  ├─ control.py ssh/scp (serial fallback)        │  mutate → exec → oracles   (spawned by agent,
  ├─ campaign.py deploy, run, fetch, replay       │  corpus, minimiser          optional coverage
  ├─ triage.py  buckets, gdb backtraces           │  watchdog thread            shm "RESFUZZ_SHM")
  ├─ report.py  Markdown + HTML                   └─ out/ events.jsonl stats.json crashes/ hangs/ deadlines/ corpus/
  └─ dictgen.py devctl header → resolver C → dictionary (+ optional LLM seeds)
                                                dumper -d /data/resfuzz/cores
```

The fuzzing loop runs **on the target**: running it there avoids an ssh round trip per test case.
The host builds the image, deploys, collects the results and analyses them.

Portability layer: `target/include/rf_platform.h` is the only interface to the OS.
- `platform_qnx.c` implements it with QNX calls.
- `platform_linux.c` implements it over a SEQPACKET socket that carries a header with
  the same field layout as `struct _io_devctl`. On Linux, `mock_rm` stands in for
  `buggy_rm.c`: both share `buggy_logic.c`, so the same bugs exist on Linux.
- The whole engine, the unit tests and the host tests therefore run in CI without QNX.

## 3. Test-case format (RFZ1)

Little-endian; implemented in `target/src/testcase.c` and `host/resfuzz/testcase.py`, and cross-tested.

```
header : "RFZ1" | u16 nops (≤64) | u16 reserved = 0
op     : u8 kind | u8 flags (bits0-1 = connection slot) | u16 len (≤4096) | u32 a | u32 b | payload[len]
```

| kind | meaning |
|---|---|
| OPEN | `open(path, a%3)`; or `name_open()` when the target is a name, not a path |
| CLOSE | close slot |
| READ | `read(a)` bytes |
| WRITE | `write(payload)` |
| DEVCTL | `devctl(fd, dcmd=a, payload, max(len,b))`, the well-formed path through libc |
| DEVCTL_RAW | hand-built `_IO_DEVCTL` via `MsgSend`, where header `nbytes = b` may lie about the payload length |
| LSEEK | `lseek((int32)a, b%3)` |
| MSG_RAW | `MsgSend(payload)` verbatim |
| SLEEP | `usleep(min(a, cap))`, to perturb timing |

Parsing is strict. Any trailing bytes, a non-zero reserved field or an out-of-range
value is rejected, so `parse(serialize(x)) == x`, and the parser is itself fuzzed in the unit tests.

## 4. Mutation engine

- **Operation-level mutations:** insert, delete, duplicate, swap, splice with another corpus entry.
- **Payload mutations:** bit and byte flips, arithmetic, interesting values (0, -1, sizes, 2^n±1), block insert/delete, and dictionary tokens.
- **Field mutations:** `a` and `b`, which hold the dcmd, the claimed length and offsets. They take interesting values, sizes from the dcmd, and 4-byte dictionary tokens.
- **Seeds:** when no corpus is given, `rf_gen_seeds` builds one seed per dictionary dcmd, sized from the dcmd's size bits.
- **Dictionary format:** one entry per line, either `dcmd NAME 0xVALUE size=N dir=to|from|tofrom|none` or `name="escaped bytes"`.

## 5. Feedback

- **Coverage** is collected when the server is compiled with `-fsanitize-coverage=trace-pc,trace-cmp` and linked
  with `covrt/cov_rt.c`.
  - Edges are hashed into a 64 KiB map in POSIX shared memory ([shm_open](https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.lib_ref/topic/s/shm_open.html)), named by `RESFUZZ_SHM`.
  - The compare log records constant operands (Arg1 for `const_cmp`, the case value for switches;
    [SanitizerCoverage](https://clang.llvm.org/docs/SanitizerCoverage.html#tracing-data-flow)). Each round, values ≥ 256 are fed back as dictionary tokens. This is how the fuzzer learns dcmd values and magic values without a dictionary.
  - QNX 8.0's gcc 12.2.0 accepts these flags (verified, TODO C1).
- **Reply signatures**, which hash (op kind, dcmd, errno class) for each executed op, are used in both modes. In **blind mode** they are the only feedback: a test case is kept if it produces a new signature.
- **Corpus admission:** only cases whose result is OK are added. Slow (deadline) inputs are kept as findings, not as corpus entries, so they don't drag down the exec rate.

## 6. Statefulness and reproducibility

Resource managers keep state across clients: modes, names, tables. A finding
can depend on state left behind by earlier test cases. For that reason:

1. Every new finding is replayed once on a freshly restarted server and
   recorded as `reproducible: true/false` (or null when the agent doesn't manage the server).
2. The minimiser restarts the server before every candidate, so a minimised case reproduces from a clean boot.
   - Pass 1 deletes ops; pass 2 shrinks payloads (head and tail halves, then drop the last or first byte).
   - These restarts use SIGKILL only, with no core; restarts after a real crash send SIGABRT first so `dumper` writes a core.
3. A crash is bucketed by (kind, op kind, dcmd, signal). The host refines the bucket with a
   stack hash of the top three frames, which ignores addresses.

## 7. Host orchestrator

- `qemu.py` wraps `mkqnximage` ([mkqnximage](https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.utilities/topic/m/mkqnximage.html)):
  - flags `--type=qemu --arch=… [--ssh-ident] [--assumed-ip] --build [--clean]`, then `--run=-h` (headless) and `--getip`;
  - it always runs in the image's directory;
  - console output is in `output/qemu.out`.
- `control.py` provides `SshTarget` (ssh/scp; root login is enabled on these images), `SerialTarget`
  (sentinel-wrapped commands, for when networking is broken) and `LocalTarget`.
- `campaign.py`:
  - `LocalCampaign` drives the Linux mock;
  - `QemuCampaign` boots, deploys, starts `dumper -d` ([dumper](https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.utilities/topic/d/dumper.html)), fuzzes, and fetches results;
  - then, for each finding, it **clears the dump directory, replays the case, and fetches the cores**. A core is matched to its finding by construction, with no assumption about dumper's file naming. It then minimises the case.
- `triage.py` / `report.py` produce `report.md` and a self-contained `report.html` with everything escaped.
  Each finding lists its severity, reproducibility, hit count, a reproduce command, the decoded minimised case and the backtrace.
- `dictgen.py` (the "AI step"):
  1. Parse the `__DIO*` defines and constants in the header.
  2. Generate a C resolver that includes the real `<devctl.h>` and prints the values through
     `get_device_size()` and `get_device_direction()` ([devctl](https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.lib_ref/topic/d/devctl.html)).
     The host never re-implements the dcmd bit layout.
  3. Optionally, ask an OpenAI-compatible model for seed *sequences*, such as "set admin mode, then send an over-long name".
     The reply is validated strictly: only known dcmd names, valid hex, size limits.

## 8. Demo target: buggy_rm

`target/buggy_rm/` is a resource manager written from the QNX resmgr samples
([devctl](https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.resmgr/topic/messages_Sample_IO_DEVCTL.html),
[read](https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.resmgr/topic/read_write_Sample_IO_READ.html),
[write](https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.resmgr/topic/read_write_Sample_IO_WRITE.html)).
It has three planted bugs of increasing depth:

| Bug | Command | Kind | Gate |
|---|---|---|---|
| 1 | `SET_NAME` | `memcpy` uses the client's claimed length, overflowing into a pointer, followed by a dereference | only in admin mode: `SET_MODE{magic "ADMN", level 2}` first |
| 2 | `GET_STATS` | the index check allows 0–7 but only slots 0–3 exist, so a NULL dereference | none |
| 3a | `COMPUTE` | precise mode: an uncapped loop of about 2^18 iterations, a deadline miss | `flags == SLOW` exactly |
| 3b | `COMPUTE` | `n + 1` wraps at `UINT32_MAX`, so the loop never ends (hang) | as 3a, plus the exact value |

## 9. Milestones (deadline: Devpost, 15 Dec 2026 21:00 Helsinki time; we freeze on 13 Dec)

| Week | Dates | Deliverable | Status |
|---|---|---|---|
| W1 | 4–8 Nov | Portable engine, Linux mock, host orchestrator, tests, docs skeleton | done (this commit) |
| W2 | 9–15 Nov | Install the SDP; `make qnx` compiles; mkqnximage QEMU image boots; ssh/scp work; work through TODO-QNX sections A, D, E, I | |
| W3 | 16–22 Nov | First campaign on QNX against `buggy_rm` (blind); real dcmd values from the resolver; dumper cores with gdb backtraces in the report | |
| W4 | 23–29 Nov | Coverage on QNX (or a documented fallback); tune the deadline oracle against `cycles_per_sec`; fuzz one real or open-source resource manager | |
| W5 | 30 Nov–6 Dec | Polish the AI dictionary step; finish the report UI; write the codelab and check it on a clean machine | |
| W6 | 7–13 Dec | Demo video, Devpost write-up, screenshots; **feature freeze 13 Dec**; 14–15 Dec are buffer | |
