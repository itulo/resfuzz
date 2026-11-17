#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Rebuild docs/video/v4.mp4 from v3.2.mp4 + Kokoro af_heart narration.
# Needs: ffmpeg, ffprobe, Python 3 with kokoro-onnx (onnxruntime), and the
# Kokoro v1.0 model files (downloaded on first run to $KOKORO_DIR).
set -euo pipefail
cd "$(dirname "$0")"
KOKORO_DIR=${KOKORO_DIR:-$HOME/.cache/resfuzz-kokoro}
PYTHON=${PYTHON:-python3}
export KOKORO_DIR

if ! command -v ffmpeg >/dev/null || ! command -v ffprobe >/dev/null; then
    echo "need ffmpeg and ffprobe on PATH" >&2; exit 1
fi
if ! "$PYTHON" -c "import kokoro_onnx" 2>/dev/null; then
    echo "need a Python with kokoro-onnx installed (pip install kokoro-onnx). Set PYTHON= if it lives in a venv." >&2
    exit 1
fi

mkdir -p "$KOKORO_DIR"
if [[ ! -f $KOKORO_DIR/kokoro-v1.0.onnx || ! -f $KOKORO_DIR/voices-v1.0.bin ]]; then
    echo "downloading Kokoro v1.0 model into $KOKORO_DIR (~338 MB)"
    wget -q --show-progress -O "$KOKORO_DIR/kokoro-v1.0.onnx" \
      https://github.com/thewh1teagle/kokoro-onnx/releases/download/model-files-v1.0/kokoro-v1.0.onnx
    wget -q --show-progress -O "$KOKORO_DIR/voices-v1.0.bin" \
      https://github.com/thewh1teagle/kokoro-onnx/releases/download/model-files-v1.0/voices-v1.0.bin
fi

exec "$PYTHON" "$PWD/build.py"
