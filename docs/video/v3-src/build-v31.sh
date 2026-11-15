#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Remaster docs/video/v3.1.mp4 from the existing v3 narration.wav (no re-flite).
# Same picture and timing as v3; louder mix via two-pass loudnorm (−14 LUFS,
# true peak −1.5 dB). Other flite voices and duration_stretch overrun the
# 5 s title pad, so this script does not re-synthesize.
set -euo pipefail
cd "$(dirname "$0")"
V2=../v2.mp4
V31=../v3.1.mp4
NAR=narration.wav
[[ -f $V2 && -f $NAR ]] || { echo "need $V2 and $NAR (run build.sh first)" >&2; exit 1; }
command -v ffmpeg >/dev/null || { echo "need ffmpeg" >&2; exit 1; }

JSON=$(ffmpeg -hide_banner -i "$NAR" \
  -af "highpass=f=80,lowpass=f=6000,loudnorm=I=-14:TP=-1.5:LRA=11:print_format=json" \
  -f null - 2>&1 | python3 -c 'import sys,re,json; s=sys.stdin.read(); m=re.search(r"\{[^{}]*\"input_i\"[^{}]*\}", s, re.S); d=json.loads(m.group(0)); print(d["input_i"], d["input_tp"], d["input_lra"], d["input_thresh"], d["target_offset"])')
set -- $JSON
measured_I=$1; measured_TP=$2; measured_LRA=$3; measured_thresh=$4; offset=$5
echo "measured I=$measured_I TP=$measured_TP LRA=$measured_LRA thresh=$measured_thresh offset=$offset"

ffmpeg -y -v error -i "$V2" -i "$NAR" \
  -filter_complex "[1:a]highpass=f=80,lowpass=f=6000,loudnorm=I=-14:TP=-1.5:LRA=11:measured_I=${measured_I}:measured_TP=${measured_TP}:measured_LRA=${measured_LRA}:measured_thresh=${measured_thresh}:offset=${offset}:linear=true[a]" \
  -map 0:v -map "[a]" -c:v copy -c:a aac -b:a 96k -ac 1 -ar 44100 -shortest \
  "$V31"
echo "wrote $V31"
ffprobe -v error -show_entries format=duration,size -of default=nw=1 "$V31"
