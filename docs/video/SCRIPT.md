# Demo video script — resfuzz (target ≤ 2:45; hard limit < 3:00)

Format: 1920×1080, captions burned in, **no music, no third-party logos**.
All terminal footage is real output from QNX 8.0 under QEMU (TCG).

Two cuts live in `docs/video/` (both gitignored; rebuild from `v2-src/` / `v3-src/`):

| Cut | File | Duration | Audio |
|---|---|---|---|
| v2 | `docs/video/v2.mp4` | 2:09 | captions only |
| v3 | `docs/video/v3.mp4` | 2:09 | v2 picture plus spoken narration |

v3 narration is generated with the box's `flite` (voice `slt`, no large model
download). The voice is clearly synthetic; treat v3 as a draft. A cleaner
human recording can replace the `flite` track later without changing the picture.
Rebuild: `docs/video/v3-src/build.sh`.

Times below are the **v2/v3** scene lengths from `docs/video/v2-src/build.py`.

| # | Time | Shot (what is on screen) | Caption / narration |
|---|---|---|---|
| 1 | 0:00–0:05 | Title card: "resfuzz — fuzzing QNX 8.0 resource managers" | "resfuzz finds crashes, hangs and deadline misses in QNX resource managers." |
| 2 | 0:05–0:16 | Card: client → `MsgSend` → resource manager; three oracle icons | "On QNX, drivers and services are resource managers. Every client reaches them by message passing. resfuzz attacks that interface on the real operating system." |
| 3 | 0:16–0:23 | Terminal: `scripts/qnx-up.sh` (4× speed) → target up, ssh | "One script builds a QEMU target, boots it headless under TCG, and waits for SSH." |
| 4 | 0:23–0:37 | Terminal: live `events.jsonl` / `stats.json` on the target | "The agent runs on QNX next to the server. It mutates open, read, write, devctl and raw messages, with coverage feedback." |
| 5 | 0:37–0:53 | Card: QNX-specific technology (MsgSend, ClockCycles, TimerTimeout, dumper) | "It uses native message passing, ClockCycles for deadlines, TimerTimeout for hangs, and dumper cores for backtraces." |
| 6 | 0:53–1:02 | Report crops: the four planted bugs and first-seen times | "Ten minutes in, all four planted bugs: a deadline miss, a null dereference, a hang, and a stateful overflow." |
| 7 | 1:02–1:15 | Terminal: 271-case BUG1 history → 2-op minimised case; replay 3/3 | "The overflow only happens after admin mode. resfuzz recorded 271 messages and shrank them to two. They crash a fresh server every time." |
| 8 | 1:15–1:25 | Terminal: gdb backtrace from the dumper core | "Every crash and hang comes with a dumper core and a backtrace through QNX's own resource manager layer." |
| 9 | 1:25–1:34 | Card: real SDP `devc-pty` — 83,270 execs, 0 crashes, 0 hangs | "Pointed at the SDP's own pty driver, it found nothing, and says so." |
| 10 | 1:34–1:49 | Impact card | "That honesty matters. Public QNX advisories have documented resource-manager bugs in the field. Teams can drop in their own server and run the same loop." |
| 11 | 1:49–2:03 | Results card (run5 + AI seeds) | "An optional AI step proposes seeds. A validator keeps command numbers from the target, never from the model." |
| 12 | 2:03–2:09 | Closing card: repo URL placeholder, codelab, Apache-2.0 | "Open source, with a one-hour codelab. resfuzz." |

## Shot list / capture notes

- Shots 3, 4, 7, 8: asciinema recordings rendered with agg (no fonts/logos beyond the terminal).
- Shot 6: crops of `docs/results/qnx-run5/report.html`.
- Shots 1, 2, 5, 9, 10, 11, 12: plain HTML cards rendered to PNG — text only.
- Avoid: QNX/BlackBerry logos, Devpost branding, any music. The word "QNX" in
  text is a factual product reference (with the trademark note in the README).
- Final cut: replace the closing-card placeholder with the real GitHub URL
  once the repo is public.
