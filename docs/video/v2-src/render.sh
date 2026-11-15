#!/bin/bash
# Render recorded casts (agg) and HTML cards/report crops (headless Chrome) into $WORK.
# Casts are recorded first with the scene scripts against a booted target, e.g.
#   asciinema rec --cols 72 --rows 15 -c "bash live.sh" $WORK/live.cast
set -e
D=$(cd "$(dirname "$0")" && pwd); WORK=${WORK:-$HOME/vid2}; AGG=${AGG:-$HOME/vid/agg}
cd "$WORK"
$AGG --font-size 40 --idle-time-limit 30  --last-frame-duration 0.5 boot.cast boot.gif
for s in live bug1 bt; do $AGG --font-size 40 --idle-time-limit 1.5 --last-frame-duration 0.5 $s.cast $s.gif; done
python3 "$D/report_crops.py" "$WORK"
shot(){ google-chrome --headless --disable-gpu --hide-scrollbars --window-size=1920,1080 --virtual-time-budget=2000 --screenshot="$WORK/$2.png" "file://$1" 2>/dev/null; }
for c in title iface qnx impact results close; do shot "$D/$c.html" $c; done
for r in r1 r2; do shot "$WORK/$r.html" $r; done
