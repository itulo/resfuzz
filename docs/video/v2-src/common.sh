# shared settings for the v2 scene scripts (sourced)
R=/workspace/qnx-hackathon/resfuzz
IP=${QNX_IP:-172.31.1.156}
K="-i $HOME/.ssh/qnx_resfuzz_ed25519 -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR"
D=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
say(){ echo -e "\033[1;32m\$\033[0m $1"; sleep 0.6; }
