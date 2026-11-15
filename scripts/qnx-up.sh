#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Boot the resfuzz QNX 8.0 QEMU target (headless) and wait until ssh works.
#   scripts/qnx-up.sh          boot (rebuilds the image only if its options changed)
#   scripts/qnx-up.sh --stop   stop it
# Env: QNX_SDP (default ~/qnx800), QNX_IMAGE_DIR (default <repo>/qnx-image),
#      QNX_SSH_KEY (default ~/.ssh/qnx_resfuzz_ed25519; the .pub is baked into the image).
set -euo pipefail
repo=$(cd "$(dirname "$0")/.." && pwd)
sdp=${QNX_SDP:-$HOME/qnx800}
img=${QNX_IMAGE_DIR:-$repo/qnx-image}
key=${QNX_SSH_KEY:-$HOME/.ssh/qnx_resfuzz_ed25519}
# shellcheck disable=SC1091
source "$sdp/qnxsdp-env.sh" >/dev/null
mkdir -p "$img" && cd "$img"

if [[ ${1:-} == --stop ]]; then mkqnximage --stop </dev/null; exit; fi

# KVM is deliberately NOT used: on this host, creating a KVM vCPU hit a kernel
# BUG (kvm/x86.c, vmx_vcpu_create, nested virtualisation). runimage only uses
# KVM when /dev/kvm is accessible, so refuse to start if it is.
if [[ -r /dev/kvm && -w /dev/kvm ]]; then
    echo "qnx-up: /dev/kvm is accessible; this host crashes under KVM. Remove access first:" >&2
    echo "        sudo setfacl -x u:$(id -un) /dev/kvm   (or run as a user outside the kvm group)" >&2
    exit 1
fi

# Host packages (Debian/Ubuntu names) and the setuid bridge helper QEMU uses.
need=()
for c in qemu-system-x86_64:qemu-system-x86 qemu-img:qemu-utils brctl:bridge-utils dnsmasq:dnsmasq-base ping:iputils-ping; do
    command -v "${c%%:*}" >/dev/null 2>&1 || [[ -x /usr/sbin/${c%%:*} ]] || need+=("${c#*:}")
done
if ((${#need[@]})); then
    sudo DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends "${need[@]}"
fi
helper=/usr/lib/qemu/qemu-bridge-helper
[[ -u $helper ]] || sudo chmod u+s "$helper"
# mkqnximage --getip pings the arp candidates: ping needs CAP_NET_RAW.
if ! ping -nc 1 -W 1 127.0.0.1 >/dev/null 2>&1; then
    sudo setcap cap_net_raw+p "$(command -v ping)" 2>/dev/null || sudo chmod u+s "$(command -v ping)"
fi
export PATH=$PATH:/usr/sbin:/sbin

# Bridge + DHCP for mkqnximage's QEMU networking (same call runimage makes).
if ! ip link show br0 >/dev/null 2>&1 || ! pgrep -f 'dnsmasq --interface=br0' >/dev/null; then
    sudo bash "$QNX_HOST/../../common/mkqnximage/qemu/net.sh" \
        /usr/lib/qemu/qemu-bridge-helper /etc/qemu/bridge.conf nat br0
fi

# Same options as the build, or mkqnximage rebuilds the image.
opts=(--noprompt --type=qemu --arch=x86_64 "--ssh-ident=$key.pub" --data-size=512 --data-inodes=65536)
[[ -f output/disk-qemu.vmdk || -d output ]] || mkqnximage "${opts[@]}" --build </dev/null
# QEMU inherits stdout/stderr: never pipe this command.
mkqnximage "${opts[@]}" --run=-h </dev/null >output/run.log 2>&1
ip=$(mkqnximage --getip </dev/null | grep -Eo '([0-9]{1,3}\.){3}[0-9]{1,3}' | head -1)
[[ -n $ip ]] || { echo "qnx-up: no IP from mkqnximage --getip" >&2; exit 1; }
ssh_opts=(-i "$key" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR -o ConnectTimeout=5)
for _ in $(seq 60); do
    if ssh "${ssh_opts[@]}" "root@$ip" uname -a 2>/dev/null; then
        echo "qnx-up: target up at $ip  (ssh ${ssh_opts[*]:0:2} root@$ip)"
        exit 0
    fi
    sleep 5
done
echo "qnx-up: $ip did not answer ssh within 5 min; console log: $img/output/qemu.out" >&2
exit 1
