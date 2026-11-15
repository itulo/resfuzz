#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Boot the resfuzz QNX 8.0 QEMU target (headless, TCG) and wait until ssh works.
#   scripts/qnx-up.sh            boot (rebuilds the image when the snippets or binaries changed)
#   scripts/qnx-up.sh --stop     stop it
#   scripts/qnx-up.sh --serial   attach to the second serial console (root shell; Ctrl-] to quit)
# Env: QNX_SDP (default ~/qnx800), QNX_IMAGE_DIR (default <repo>/qnx-image),
#      QNX_SSH_KEY (default ~/.ssh/qnx_resfuzz_ed25519; the .pub is baked into the image).
set -euo pipefail
repo=$(cd "$(dirname "$0")/.." && pwd)
sdp=${QNX_SDP:-$HOME/qnx800}
img=${QNX_IMAGE_DIR:-$repo/qnx-image}
key=${QNX_SSH_KEY:-$HOME/.ssh/qnx_resfuzz_ed25519}
# shellcheck disable=SC1091
source "$sdp/qnxsdp-env.sh" >/dev/null
export PATH=$PATH:/usr/sbin:/sbin
mkdir -p "$img" && cd "$img"

case ${1:-} in
--stop) mkqnximage --stop </dev/null; exit ;;
--serial)
    exec python3 - "$img/output/ser2.sock" <<'PY'
import os, select, socket, sys, termios, tty
s = socket.socket(socket.AF_UNIX); s.connect(sys.argv[1]); s.send(b"\n")
fd = sys.stdin.fileno(); old = termios.tcgetattr(fd); tty.setraw(fd)
try:
    while True:
        r, _, _ = select.select([s, fd], [], [])
        if s in r:
            d = s.recv(4096)
            if not d: break
            os.write(1, d)
        if fd in r:
            d = os.read(fd, 1024)
            if b"\x1d" in d: break
            s.send(d)
finally:
    termios.tcsetattr(fd, termios.TCSADRAIN, old)
PY
    ;;
esac

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
for c in qemu-system-x86_64:qemu-system-x86 qemu-img:qemu-utils brctl:bridge-utils dnsmasq:dnsmasq-base ping:iputils-ping arp:net-tools; do
    command -v "${c%%:*}" >/dev/null 2>&1 || need+=("${c#*:}")
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

# Bridge + DHCP for mkqnximage's QEMU networking (same call runimage makes).
if ! ip link show br0 >/dev/null 2>&1 || ! pgrep -f 'dnsmasq --interface=br0' >/dev/null; then
    sudo bash "$QNX_HOST/../../common/mkqnximage/qemu/net.sh" "$helper" /etc/qemu/bridge.conf nat br0
fi

# Image customisation lives in the repo (scripts/qnx-image-snippets): sshd-auth,
# the resfuzz binaries baked into /data, and a second serial console.
build=$repo/target/build-qnx
for b in resfuzz-agent buggy_rm buggy_rm_blind; do
    [[ -x $build/$b ]] || make -C "$repo/target" qnx >/dev/null
done
snip=$repo/scripts/qnx-image-snippets
mkdir -p local/snippets output
cp "$snip"/ifs_files.custom "$snip"/post_start.custom "$snip"/qemu_options.custom local/snippets/
sed -e "s#@BUILD@#$build#g" -e "s#@REPO@#$repo#g" "$snip/data_files.custom.in" > local/snippets/data_files.custom
stamp=$(cat local/snippets/*.custom "$build"/resfuzz-agent "$build"/buggy_rm | sha1sum | cut -c1-40)

# Same options as the build, or mkqnximage rebuilds the image.
opts=(--noprompt --type=qemu --arch=x86_64 "--ssh-ident=$key.pub" --data-size=512 --data-inodes=65536)
if [[ ! -f output/ifs.bin || $(cat output/.resfuzz-stamp 2>/dev/null) != "$stamp" ]]; then
    [[ -f output/qemu.pid ]] && mkqnximage --stop </dev/null >/dev/null 2>&1 || true
    echo "qnx-up: (re)building the image (snippets or binaries changed); /data is recreated"
    mkqnximage "${opts[@]}" --build </dev/null >output/build.log 2>&1
    echo "$stamp" > output/.resfuzz-stamp
fi
# QEMU inherits stdout/stderr: never pipe this command.
mkqnximage "${opts[@]}" --run=-h </dev/null >output/run.log 2>&1
ip=$(mkqnximage --getip </dev/null | grep -Eo '([0-9]{1,3}\.){3}[0-9]{1,3}' | head -1)
[[ -n $ip ]] || { echo "qnx-up: no IP from mkqnximage --getip" >&2; exit 1; }
ssh_opts=(-i "$key" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR -o ConnectTimeout=5)
for _ in $(seq 60); do
    if ssh "${ssh_opts[@]}" "root@$ip" uname -a 2>/dev/null; then
        echo "qnx-up: target up at $ip  (ssh ${ssh_opts[*]:0:2} root@$ip); serial: scripts/qnx-up.sh --serial"
        exit 0
    fi
    sleep 5
done
echo "qnx-up: $ip did not answer ssh within 5 min; console log: $img/output/qemu.out" >&2
exit 1
