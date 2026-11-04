# Codelab outline: Fuzz-test a QNX 8.0 resource manager on QEMU from a Linux host

Format: [QNX codelab](https://qnx.github.io/codelabs/) style (steps with
duration estimates). Target length is about 60 minutes. Steps 1–3 work without QNX, so readers
can try the tool before installing anything.

**You'll learn:** how resource managers receive `devctl` messages, why their
length and state handling is a classic bug source, and how to find crashes, hangs and
real-time deadline misses automatically with coverage-guided fuzzing.

**You'll need:** an Ubuntu 22.04/24.04 x86-64 host (the supported host for mkqnximage's QEMU target), gcc, make,
Python 3.11+ and about 10 GB of disk. For steps 4 onward: a free QNX account, a QNX SDP 8.0
non-commercial licence and QEMU. *(TODO: confirm the exact package list, see TODO-QNX I2.)*

---

### 1. Overview (2 min)
- What a resource manager is: a user-space server that clients reach through `open`, `read`, `write` and `devctl`, all as messages.
- The demo target `buggy_rm` and its three planted bugs. Don't reveal the details yet.
- Architecture diagram from DESIGN.md: the agent runs on the target, and the host orchestrates.

### 2. Run it locally against the mock (10 min, no QNX)
- `make -C target test linux`; set up a venv; `pip install -e host`.
- `resfuzz local --seconds 30 --no-cov`: a blind run. Open `report.html`.
- Read one finding: bucket, reproducible flag, minimised case, reproduce command.
- Replay it by hand with `resfuzz-agent --replay`; use `--decode` to read the test case.

### 3. Turn on coverage and see what it buys (8 min, no QNX)
- Rebuild the mock with `-fsanitize-coverage=trace-pc,trace-cmp` (already the default `mock_rm`).
- Run without a dictionary: dcmd values and the `"ADMN"` magic are recovered from compare operands.
- Compare edges and findings with step 2. Discuss why BUG1 needs state (admin mode first) and why BUG3b is hard.
- Concept box: stateful targets, and why every finding is replayed on a fresh server.

### 4. Set up the QNX side (12 min)
- Install the SDP 8.0 and source its environment script; check with `qcc -V`.
- `make -C target qnx` builds the agent and `buggy_rm` for x86_64.
- `resfuzz image build -c host/examples/qemu.toml` (mkqnximage `--type=qemu`), then `resfuzz image run` and `resfuzz image ip`.
- Check you can reach the target: `ssh root@<ip>`, and confirm `/data` is writable.

### 5. Generate a dictionary from the header (8 min)
- `resfuzz dictgen target/buggy_rm/buggy_rm.h --resolver-out resolver.c`.
- Compile `resolver.c` with qcc, run it on the target, then `resfuzz dictgen … --resolved out.txt --out buggy_rm.qnx.dict`.
- Explain why the dcmd values come from the target's own `<devctl.h>` and not from Python or an LLM.
- Optional: `--ai` with an OpenAI-compatible endpoint (environment variables) to propose seed sequences; show the validation.

### 6. Fuzz on QNX (10 min)
- `resfuzz run -c host/examples/qemu.toml`: deploy, start `dumper`, fuzz for 10 minutes, fetch the results.
- While it runs: read `events.jsonl` live over ssh; explain the watchdog and the deadline oracle (`ClockCycles`).

### 7. Triage (8 min)
- Open the report: backtraces from dumper cores via the SDP's gdb, and minimised reproducers.
- Walk through each bug and its fix in `buggy_logic.c`: validate `nbytes` against `sizeof`, a bounds-and-NULL check,
  and capping `n` before `n + 1`.
- Re-run the campaign on the fixed build: zero findings, and the deadline miss is gone.

### 8. Fuzz your own resource manager (2 min)
- Checklist: start command (`--spawn`), path or name, header for dictgen, deadline budget,
  and optional coverage build flags.
- Where to go next: CI integration (`resfuzz local` in GitHub Actions), and links to the docs.

---

**Open items before publishing:** every QNX command above must be run and its output pasted in
(TODO-QNX A1, I1–I4, T1–T2); add screenshots of the report; time each step on a clean machine.
