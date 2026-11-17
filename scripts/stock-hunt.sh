#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Long campaigns against unmodified QNX 8.0 services (no planted bugs).
# Usage:
#   scripts/stock-hunt.sh                  # all default targets, sequential
#   scripts/stock-hunt.sh procfs slog      # subset
#   scripts/stock-hunt.sh --seconds 600 procfs
#
# Results land in resfuzz-results/qnx-<name>1/ and are summarised on stdout.
# Does NOT publish. Security-looking findings stay in the result dir; report them
# to the owner before anything goes into docs/ or the public repo.
set -euo pipefail
repo=$(cd "$(dirname "$0")/.." && pwd)
export PATH=${HOME}/.local/bin:/usr/sbin:$PATH
seconds_override=
targets=()
while (($#)); do
  case $1 in
    --seconds) seconds_override=$2; shift 2 ;;
    -h|--help) sed -n '2,14p' "$0"; exit 0 ;;
    *) targets+=("$1"); shift ;;
  esac
done
((${#targets[@]})) || targets=(procfs procfs-as slog slog2 ser8250 iosock fs-ram)

boot_log=$(mktemp)
"$repo/scripts/qnx-up.sh" | tee "$boot_log"
ip=$(grep -Eo 'target up at ([0-9.]+)' "$boot_log" | awk '{print $NF}' | tail -1)
rm -f "$boot_log"
[[ -n $ip ]] || { echo "stock-hunt: no IP" >&2; exit 1; }
key=${QNX_SSH_KEY:-$HOME/.ssh/qnx_resfuzz_ed25519}
sdp=${QNX_SDP:-$HOME/qnx800}

if [[ -x $repo/.venv/bin/python ]]; then py=$repo/.venv/bin/python
elif [[ -x /tmp/venv/bin/python ]]; then py=/tmp/venv/bin/python
else python3 -m venv "$repo/.venv" && "$repo/.venv/bin/pip" -q install -e "$repo/host"; py=$repo/.venv/bin/python
fi

# Ensure QNX agent + helpers exist.
source "$sdp/qnxsdp-env.sh" >/dev/null
make -C "$repo/target" qnx >/dev/null
# rmwrap for private spawns
if [[ ! -x $repo/target/examples/rmwrap ]]; then
  qcc -Vgcc_ntox86_64 -o "$repo/target/examples/rmwrap" "$repo/target/examples/rmwrap.c"
fi

rewrite() {
  # $1=src.toml $2=dst.toml $3=out_dir
  python3 - "$1" "$2" "$3" "$key" "$sdp" <<'PY'
import pathlib, sys
src, dst, out, key, sdp = sys.argv[1:6]
home = pathlib.Path.home()
text = pathlib.Path(src).read_text()
text = text.replace("/home/me/.ssh/id_ed25519.pub", str(home / ".ssh/qnx_resfuzz_ed25519.pub"))
text = text.replace("/home/me/.ssh/id_ed25519", key)
text = text.replace("/home/me/qnx800", sdp)
# force out_dir
import re
text = re.sub(r'^out_dir\s*=\s*.*$', f'out_dir = "{out}"', text, count=1, flags=re.M)
pathlib.Path(dst).write_text(text)
PY
}

for name in "${targets[@]}"; do
  cfg=$repo/host/examples/qemu-${name}.toml
  [[ -f $cfg ]] || { echo "stock-hunt: no config $cfg" >&2; exit 1; }
  out=$repo/resfuzz-results/qnx-${name}1
  mkdir -p "$out"
  run_cfg=$(mktemp --suffix=.toml)
  rewrite "$cfg" "$run_cfg" "$out"
  sec_args=()
  [[ -n $seconds_override ]] && sec_args=(--seconds "$seconds_override")
  extra_args=()
  pre_args=()
  case $name in
    ser8250)
      # Private unit 3 via rmwrap. Do NOT slay the image's console ser8250 (ser1/ser2).
      extra_args=(--extra "$repo/target/examples/rmwrap" --resmgr "$sdp/target/qnx/x86_64/sbin/devc-ser8250")
      pre_args=(--pre 'for p in $(pidin ar | awk "/\\/data\\/resfuzz\\/devc-ser8250|ser3drv|rmwrap/ {print \$1}"); do kill -9 $p 2>/dev/null; done; sleep 1; true')
      ;;
    iosock|slog|slog2|procfs|procfs-as)
      # attach-only: leave system services alone
      ;;
  esac
  echo "===== stock-hunt $name on $ip → $out ====="
  set +e
  "$py" "$repo/scripts/run-existing.py" -c "$run_cfg" --host "$ip" --identity "$key" \
    "${sec_args[@]}" "${extra_args[@]}" "${pre_args[@]}" | tee "$out/run.log"
  rc=${PIPESTATUS[0]}
  set -e
  rm -f "$run_cfg"
  # summarise without dumping payloads
  if [[ -f $out/stats.json ]]; then
    "$py" - "$out" <<'PY'
import json,sys
from pathlib import Path
out=Path(sys.argv[1])
st=json.loads((out/'stats.json').read_text())
rep=(out/'report.md').read_text() if (out/'report.md').exists() else ''
crash=hang=dead=0
for line in rep.splitlines():
  if '| high | CRASH' in line or '| medium | CRASH' in line: crash+=1
  if ' HANG ' in line and line.startswith('|'): hang+=1
  if ' DEADLINE ' in line and line.startswith('|') and 'yes' in line: dead+=1
print(json.dumps({
  "target": st.get("target"), "elapsed_s": st.get("elapsed_s"), "execs": st.get("execs"),
  "crashes": st.get("crashes"), "hangs": st.get("hangs"),
  "deadline_misses": st.get("deadline_misses"),
  "report_reproducible_crashes_rows": crash,
  "report_reproducible_hang_rows": hang,
  "report_reproducible_deadline_rows": dead,
  "out": str(out),
}, indent=2))
PY
  fi
  echo "stock-hunt $name exit $rc"
done
