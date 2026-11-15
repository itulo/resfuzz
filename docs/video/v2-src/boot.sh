#!/bin/bash
# boot the QNX 8.0 QEMU target (TCG); output shortened to fit 72 columns
. "$(dirname "$0")/common.sh"; cd $R
source $HOME/qnx800/qnxsdp-env.sh >/dev/null
say "scripts/qnx-up.sh       # SDP 8.0 · mkqnximage · QEMU TCG, no KVM"
s=$(date +%s)
scripts/qnx-up.sh 2>&1 | sed -E "s#$HOME#~#g; s/ +\(ssh -i [^)]*\)//; s/; serial: .*//" | cut -c1-72
echo "  (booted in $(( $(date +%s) - s )) s)"
sleep 1
