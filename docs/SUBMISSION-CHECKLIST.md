# QNX Global Hackathon: submission checklist

Source: the official rules at <https://qnx.devpost.com/rules> (re-read 15 Nov 2026).
**Deadline: 15 Dec 2026, 14:00 EST = 19:00 Helsinki (EET).** Feature freeze 13 Dec.
Category: **Developer Tooling & Host Tools** (a host-based tool that tests software targeting QNX 8.0,
plus an on-target agent).

Status: ✅ done and evidenced in this repo · 🟡 partly done · ❌ not done yet (owner action or later milestone).

## 1. Project requirements (rules §4, "Project Requirements")

| Rule | Evidence in the repo | Status |
|---|---|---|
| Working software that runs on or targets QNX 8.0 (an application/component on QNX incl. QEMU, or a host tool that builds/tests/analyses QNX software) | `target/` (agent + `buggy_rm` resource manager, built with `qcc` for `gcc_ntox86_64`) runs on a `mkqnximage --type=qemu` QNX 8.0 target; `host/resfuzz` boots, deploys, fuzzes and triages it. Campaign results: README "On QNX", `docs/TODO-QNX.md` | ✅ |
| One category selected | Developer Tooling & Host Tools (README status line) | ✅ (select it on the form) |
| Installs and runs consistently, functions as shown in video/text | `scripts/qnx-up.sh` (host packages, bridge, image build, boot, ssh wait), `resfuzz run -c host/examples/qemu.toml`; host tests `host/tests` (47), target tests `make -C target test` | ✅ clean-clone run 15 Nov: `docs/results/clean-run/NOTES.md` (timings, 4 fixes) |
| Runs on the stated platform | QNX SDP 8.0 (baseline 8.0.0.00141, OS build 2026/06/05), x86_64 QEMU 10 under TCG, Linux host | ✅ |
| New, or significantly updated, between 2 Nov and 15 Dec 2026 | First commit 4 Nov 2026 (`git log --reverse`); all QNX work 5-15 Nov | ✅ |
| Third-party SDKs/APIs used under their terms | QNX SDP 8.0 under the owner's non-commercial licence (SDP not redistributed: `qnx-image/`, `build-qnx/` are gitignored); the optional LLM step uses any OpenAI-compatible endpoint the user configures; no other dependencies beyond Python stdlib/pytest | ✅ |

## 2. Submission requirements

| Rule | Evidence | Status |
|---|---|---|
| Built with the required developer tools | `qcc` / `mkqnximage` / `ntox86_64-gdb` from SDP 8.0 (`target/Makefile` `qnx`, `scripts/qnx-up.sh`, `host/resfuzz/qemu.py`) | ✅ |
| Public GitHub/GitLab/Bitbucket repo with all source, assets and instructions; open-source licence detectable in the About section | Apache-2.0 `LICENSE` + `NOTICE` at the root. Repo is **local only** (no remote, nothing pushed) | ❌ owner: create the public repo and push before 15 Dec |
| Repo shows it genuinely builds for/runs on QNX 8.0 | `target/platform/platform_qnx.c` (MsgSend, TimerTimeout, ClockCycles, name_open), `target/buggy_rm/buggy_rm.c` (resmgr/iofunc), `target/tests/qnx/*`, `scripts/qnx-image-snippets/*` (mkqnximage snippets), `target/examples/buggy_rm.qnx.dict` (dcmds resolved on QNX), and the committed QNX campaign [docs/results/qnx-run5](results/qnx-run5/report.md): report, minimised reproducers, gdb backtraces from QNX `dumper` cores (frames in QNXOS_805 `libc` resmgr code) | ✅ |
| Build and run instructions naming QNX version, target and toolchain | README "On QNX" + `docs/TODO-QNX.md` header (SDP 8.0, QEMU x86_64, qcc gcc 12.2.0) | 🟡 fold into the codelab with exact commands |
| Text description of features and functionality | README intro; [`docs/devpost/SUBMISSION.md`](devpost/SUBMISSION.md) draft | 🟡 drafted; paste into the form |
| Demo video < 3 min, public on YouTube, footage of it running on the target, no third-party trademarks/copyrighted music without permission | script [`docs/video/SCRIPT.md`](video/SCRIPT.md); current cut `docs/video/v3.1.mp4` (2:09, narrated flite draft, no music/logos; gitignored) | 🟡 cut ready in-repo; ❌ public YouTube upload |
| Codelab URL (Best Codelab/Tutorial bonus) | full tutorial in [`docs/codelab/README.md`](codelab/README.md) (~60 min, steps 1–8, TCG/KVM notes, troubleshooting) | 🟡 written; ❌ publish + submit URL |
| Evidence of QNX use explained in the description, naming files/functions/output | table and criteria map in [`docs/devpost/SUBMISSION.md`](devpost/SUBMISSION.md) name the files, functions and campaign outputs | 🟡 drafted; paste into the form |
| English | all materials in English | ✅ |
| Original work, solely owned | own code; QNX doc pages are cited, not copied | ✅ |
| Free access for judges until the end of judging | public repo + Apache-2.0 | ❌ with the repo |

