#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Build docs/video/v4.mp4: v3.2 picture + Kokoro (af_heart) narration.

Requires: ffmpeg, ffprobe, kokoro-onnx, and the Kokoro v1.0 ONNX model
(see build.sh). The video stream is copied; only the audio is replaced.
Loudness target: −14 LUFS, true peak ≤ −1 dBTP.
"""
from __future__ import annotations

import json, os, re, subprocess, sys, wave
from pathlib import Path

import numpy as np

D = Path(__file__).resolve().parent
REPO = D.parents[2]
V32 = D.parent / "v3.2.mp4"
V4 = D.parent / "v4.mp4"
LINES = D / "lines.tsv"
VOICE = os.environ.get("KOKORO_VOICE", "af_heart")
MODEL_DIR = Path(os.environ.get("KOKORO_DIR", Path.home() / ".cache/resfuzz-kokoro"))
TARGET_LUFS = -14.0
TARGET_TP = -1.0          # final-file true-peak ceiling (dBTP)
LOUDNORM_TP = -1.5         # wav target; leaves headroom for AAC overshoot
# Leave this much slack at the end of a scene so a line is never trimmed mid-word.
MIN_SLACK = 0.15
SR_OUT = 44100


def run(cmd, **kw):
    return subprocess.run(cmd, check=True, **kw)


def probe_duration(path: Path) -> float:
    out = subprocess.check_output(
        ["ffprobe", "-v", "error", "-show_entries", "format=duration",
         "-of", "default=nw=1:nk=1", str(path)],
        text=True,
    ).strip()
    return float(out)


def load_lines():
    rows = []
    for line in LINES.read_text().splitlines():
        if not line.strip():
            continue
        name, dur, text = line.split("\t", 2)
        rows.append((name, float(dur), text))
    return rows


def synthesize(kokoro, text: str, speed: float) -> tuple[np.ndarray, int]:
    audio, sr = kokoro.create(
        text,
        voice=VOICE,
        speed=speed,
        lang="en-us",
        sentence_pause=0.16,
        clause_pause=0.08,
        trim=True,
    )
    audio = np.asarray(audio, dtype=np.float32)
    return audio, sr


def fit_line(kokoro, name: str, pad: float, text: str) -> tuple[np.ndarray, int, float, float]:
    """Return mono float audio that is exactly `pad` seconds long, plus speed used and raw duration."""
    speed = 1.0
    audio, sr = synthesize(kokoro, text, speed)
    raw = audio.shape[0] / sr
    for _ in range(4):
        if raw <= pad - MIN_SLACK:
            break
        # Kokoro speed is not perfectly linear; iterate until the line fits.
        speed *= raw / (pad - MIN_SLACK)
        audio, sr = synthesize(kokoro, text, speed)
        raw = audio.shape[0] / sr
    # Last resort: trim a few ms of tail only — never mid-word.
    max_samples = int((pad - 0.04) * sr)
    if audio.shape[0] > max_samples:
        audio = audio[:max_samples]
        raw = audio.shape[0] / sr
    # 80 ms pre-roll when there is room; otherwise split remaining slack.
    slack = pad - raw
    preroll = 0.08 if slack >= 0.16 else max(0.0, slack / 2)
    n_pre = int(round(preroll * sr))
    n_body = audio.shape[0]
    n_total = int(round(pad * sr))
    n_post = n_total - n_pre - n_body
    if n_post < 0:
        n_pre = max(0, n_pre + n_post)
        n_post = n_total - n_pre - n_body
    out = np.zeros(n_total, dtype=np.float32)
    start = n_pre
    end = start + n_body
    if end > n_total:
        audio = audio[: n_total - start]
        end = n_total
    out[start:end] = audio
    return out, sr, speed, raw


def resample(audio: np.ndarray, sr_in: int, sr_out: int) -> np.ndarray:
    if sr_in == sr_out:
        return audio
    # Write a temp wav and let ffmpeg resample (better than linear).
    tmp_in = D / "_tmp_in.wav"
    tmp_out = D / "_tmp_out.wav"
    write_wav(tmp_in, audio, sr_in)
    run(["ffmpeg", "-y", "-v", "error", "-i", str(tmp_in),
         "-ar", str(sr_out), "-ac", "1", str(tmp_out)])
    audio2, sr2 = read_wav(tmp_out)
    tmp_in.unlink()
    tmp_out.unlink()
    assert sr2 == sr_out
    return audio2


def write_wav(path: Path, audio: np.ndarray, sr: int) -> None:
    audio = np.clip(audio, -1.0, 1.0)
    pcm = (audio * 32767.0).astype(np.int16)
    with wave.open(str(path), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(sr)
        w.writeframes(pcm.tobytes())


def read_wav(path: Path) -> tuple[np.ndarray, int]:
    with wave.open(str(path), "rb") as w:
        sr = w.getframerate()
        n = w.getnchannels()
        sw = w.getsampwidth()
        raw = w.readframes(w.getnframes())
    if sw == 2:
        data = np.frombuffer(raw, dtype=np.int16).astype(np.float32) / 32767.0
    else:
        raise SystemExit(f"unexpected sample width {sw} in {path}")
    if n > 1:
        data = data.reshape(-1, n).mean(axis=1)
    return data, sr


def loudnorm_two_pass(src: Path, dest: Path) -> dict:
    err = subprocess.run(
        ["ffmpeg", "-hide_banner", "-i", str(src),
         "-af", f"loudnorm=I={TARGET_LUFS}:TP={LOUDNORM_TP}:LRA=11:print_format=json",
         "-f", "null", "-"],
        capture_output=True, text=True,
    )
    m = re.search(r"\{[^{}]*\"input_i\"[^{}]*\}", err.stderr, re.S)
    if not m:
        sys.stderr.write(err.stderr[-2000:])
        raise SystemExit("loudnorm first pass produced no JSON")
    d = json.loads(m.group(0))
    # Dynamic (not linear) two-pass: long scene-end pads pull gated LUFS down,
    # and linear mode cannot boost enough without breaking the peak ceiling.
    # TP is aimed a little under the final ceiling so AAC overshoot stays ≤ −1 dBTP.
    filt = (
        f"loudnorm=I={TARGET_LUFS}:TP={LOUDNORM_TP}:LRA=11:"
        f"measured_I={d['input_i']}:measured_TP={d['input_tp']}:"
        f"measured_LRA={d['input_lra']}:measured_thresh={d['input_thresh']}:"
        f"offset={d['target_offset']}"
    )
    run(["ffmpeg", "-y", "-v", "error", "-i", str(src), "-af", filt,
         "-ar", str(SR_OUT), "-ac", "1", str(dest)])
    return d


def measure_loudness(path: Path) -> dict:
    err = subprocess.run(
        ["ffmpeg", "-hide_banner", "-i", str(path),
         "-af", f"loudnorm=I={TARGET_LUFS}:TP={TARGET_TP}:LRA=11:print_format=json",
         "-f", "null", "-"],
        capture_output=True, text=True,
    )
    m = re.search(r"\{[^{}]*\"input_i\"[^{}]*\}", err.stderr, re.S)
    if not m:
        raise SystemExit("loudness measure failed")
    return json.loads(m.group(0))


def silence_report(path: Path, noise_db: float = -40.0, min_d: float = 0.6) -> str:
    err = subprocess.run(
        ["ffmpeg", "-hide_banner", "-i", str(path),
         "-af", f"silencedetect=noise={noise_db}dB:d={min_d}",
         "-f", "null", "-"],
        capture_output=True, text=True,
    )
    lines = [ln for ln in err.stderr.splitlines() if "silence_" in ln]
    return "\n".join(lines)


def main() -> int:
    if not V32.is_file():
        print(f"missing {V32} — build v3.2 first", file=sys.stderr)
        return 1
    model = MODEL_DIR / "kokoro-v1.0.onnx"
    voices = MODEL_DIR / "voices-v1.0.bin"
    if not model.is_file() or not voices.is_file():
        print(
            f"Kokoro model not found in {MODEL_DIR}.\n"
            "Download:\n"
            f"  mkdir -p {MODEL_DIR}\n"
            "  wget -O {0}/kokoro-v1.0.onnx "
            "https://github.com/thewh1teagle/kokoro-onnx/releases/download/model-files-v1.0/kokoro-v1.0.onnx\n"
            "  wget -O {0}/voices-v1.0.bin "
            "https://github.com/thewh1teagle/kokoro-onnx/releases/download/model-files-v1.0/voices-v1.0.bin\n"
            .format(MODEL_DIR),
            file=sys.stderr,
        )
        return 1

    from kokoro_onnx import Kokoro
    kokoro = Kokoro(str(model), str(voices))
    rows = load_lines()
    parts = []
    sr = None
    print(f"voice={VOICE}")
    for name, pad, text in rows:
        audio, sr_in, speed, raw = fit_line(kokoro, name, pad, text)
        wav = D / f"{name}.wav"
        write_wav(wav, audio, sr_in)
        # Keep scene files at native rate; resample when concatenating.
        parts.append((name, pad, speed, raw, wav, sr_in, text))
        print(f"  {name:8} pad={pad:5.1f}s  spoken={raw:6.3f}s  speed={speed:.3f}  slack={pad-raw:+.3f}s")

    # Concatenate at a common rate.
    chunks = []
    for name, pad, speed, raw, wav, sr_in, text in parts:
        audio, _ = read_wav(wav)
        audio = resample(audio, sr_in, SR_OUT)
        want = int(round(pad * SR_OUT))
        if audio.shape[0] < want:
            audio = np.pad(audio, (0, want - audio.shape[0]))
        elif audio.shape[0] > want:
            audio = audio[:want]
        chunks.append(audio)
    narration = np.concatenate(chunks)
    raw_nar = D / "narration_raw.wav"
    write_wav(raw_nar, narration, SR_OUT)
    print(f"raw narration {narration.shape[0]/SR_OUT:.3f}s")

    normed = D / "narration.wav"
    meas = loudnorm_two_pass(raw_nar, normed)
    print("loudnorm first-pass:", json.dumps({k: meas[k] for k in
          ("input_i", "input_tp", "input_lra", "target_offset")}, indent=2))

    vdur = probe_duration(V32)
    adur = probe_duration(normed)
    print(f"v3.2 video {vdur:.3f}s  narration {adur:.3f}s")

    run([
        "ffmpeg", "-y", "-v", "error",
        "-i", str(V32), "-i", str(normed),
        "-map", "0:v", "-map", "1:a",
        "-c:v", "copy",
        "-c:a", "aac", "-b:a", "96k", "-ac", "1", "-ar", str(SR_OUT),
        "-shortest",
        str(V4),
    ])

    final_d = probe_duration(V4)
    loud = measure_loudness(V4)
    print(f"wrote {V4}  duration={final_d:.3f}s")
    print("final loudness:", json.dumps({k: loud[k] for k in
          ("input_i", "input_tp", "input_lra")}, indent=2))
    sil = silence_report(V4)
    print("silencedetect (>0.6s below -40 dB):")
    print(sil or "(none)")
    # Sanity: duration under 3:00, audio present, peak constraint.
    if final_d >= 180:
        print("ERROR: duration is not under 3:00", file=sys.stderr)
        return 1
    if abs(final_d - 129.0) > 0.25:
        print(f"WARNING: expected ~129 s, got {final_d:.3f}s", file=sys.stderr)
    tp = float(loud["input_tp"])
    if tp > -1.0 + 1e-6:
        print(f"ERROR: true peak {tp} dBTP exceeds −1 dBTP", file=sys.stderr)
        return 1
    # Drop raw concat working file; keep per-scene wavs and narration.wav locally
    # (gitignored) in case we need to inspect them. Callers may delete them.
    (D / "_tmp_in.wav").unlink(missing_ok=True)
    (D / "_tmp_out.wav").unlink(missing_ok=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
