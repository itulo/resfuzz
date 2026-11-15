#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Rebuild docs/video/v3.mp4: v2 picture + flite (voice slt) narration.
# Requires: flite, ffmpeg, docs/video/v2.mp4 (from v2-src/build.py).
set -euo pipefail
cd "$(dirname "$0")"
V2=../v2.mp4
V3=../v3.mp4
[[ -f $V2 ]] || { echo "missing $V2 — build v2 first" >&2; exit 1; }
command -v flite >/dev/null && command -v ffmpeg >/dev/null || {
    echo "need flite and ffmpeg on PATH" >&2; exit 1
}

while IFS=$'\t' read -r name dur text; do
    [[ -z ${name:-} ]] && continue
    printf '%s\n' "$text" > "$name.txt"
    flite -voice slt -f "$name.txt" -o "$name.wav"
    ffmpeg -y -v error -i "$name.wav" \
        -af "apad=whole_dur=${dur},atrim=0:${dur},asetpts=PTS-STARTPTS" \
        -ar 16000 -ac 1 "${name}_pad.wav"
    echo "$name $dur"
done < lines.tsv

# concat list is written in scene order (same as lines.tsv)
: > concat.txt
while IFS=$'\t' read -r name dur text; do
    [[ -z ${name:-} ]] && continue
    echo "file '${name}_pad.wav'" >> concat.txt
done < lines.tsv
ffmpeg -y -v error -f concat -safe 0 -i concat.txt -c copy narration.wav
ffmpeg -y -v error -i "$V2" -i narration.wav \
    -filter_complex "[1:a]highpass=f=80,lowpass=f=6000,volume=1.0[a]" \
    -map 0:v -map "[a]" -c:v copy -c:a aac -b:a 96k -ac 1 -ar 44100 -shortest \
    "$V3"
echo "wrote $V3"
ffprobe -v error -show_entries format=duration,size -of default=nw=1 "$V3"