## 3. Judging criteria (stage two, equally weighted)

| Criterion | What judges look for | Evidence now | Gaps / next |
|---|---|---|---|
| **Technological Implementation** | well built, effective use of QNX 8.0, non-trivial, really builds/runs on QNX | run5 on QNX found all four planted bugs in 600 s, each with a 1-2 op reproducer replaying 3/3 and a core backtrace (`docs/results/qnx-run5`); six unmodified SDP services on the same path (`devc-pty`, `devb-ram`, `mqueue`, `devf-ram`, attach-only `pipe` and `random`) with honest null crash/hang results; portable C engine, QNX platform layer on native message passing, three oracles (crash via ESRCH/EBADF + waitpid, hang via TimerTimeout + watchdog, deadline via ClockCycles), ASLR-safe coverage, stateful replay; 47 host tests + C unit/integration tests; assumptions in `docs/TODO-QNX.md` | none required for the write-up; more SDP services remain optional |
| **Design** | complete, coherent product, not just a proof of concept | one command boots/deploys/fuzzes/triages (`resfuzz run` / `scripts/run-existing.py`); Markdown+HTML report with minimised reproducers, backtraces, severity, replay command; screenshot `docs/img/report-qnx-run5.png`; `qnx-up.sh`, `--serial`; spawn vs attach-only campaign tomls | a `resfuzz init` scaffold is still "what's next" |
| **Potential Impact** | credible, specific case in a market QNX serves; why a real-time OS foundation matters | README "Potential Impact" plus Devpost draft: automotive/medical/industrial resmgrs; sourced QNX-2017-001 / CVE-2017-3892 and QNX-2023-001 / CVE-2023-32701 (earlier SDP, not a claim about 8.0); deadline oracle via `ClockCycles`; run5 time-to-bug; stock campaigns as a credibility check | none required; a customer quote would still be optional |
| **Innovation & AI Creativity** | creative, non-obvious use of QNX, genuine problem understanding | raw `_IO_DEVCTL` with lying `nbytes`; deadline oracle; stateful minimisation (271→2); ASLR/PIE coverage fix and EBADF corroboration; resolver-on-target; AI seeds via local Qwen2.5-Coder-1.5B with stall disclosed (`docs/results/ai-seeds/`) | covered in `docs/devpost/SUBMISSION.md` |

## 4. Bonus prizes

| Bonus | Requirement | Evidence | Status |
|---|---|---|---|
| **Best Codelab/Tutorial** | URL of a codelab authored during the period that teaches a task on/for QNX 8.0 | [`docs/codelab/README.md`](codelab/README.md); clean-clone timed in `docs/results/clean-run/` | 🟡 written + timed; ❌ publish URL |
| **Best Use of QNX-specific Technologies** | creative/sophisticated use of message passing, resource managers, microkernel, boot customisation, real-time services, security policies | native `MsgSend` on resmgr connections, hand-built `_IO_DEVCTL`, `name_open` targets, `TimerTimeout` unblock, `ClockCycles`/`SYSPAGE_ENTRY(qtime)` deadlines, `dumper` cores, mkqnximage snippets (IFS + data partition + COM2 console), resmgr written against `iofunc`/`resmgr_msgget` | ✅ evidence; 🟡 a security-policy (secpol) angle is not covered |

