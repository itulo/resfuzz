#!/bin/bash
export HOME=/tmp/rfclean/home
cd /tmp/rfclean/resfuzz
source /home/box/qnx800/qnxsdp-env.sh >/dev/null
echo -e '\033[1;32m$\033[0m scripts/qnx-up.sh          # QNX SDP 8.0 · mkqnximage QEMU x86_64 · TCG (no KVM)'
s=$(date +%s); scripts/qnx-up.sh 2>&1 | sed "s#/tmp/rfclean/home#~#g"; echo "  (booted in $(( $(date +%s) - s )) s)"
sleep 3
