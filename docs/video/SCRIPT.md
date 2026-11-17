# Demo video script — resfuzz (target ≤ 2:45; hard limit < 3:00)

Format: 1920×1080, captions burned in, **no music, no third-party logos**.
All terminal footage is real output from QNX 8.0 under QEMU (TCG).

Cuts live in `docs/video/` (mp4 files are gitignored; rebuild from `v2-src/` / `v3-src/` / `v4-src/`):

| Cut | File | Duration | Audio |
|---|---|---|---|
| v2 | `docs/video/v2.mp4` | 2:09 | captions only |
| v3 | `docs/video/v3.mp4` | 2:09 | v2 picture plus spoken narration (flite `slt`) |
| v3.1 | `docs/video/v3.1.mp4` | 2:09 | same flite `slt` track, louder/more even mix |
| v3.2 | `docs/video/v3.2.mp4` | 2:09 | v3.1 audio and picture, closing card shows `github.com/itulo/resfuzz` |
| v4 | `docs/video/v4.mp4` | 2:09 | v3.2 picture (video bitstream unchanged) plus Kokoro `af_heart` narration |

v3 / v3.1 / v3.2 use the box's `flite` (voice `slt`). That track is clearly
robotic. v4 replaces only the audio, with the same scene timings and the same
picture as v3.2 (including the public repo URL on the close card).

v4 voice: Kokoro-82M ONNX, speaker `af_heart`, via `kokoro-onnx` (free, local,
no account). Piper was tried first (`en_US-lessac-medium`, `en_US-ryan-high`,
`en_GB-alan-medium`); lessac and ryan can fit the 5 s title at near-natural
speed, but Kokoro is the more natural neural voice of the two tools, and
`af_heart` is the usual first choice among its English speakers. `edge-tts`
was not used. Spoken lines match the table below, with a few pronunciation
spellings in `docs/video/v4-src/lines.tsv` (notably `P-T-Y`, so it is not
read as "tie"; "dev control"; "two hundred and seventy-one"; "code lab").
Only the title line is slightly sped up (about 1.10×) to fit the 5 s pad;
the other eleven lines play at natural speed and are padded with trailing
silence. Loudness: two-pass `loudnorm` to −14 LUFS with a −1.5 dBTP wav
target so the AAC encode stays at or under −1 dBTP. Rebuild:
`docs/video/v4-src/build.sh` (needs `kokoro-onnx` and ~338 MB of model files
in `$KOKORO_DIR`, default `~/.cache/resfuzz-kokoro`).

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
| 12 | 2:03–2:09 | Closing card: `github.com/itulo/resfuzz`, codelab, Apache-2.0 | "Open source, with a one-hour codelab. resfuzz." |

The spoken line in shot 9 is still accurate for the pty campaign shown on
screen. Later official stock campaigns (`devb-ram`, `mqueue`, `devf-ram`,
attach-only `pipe` and `random`) are documented in `docs/results/` and in the
Devpost draft; they are not in this cut, and the narration was left unchanged
so it stays in sync with the 2:09 picture.

## Shot list / capture notes

- Shots 3, 4, 7, 8: asciinema recordings rendered with agg (no fonts/logos beyond the terminal).
- Shot 6: crops of `docs/results/qnx-run5/report.html`.
- Shots 1, 2, 5, 9, 10, 11, 12: plain HTML cards rendered to PNG — text only.
- Avoid: QNX/BlackBerry logos, Devpost branding, any music. The word "QNX" in
  text is a factual product reference (with the trademark note in the README).
- Closing card shows the public repo URL `github.com/itulo/resfuzz` (v3.2).