## 5. Open gaps (owner decisions in bold)

1. **Create the public repository and push** (GitHub; About section shows Apache-2.0). Nothing has been published.
2. Demo video (< 3 min) on YouTube, public.
3. Devpost write-up (text description, QNX evidence list, category, links).
4. Codelab: written (`docs/codelab/README.md`); clean-clone timed (`docs/results/clean-run/`). **Still need the published URL.**
5. ~~Commit one representative QNX report~~ done: `docs/results/qnx-run5`.
6. ~~Screenshots~~ done: `docs/img/report-qnx-run5.png`, `docs/img/qnx-up-boot.png`.
7. ~~Stock SDP targets + LLM seed demo~~ done: six official campaigns under `docs/results/`; AI seeds in `docs/results/ai-seeds/` (stall disclosed).

## 6. Campaign evidence (run5, 15 Nov 2026, QNX 8.0 / QEMU x86_64 TCG, 600 s, coverage on)

| Planted bug | Found at | Oracle | Minimised reproducer | Replays | Backtrace (top frame) |
|---|---|---|---|---|---|
| BUG3a deadline (COMPUTE slow path) | 7.7 s | deadline, 991 ms > 100 ms | 2 ops | 3/3 (exit 12) | n/a (no fault) |
| BUG2 NULL deref (GET_STATS) | 12.6 s | crash, ESRCH + SIGSEGV | 1 op | 3/3 (exit 10) | `buggy_devctl` buggy_logic.c:124 |
| BUG1 overflow (admin SET_NAME), stateful | 173.4 s | crash, ESRCH + SIGSEGV | 271-case history → 2 ops (SET_MODE "ADMN"/2, SET_NAME 34 B) | 3/3 (exit 10) | `buggy_devctl` buggy_logic.c:112 |
| BUG3b hang (n+1 wraps) | 198.5 s | hang, TimerTimeout + SIGABRT core | 1 op | 3/3 (exit 11) | `compute` buggy_logic.c:77 |

136,106 execs (227/s), 107 edges, 227 server restarts, 0 false-positive crashes (run4's EBADF
false positive on a raw `_IO_OPENFD` is fixed in `target/src/exec.c`).

## 7. Stock SDP targets (no planted bugs)

Credibility check, not a failed hunt: six unmodified SDP services, all under QEMU TCG, with no claimed crash or hang:

- [`docs/results/qnx-pty1/NOTES.md`](results/qnx-pty1/NOTES.md): `devc-pty`, 300 s, 0 crashes, 0 hangs, 20 flaky TCG deadlines.
- [`docs/results/qnx-devb-ram1/NOTES.md`](results/qnx-devb-ram1/NOTES.md): `devb-ram`, 120 s, 0 crashes; 1 hang and 8 deadlines, none reproducible.
- [`docs/results/qnx-mqueue1/NOTES.md`](results/qnx-mqueue1/NOTES.md): `mqueue`, 180 s, 0 crashes, 0 hangs; 2 deadlines, none reproducible.
- [`docs/results/qnx-devf-ram1/NOTES.md`](results/qnx-devf-ram1/NOTES.md): `devf-ram`, 240 s, 0 crashes, 0 hangs; 4 deadline buckets are slow flash ops under TCG, not claimed as defects.
- [`docs/results/qnx-pipe1/NOTES.md`](results/qnx-pipe1/NOTES.md): system `pipe`, attach-only, 300 s, 0 crashes, 0 hangs; 2 deadlines, none reproducible.
- [`docs/results/qnx-random1/NOTES.md`](results/qnx-random1/NOTES.md): system `random`, attach-only, 180 s, 0 crashes, 0 hangs, 0 deadline misses.

## 8. AI seed step

Evidence in [`docs/results/ai-seeds/`](results/ai-seeds/): local Qwen2.5-Coder-1.5B via llama.cpp,
validator path exercised, three RFZ1 seeds written. Replayed on QNX: exit 10/10/11 (BUG1, BUG2, BUG3b).
The 1.5B CPU model stalled mid-JSON (~0.3 tok/s) after a correct SET_MODE seed;
that stall is disclosed in NOTES.md. The three validated seeds still replayed.
