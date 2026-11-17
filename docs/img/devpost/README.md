# Devpost gallery images

1500×1000 PNGs (3:2, the ratio Devpost's gallery and thumbnail use). Every figure
in them is read from a file in this repo; the source path is printed in each
image's footer.

| File | Shows | Source |
|---|---|---|
| `thumbnail.png` (= `01-hero.png`) | name, one-line pitch, run5 headline numbers | `docs/results/qnx-run5/` |
| `02-how.png` | host ⇄ QNX target ⇄ resource manager, the three oracles | `target/platform/platform_qnx.c`, `host/resfuzz/` |
| `03-report.png` | the real HTML report from run5 | `docs/results/qnx-run5/report.html` |
| `04-minimise.png` | BUG1: 271 recorded messages → 2 ops, replay 3/3 | `docs/results/qnx-run5/min/`, `docs/video/v2-src/bug1.cast` |
| `05-backtrace.png` | `dumper` core → `ntox86_64-gdb` backtrace | `docs/results/qnx-run5/backtraces/38d435f06c3e3629.bt.txt` |
| `06-stock.png` | six unmodified SDP services, 0 reproducible crashes/hangs | `docs/results/qnx-*/stats.json` |

Rebuild: `docs/img/devpost/src/render.sh` (headless Chrome).
