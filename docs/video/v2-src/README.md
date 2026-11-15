# Demo video v2 — sources

All terminal footage is real output recorded with asciinema (72×15) against the
QNX 8.0 QEMU target (TCG); `fmt_events.py` / `fmt_stats.py` and the `sed`/`cut` in
the scene scripts only re-lay-out text to fit 72 columns. The backtrace scene
prints the gdb output recorded from the run5 dumper core
(`docs/results/qnx-run5/backtraces/38d435f06c3e3629.bt.txt`); the prompt says `# run5`.

1. Boot the target (`scripts/qnx-up.sh`), start a short campaign on it, and record
   `live.sh`, then stop the campaign and record `bug1.sh`, `bt.sh`; record `boot.sh`
   from a stopped target. Example: `asciinema rec --cols 72 --rows 15 -c "bash live.sh" $WORK/live.cast`.
   The casts used for v2 are committed here.
2. `WORK=~/vid2 ./render.sh` — agg (font 40) for the casts, headless Chrome for the
   cards and the zoomed report crops (`report_crops.py`).
3. `WORK=~/vid2 ./build.py` — composes `docs/video/v2.mp4` (gitignored): boot at 4×
   with a label, highlight boxes and callouts on the key lines, captions at 46 px on a
   semi-transparent panel. No music, no logos.

Stills: `docs/video/v2-frames/`.
