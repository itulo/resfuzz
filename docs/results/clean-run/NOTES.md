# Clean-machine test of the codelab (15 Nov 2026)

Fresh `git clone` (commit f31b260) into `/tmp/rfclean/resfuzz`, run with a clean
`HOME=/tmp/rfclean/home` and a new venv. The installed SDP (`/home/box/qnx800`)
was reused; only the user's SDP licence directory was placed in the clean
`~/.qnx/license` (what QNX Software Center deploys for a real user). Steps were
followed from `docs/codelab/README.md` as written; wall-clock times are in
`timings.txt`.

| Step | What | Time | Result |
|---|---|---|---|
| 2 | `make -C target test linux` | 3.9 s | 486,145 checks, 0 failures |
| 2 | venv + `pip install -e host` | 3.6 s | ok |
| 2 | `resfuzz local --seconds 30 --no-cov` | 31.1 s | 3 crash, 4 deadline (5 reproducible), 151,275 execs |
| 2 | manual `--replay` + `--decode` | <0.1 s | exit 10, 2-op BUG1 case |
| 3 | `resfuzz local --seconds 60` (coverage) | 73.6 s | 120 edges vs 0; 3 crash, 1 hang, 5 deadline |
| 4 | `qcc -V` | <0.1 s | **failed** until the licence was in `~/.qnx` (doc fixed) |
| 4 | `make -C target qnx` | 1.7 s | ok |
| 4 | `scripts/qnx-up.sh` | **failed** (looked for `~/qnx800`, no ssh key) → fixed; 19.1 s incl. image build | QNX 8.0.0 up |
| 5 | dictgen → qcc resolver → run on target | **failed** (`buggy_rm.h` not found) → doc fixed (`-Itarget/buggy_rm`); 1.0 s | dcmds identical to the committed dictionary |
| 6 | `resfuzz run -c host/examples/qemu.toml --no-build` | 837.9 s (600 s fuzz + ~4 min triage) | see below |
| 7 | replay BUG1 `.seq.min` ×3 on QNX | ~6 s | exit 10, 10, 10 |

## Campaign from the clean clone (600 s, TCG)

241,138 executions (401.9/s), 114 edges. First seen: BUG3a deadline 3.3 s,
BUG2 crash 10.0 s, BUG1 crash 160.3 s (stateful), BUG3b hang 399.6 s
(`hang_…_1.rfz`, `ffffffff 01000000`, reproducible alone).

## Fixes made from this test

1. `scripts/qnx-up.sh`: use the SDP whose env script is sourced (`$QNX_HOST`) or
   `$QNX_SDP`, with a clear error otherwise; create `~/.ssh/qnx_resfuzz_ed25519`
   on first run.
2. Codelab: licence prerequisite; local-mode reproducers are `crashes/*.min.rfz`;
   `-Itarget/buggy_rm` for the resolver; write the regenerated dictionary to
   `/tmp` (the committed one has hand-added tokens); source the SDP before
   `resfuzz run` (gdb on PATH); exact BUG1 file name in step 7.
3. `resfuzz decode` now reads RFS1 sequences as well as RFZ1 cases.
4. Triage: when a bucket has an earlier flaky hit and a later case that
   reproduces alone, replay/minimise the solo-reproducible one. In this run the
   first "hang" (293 s) was a load-induced BUG3a slow path that only reproduced
   as an 813-case sequence; the real BUG3b came at 399.6 s and is now the one
   minimised (new test `test_replay_file_prefers_solo_reproducible`).
