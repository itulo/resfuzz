#!/bin/bash
# Render the Devpost gallery images (1500x1000, 3:2) with headless Chrome.
# All figures come from files under docs/results/ (paths in each image's footer).
set -e; cd "$(dirname "$0")"; OUT=..; R=$(cd ../../../.. && pwd)
mkdir -p /tmp/g
C="google-chrome --headless=new --no-sandbox --disable-gpu --hide-scrollbars"
$C --window-size=1060,800 --force-device-scale-factor=1 --virtual-time-budget=2000 --screenshot=/tmp/g/report-z.png "file://$R/docs/results/qnx-run5/report.html" 2>/dev/null
for f in 01-hero 02-how 03-report 04-minimise 05-backtrace 06-stock; do
  $C --force-device-scale-factor=1 --allow-file-access-from-files --window-size=1500,1000 --virtual-time-budget=2000 --screenshot=$OUT/$f.png "file://$PWD/$f.html" 2>/dev/null
done
cp $OUT/01-hero.png $OUT/thumbnail.png
