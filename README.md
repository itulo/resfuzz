# resfuzz

**Fuzz QNX 8.0 resource managers through message passing, from a Linux host.**

> Status: work in progress for the QNX Global Hackathon (Developer Tooling & Host Tools).
> Everything runs on Linux today against a mock server. The QNX target side is
> written but has **not been compiled or run yet**, because the QNX SDP 8.0 isn't installed.
> [docs/TODO-QNX.md](docs/TODO-QNX.md) lists every QNX-specific assumption that still needs checking.

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
- **Reproducible findings.** Every finding is replayed on a freshly restarted server and marked
  reproducible or stateful. Crashes and hangs are delta-minimised, again on a fresh server for each attempt.
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

## On QNX (planned; see docs/TODO-QNX.md)

```sh
source ~/qnx800/qnxsdp-env.sh             # QNX SDP 8.0 environment
make -C target qnx                        # qcc build into target/build-qnx
resfuzz image build -c host/examples/qemu.toml
resfuzz run -c host/examples/qemu.toml    # boot, deploy, fuzz, triage, report
```

## Results so far (Linux mock, single core)

| Mode | execs/s | Found |
|---|---|---|
| blind + dictionary, 10 s | ~5,700 | BUG2 NULL deref, BUG3a deadline miss |
| coverage, no dictionary, 30 s | ~3,500 | BUG2 and BUG3a; dcmd values recovered via cmplog |
| coverage + dictionary, 60 s | ~1,800 | BUG1 gated overflow (needs admin mode), BUG2, BUG3a |

BUG3b, the `UINT32_MAX` iteration hang, is hard to reach by design and was not found in 60 s.

## License

Apache-2.0, see [LICENSE](LICENSE) and [NOTICE](NOTICE). QNX is a trademark of BlackBerry Limited; this project is not affiliated with or endorsed by QNX.
