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
| Installs and runs consistently, functions as shown in video/text | `scripts/qnx-up.sh` (host packages, bridge, image build, boot, ssh wait), `resfuzz run -c host/examples/qemu.toml`; host tests `host/tests` (46), target tests `make -C target test` | 🟡 a clean-machine run of the codelab is still to do |
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
| Text description of features and functionality | README intro; Devpost write-up | ❌ Devpost text (draft from README + results) |
| Demo video < 3 min, public on YouTube, footage of it running on the target, no third-party trademarks/copyrighted music without permission | — | ❌ record (plan below) |
| Codelab URL (Best Codelab/Tutorial bonus) | full tutorial in [`docs/codelab/README.md`](codelab/README.md) (~60 min, steps 1–8, TCG/KVM notes, troubleshooting) | 🟡 written; ❌ publish + submit URL |
| Evidence of QNX use explained in the description, naming files/functions/output | the "Repo shows QNX" row above is that list; reuse it in the Devpost text | 🟡 |
| English | all materials in English | ✅ |
| Original work, solely owned | own code; QNX doc pages are cited, not copied | ✅ |
| Free access for judges until the end of judging | public repo + Apache-2.0 | ❌ with the repo |

## 3. Judging criteria (stage two, equally weighted)

| Criterion | What judges look for | Evidence now | Gaps / next |
|---|---|---|---|
| **Technological Implementation** | well built, effective use of QNX 8.0, non-trivial, really builds/runs on QNX | run5 on QNX found all four planted bugs in 600 s, each with a 1-2 op reproducer replaying 3/3 and a core backtrace (`docs/results/qnx-run5`); portable C engine (RFZ1/RFS1 formats, mutator, corpus, ddmin minimiser, coverage + cmplog), QNX platform layer on native message passing, three oracles (crash via ESRCH/EBADF + waitpid, hang via TimerTimeout + watchdog + SIGABRT core, deadline via ClockCycles), stateful replay; 46 host tests + C unit/integration tests; every QNX assumption verified in `docs/TODO-QNX.md` (A-T, F1-F4) | fuzz one real QNX resmgr (e.g. an SDP sample or a small open-source driver) besides `buggy_rm` |
| **Design** | complete, coherent product, not just a proof of concept | one command boots/deploys/fuzzes/triages (`resfuzz run`), Markdown+HTML report with minimised reproducers, backtraces, severity, replay command; `qnx-up.sh`, `--serial` | screenshots of the HTML report; a `resfuzz init` or a config template per target; polish report wording |
| **Potential Impact** | credible, specific case in a market QNX serves; why a real-time OS foundation matters | README "Potential Impact": automotive/medical/industrial resmgrs; ISO 26262 / IEC 62304 framing; deadline oracle via `ClockCycles`; time-to-bug from run5 (BUG3a 8 s, BUG2 13 s, BUG1 173 s, BUG3b 199 s) | 🟡 optional: one external citation / customer quote |
| **Innovation & AI Creativity** | creative, non-obvious use of QNX, genuine problem understanding | fuzzing *through* message passing (raw `_IO_DEVCTL` headers that lie about `nbytes`), a deadline oracle for real-time behaviour, stateful sequences, dictionary from headers resolved on target, optional LLM seed step that never invents dcmds | demonstrate the LLM seed step once and measure its effect; explain ASLR/PIE coverage finding |

## 4. Bonus prizes

| Bonus | Requirement | Evidence | Status |
|---|---|---|---|
| **Best Codelab/Tutorial** | URL of a codelab authored during the period that teaches a task on/for QNX 8.0 | [`docs/codelab/README.md`](codelab/README.md) written 15 Nov | 🟡 clean-machine timing + publish URL |
| **Best Use of QNX-specific Technologies** | creative/sophisticated use of message passing, resource managers, microkernel, boot customisation, real-time services, security policies | native `MsgSend` on resmgr connections, hand-built `_IO_DEVCTL`, `name_open` targets, `TimerTimeout` unblock, `ClockCycles`/`SYSPAGE_ENTRY(qtime)` deadlines, `dumper` cores, mkqnximage snippets (IFS + data partition + COM2 console), resmgr written against `iofunc`/`resmgr_msgget` | ✅ evidence; 🟡 a security-policy (secpol) angle is not covered |

## 5. Open gaps (owner decisions in bold)

1. **Create the public repository and push** (GitHub; About section shows Apache-2.0). Nothing has been published.
2. Demo video (< 3 min) on YouTube, public.
3. Devpost write-up (text description, QNX evidence list, category, links).
4. Codelab: written (`docs/codelab/README.md`); still need clean-box timing + publish URL.
5. ~~Commit one representative QNX report~~ done: `docs/results/qnx-run5`.
6. ~~Screenshots~~ done: `docs/img/report-qnx-run5.png`, `docs/img/qnx-up-boot.png`.
7. Optional strength: a second, real resource manager as a target; LLM seed step demo.

## 6. Campaign evidence (run5, 15 Nov 2026, QNX 8.0 / QEMU x86_64 TCG, 600 s, coverage on)

| Planted bug | Found at | Oracle | Minimised reproducer | Replays | Backtrace (top frame) |
|---|---|---|---|---|---|
| BUG3a deadline (COMPUTE slow path) | 7.7 s | deadline, 991 ms > 100 ms | 2 ops | 3/3 (exit 12) | n/a (no fault) |
| BUG2 NULL deref (GET_STATS) | 12.6 s | crash, ESRCH + SIGSEGV | 1 op | 3/3 (exit 10) | `buggy_devctl` buggy_logic.c:124 |
| BUG1 overflow (admin SET_NAME), stateful | 173.4 s | crash, ESRCH + SIGSEGV | 271-case history → 2 ops (SET_MODE "ADMN"/2, SET_NAME 34 B) | 3/3 (exit 10) | `buggy_devctl` buggy_logic.c:112 |
| BUG3b hang (n+1 wraps) | 198.5 s | hang, TimerTimeout + SIGABRT core | 1 op | 3/3 (exit 11) | `compute` buggy_logic.c:77 |

136,106 execs (227/s), 107 edges, 227 server restarts, 0 false-positive crashes (run4's EBADF
false positive on a raw `_IO_OPENFD` is fixed in `target/src/exec.c`).

## 7. Second target (SDP `devc-pty`, no planted bugs)

See [`docs/results/qnx-pty1/NOTES.md`](results/qnx-pty1/NOTES.md): 300 s, 0 crashes, 0 hangs, 20 flaky TCG deadlines.

## 8. AI seed step

Evidence in [`docs/results/ai-seeds/`](results/ai-seeds/): local Qwen2.5-Coder-1.5B via llama.cpp,
validator path exercised, three RFZ1 seeds written. Replayed on QNX: exit 10/10/11 (BUG1, BUG2, BUG3b).
See NOTES.md for the honesty note about the slow CPU completion.
