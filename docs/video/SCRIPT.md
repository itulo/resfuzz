# Demo video script — resfuzz (target ≤ 2:45; hard limit < 3:00)

Format: 1920×1080, captions burned in, **no music, no third-party logos**.
Narration optional (record your own voice over the captions; the draft cut is
caption-only). All terminal footage is real output from QNX 8.0 under QEMU (TCG).

| # | Time | Shot (what is on screen) | Caption / narration |
|---|---|---|---|
| 1 | 0:00–0:08 | Title card: "resfuzz — fuzzing QNX 8.0 resource managers" | "resfuzz: find crashes, hangs and deadline misses in QNX resource managers." |
| 2 | 0:08–0:22 | Card: client → `MsgSend` → resource manager; three oracle icons (text only) | "On QNX, drivers and services are resource managers. Every client reaches them by message passing. resfuzz attacks that interface on the real OS." |
| 3 | 0:22–0:42 | Terminal: `scripts/qnx-up.sh` → `QNX noname 8.0.0 … x86_64`, "target up at 172.31.1.N" | "One script builds a mkqnximage QEMU target with the tool baked in, boots it headless under TCG and waits for ssh." |
| 4 | 0:42–1:05 | Terminal: live `events.jsonl` on the target, then `stats.json` (platform qnx, execs, edges) | "The agent runs on QNX next to the server. It mutates open, read, write, devctl and raw MsgSend, with coverage feedback." |
| 5 | 1:05–1:30 | Screenshot: HTML report (`docs/img/report-qnx-run5.png`), slow pan | "Ten minutes in, all four planted bugs: a deadline miss at 8 seconds, a NULL dereference at 13, a hang at 3 minutes, and a stateful overflow." |
| 6 | 1:30–2:00 | Terminal: decode the 271-case BUG1 sequence → 2-op minimised case; replay on QNX ×3 → `exit 10` | "The overflow only happens after admin mode. resfuzz recorded 271 messages of history and shrank them to two. They crash a fresh server every time." |
| 7 | 2:00–2:15 | Terminal: gdb backtrace from the dumper core (`buggy_devctl` → `_resmgr_io_handler` → `main`) | "Every crash and hang comes with a dumper core and a backtrace through QNX's own resmgr layer." |
| 8 | 2:15–2:32 | Card: real SDP `devc-pty`: 83,270 execs, 0 crashes, 0 hangs; AI seeds: 3 validated → crash/crash/hang | "Pointed at the SDP's own pty driver it found nothing, and says so. An optional AI step proposes seeds; a validator keeps dcmd numbers from the target." |
| 9 | 2:32–2:42 | Closing card: repo URL placeholder, codelab, Apache-2.0 | "Open source, with a one-hour codelab. resfuzz." |

## Shot list / capture notes

- Shots 3, 4, 6, 7: asciinema recordings rendered with agg (no fonts/logos beyond the terminal).
- Shot 5: headless-Chrome screenshot of `docs/results/qnx-run5/report.html`.
- Shots 1, 2, 8, 9: plain HTML cards rendered to PNG — text only.
- Avoid: QNX/BlackBerry logos, Devpost branding, any music. The word "QNX" in
  text is a factual product reference (with the trademark note in the README).
- Final cut: re-record shot 3 with the public repo URL in the prompt; replace the
  closing-card placeholder with the real GitHub URL.
