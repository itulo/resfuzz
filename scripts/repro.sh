#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Full QNX path for judges: boot (or reuse) the QEMU target, run a short campaign
# against buggy_rm, write a report, and check the planted bugs were found.
#
#   scripts/repro.sh              # 120 s campaign → resfuzz-results/repro/
#   scripts/repro.sh --seconds 60
#   scripts/repro.sh --out /tmp/rf-repro
#
# Needs QNX SDP 8.0 (QNX_SDP or ~/qnx800), qemu-system-x86_64, and the packages
# listed in docs/codelab/README.md. TCG only (scripts/qnx-up.sh refuses /dev/kvm).
set -euo pipefail
repo=$(cd "$(dirname "$0")/.." && pwd)
out=$repo/resfuzz-results/repro
seconds=120
while (($#)); do
  case $1 in
    --seconds) seconds=$2; shift 2 ;;
    --out) out=$2; shift 2 ;;
    -h|--help) sed -n '2,12p' "$0"; exit 0 ;;
    *) echo "unknown arg: $1" >&2; exit 2 ;;
  esac
done

export PATH=${HOME}/.local/bin:/usr/sbin:$PATH
# Boot (or reconnect to) the QNX 8.0 QEMU target. Captures the IP from the last line.
boot_log=$(mktemp)
trap 'rm -f "$boot_log"' EXIT
"$repo/scripts/qnx-up.sh" | tee "$boot_log"
ip=$(grep -Eo 'target up at ([0-9.]+)' "$boot_log" | awk '{print $NF}' | tail -1)
[[ -n $ip ]] || { echo "repro: no IP from qnx-up.sh" >&2; exit 1; }

key=${QNX_SSH_KEY:-$HOME/.ssh/qnx_resfuzz_ed25519}
cfg=$repo/host/examples/qemu.toml
# Rewrite /home/me paths to this host for a one-shot run without editing the example.
run_cfg=$(mktemp --suffix=.toml)
trap 'rm -f "$boot_log" "$run_cfg"' EXIT
python3 - "$cfg" "$run_cfg" "$out" "$key" <<'PY'
import pathlib, sys
src, dst, out, key = sys.argv[1:5]
home = pathlib.Path.home()
text = pathlib.Path(src).read_text()
text = text.replace("/home/me/.ssh/id_ed25519.pub", str(home / ".ssh/qnx_resfuzz_ed25519.pub"))
text = text.replace("/home/me/.ssh/id_ed25519", key)
text = text.replace('out_dir = "resfuzz-results"', f'out_dir = "{out}"')
# Keep fuzz.seconds override via run-existing --seconds; leave toml default.
pathlib.Path(dst).write_text(text)
PY

# Prefer an existing venv; otherwise create a throwaway one under the out dir.
if [[ -x $repo/.venv/bin/resfuzz ]]; then
  py=$repo/.venv/bin/python
elif [[ -x /tmp/venv/bin/resfuzz ]]; then
  py=/tmp/venv/bin/python
else
  python3 -m venv "$out/.venv"
  "$out/.venv/bin/pip" -q install -e "$repo/host"
  py=$out/.venv/bin/python
fi

echo "repro: campaign on $ip for ${seconds}s → $out"
"$py" "$repo/scripts/run-existing.py" -c "$run_cfg" --host "$ip" --identity "$key" --seconds "$seconds"
report=$out/report.md
[[ -f $report ]] || { echo "repro: missing $report" >&2; exit 1; }
"$py" "$repo/scripts/check-demo.py" "$report"
echo "repro: report $out/report.html"
