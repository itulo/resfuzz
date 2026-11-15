# Codelab: Fuzz-test a QNX 8.0 resource manager on QEMU from a Linux host

**Audience:** developers writing or reviewing QNX resource managers.
**Time:** about 60 minutes. Steps 1–3 need no QNX; steps 4–8 need SDP 8.0.
**Bonus:** this is the tutorial submitted for the QNX Global Hackathon *Best Codelab/Tutorial* prize.

You will learn how a resource manager receives `devctl` messages, why length and
state handling is a classic bug source, and how to find crashes, hangs and
real-time deadline misses automatically with coverage-guided fuzzing.

## What you'll need

| | Without QNX (steps 1–3) | With QNX (steps 4–8) |
|---|---|---|
| Host | Ubuntu 22.04 or 24.04 x86-64 | same |
| Packages | `build-essential`, `python3`, `python3-venv`, `make` | plus `qemu-system-x86`, `qemu-utils`, `bridge-utils`, `dnsmasq-base`, `iputils-ping` |
| Disk | ~500 MB | ~4 GB for the SDP (already installed) + ~1 GB for the image |
| Accounts | none | a free [myQNX](https://www.qnx.com/) account and a non-commercial SDP 8.0 licence |

**TCG only on this box.** The host kernel panics if QEMU uses KVM here, so
`scripts/qnx-up.sh` refuses to start when `/dev/kvm` is accessible. On a normal
desktop where KVM works you can drop that check; under TCG expect ~200–300
executions per second instead of thousands.

---

## Step 1 — Overview (2 min)

A QNX *resource manager* is a user-space server. Clients reach it with the
ordinary POSIX calls `open`, `read`, `write`, `devctl`, …; the kernel turns
each call into a message (`MsgSend`) and the server replies. Bugs live in how
the server trusts the message: a length field it never checks, a table index it
never validates, work that never finishes on its only thread.

`resfuzz` drives that interface the way a malicious (or just buggy) client would,
and reports three kinds of finding:

| Oracle | What it means | How it is detected on QNX |
|---|---|---|
| crash | the server died | `MsgSend` fails with ESRCH/EBADF *and* the spawned process is gone; `dumper` writes a core |
| hang | no reply within the hang limit | `TimerTimeout` on send/reply, plus a watchdog that aborts the server |
| deadline miss | a reply that took longer than the budget | `ClockCycles()` delta / `SYSPAGE_ENTRY(qtime)->cycles_per_sec` |

Architecture (host orchestrates, agent fuzzes on the target):

```
 Linux host                                   QNX 8.0 guest (QEMU, mkqnximage)
 ───────────────────────────────              ────────────────────────────────
 resfuzz CLI                                  /data/resfuzz/
  ├─ campaign, triage, report                   resfuzz-agent ──MsgSend──▶ resource manager
  └─ dictgen (header → resolver → dictionary)   out/ events.jsonl crashes/ hangs/ deadlines/
```

The demo target is `buggy_rm`, a small resource manager with three planted bugs.
You will find them yourself in steps 2–7; don't peek at `buggy_logic.c` yet.

---

## Step 2 — Run it locally against the mock (10 min, no QNX)

```sh
git clone <this-repo> resfuzz && cd resfuzz
make -C target test linux          # builds the agent + mock_rm, runs ~480k unit checks
python3 -m venv .venv && . .venv/bin/activate
pip install -e host
resfuzz local --seconds 30 --no-cov --out /tmp/rf-blind
```

**Expected:** a Markdown and HTML report under `/tmp/rf-blind/`. Open the HTML:

```sh
xdg-open /tmp/rf-blind/report.html   # or: python3 -m http.server --directory /tmp/rf-blind
```

Pick one high-severity finding. Note:

- the *bucket* (stable hash of the call site),
- whether it is marked *reproducible*,
- the minimised test case (a few ops),
- the reproduce command.

Replay it by hand:

```sh
./target/build/resfuzz-agent \
  --target /tmp/rf.sock \
  --spawn "./target/build/mock_rm /tmp/rf.sock" \
  --replay /tmp/rf-blind/min/<the-file>.min
echo $?    # 10 = crash, 11 = hang, 12 = deadline
./target/build/resfuzz-agent --decode /tmp/rf-blind/min/<the-file>.min
```

**Troubleshooting**

| Symptom | Fix |
|---|---|
| `make: qcc: not found` in this step | you don't need qcc yet; `make -C target linux` is enough |
| no findings in 30 s | rerun with `--seed 1`; a blind run is noisy |
| `Address already in use` on the socket | `rm -f /tmp/rf.sock` |

---

## Step 3 — Turn on coverage (8 min, no QNX)

```sh
resfuzz local --seconds 60 --out /tmp/rf-cov
```

`mock_rm` is already built with `-fsanitize-coverage=trace-pc,trace-cmp`. Compare
`/tmp/rf-cov/stats.json` (`edges`) with the blind run. Coverage also feeds a
compare-log path that recovers interesting constants (dcmd values, the `"ADMN"`
magic) even without a dictionary.

**Concept box — stateful findings.** BUG1 only fires after the server is put into
admin mode. A single test case that does `SET_NAME` alone is fine; the same
`SET_NAME` after a `SET_MODE` crashes. The agent keeps a ring of recent cases
and, when a finding does not reproduce alone, saves the history as an RFS1
*sequence*. Minimisation then shrinks the history and flattens it into one short
test case when it can. Every finding is replayed against a freshly restarted
server so "reproducible" means what it says.

---

## Step 4 — Set up the QNX side (12 min)

1. Install QNX SDP 8.0, then:

```sh
source ~/qnx800/qnxsdp-env.sh
qcc -V                      # expect: gcc_nto* variants including gcc_ntox86_64
```

2. Cross-build the agent and the demo resource manager:

```sh
make -C target qnx          # → target/build-qnx/{resfuzz-agent,buggy_rm}
```

3. Boot the mkqnximage QEMU target (TCG). The helper installs host packages,
sets up a bridge (`br0`), bakes the binaries into the image, boots headless and
waits for ssh:

```sh
scripts/qnx-up.sh           # refuses to start if /dev/kvm is accessible (TCG only)
# … ends with: target up at 172.31.1.N
ssh -i ~/.ssh/qnx_resfuzz_ed25519 root@172.31.1.N 'ls /data/resfuzz'
```

**Expected:** `resfuzz-agent`, `buggy_rm`, `buggy_rm.qnx.dict` under `/data/resfuzz`.

Optional console on COM2:

```sh
scripts/qnx-up.sh --serial  # Ctrl-] to quit the client
```

Stop when you are done: `scripts/qnx-up.sh --stop`.

**Troubleshooting**

| Symptom | Fix |
|---|---|
| `qnx-up: refusing to start: /dev/kvm is accessible` | intentional on this host; remove yourself from the kvm group, or edit the check if you know KVM is safe |
| `getip` hangs / no IP | `arp` must be on `PATH` (`/usr/sbin`); `ping` needs `cap_net_raw`; the script sets both |
| `/data` missing after a rebuild | expected: rebuilding the image recreates `/data`; the snippets bake the binaries back in |
| ssh permission denied | use the *private* key with `ssh -i`; mkqnximage took the *public* key via `--ssh-ident` |

---

## Step 5 — Generate a dictionary from the header (8 min)

dcmd values are macros that expand differently per architecture. Never invent
them on the host: compile a tiny resolver with `qcc` and run it on the target.

```sh
resfuzz dictgen target/buggy_rm/buggy_rm.h \
  --resolver-out /tmp/resolver.c --out /tmp/partial.dict

# for a system header that needs prerequisites (e.g. <sys/dcmd_chr.h>):
#   --header-include '<sys/dcmd_chr.h>' --include termios.h --include sys/ioctl.h

source ~/qnx800/qnxsdp-env.sh
qcc -Vgcc_ntox86_64 -o /tmp/dcmd_resolver /tmp/resolver.c
scp -i ~/.ssh/qnx_resfuzz_ed25519 /tmp/dcmd_resolver root@172.31.1.N:/data/
ssh -i ~/.ssh/qnx_resfuzz_ed25519 root@172.31.1.N /data/dcmd_resolver > /tmp/resolved.txt

resfuzz dictgen target/buggy_rm/buggy_rm.h \
  --resolved /tmp/resolved.txt --out target/examples/buggy_rm.qnx.dict
```

**Expected:** lines like `dcmd DCMD_BUGGY_SET_MODE 0x80080540 size=8 dir=to`, plus
token lines for magics (`buggy_magic_admin="ADMN"`).

Optional AI step (OpenAI-compatible endpoint). The model proposes *payloads*;
dcmd values still come only from `--resolved`:

```sh
export RESFUZZ_LLM_BASE_URL=http://127.0.0.1:8080/v1
export RESFUZZ_LLM_MODEL=local
# RESFUZZ_LLM_API_KEY=…   # if the endpoint needs one
resfuzz dictgen target/buggy_rm/buggy_rm.h \
  --resolved /tmp/resolved.txt --ai --seeds-out docs/results/ai-seeds
```

Validated seeds land as `ai_seed_NNN.rfz`. See [docs/results/ai-seeds/](../results/ai-seeds/) for a recorded run.

---

## Step 6 — Fuzz on QNX (10 min)

```sh
# edit host/examples/qemu.toml so ssh.identity and image.ssh_ident point at your key
resfuzz run -c host/examples/qemu.toml --no-build
```

While it runs you can watch live:

```sh
ssh -i ~/.ssh/qnx_resfuzz_ed25519 root@172.31.1.N \
  'tail -f /data/resfuzz/out/events.jsonl'
```

**Expected (600 s, coverage on, TCG):** on the order of 10⁵ executions, ~100 edges,
and the three planted bugs. A committed reference run is
[docs/results/qnx-run5/report.md](../results/qnx-run5/report.md).

| Bug | Typical first-seen | Oracle |
|---|---|---|
| BUG3a (slow COMPUTE) | ~8 s | deadline |
| BUG2 (GET_STATS NULL) | ~13 s | crash |
| BUG1 (admin SET_NAME overflow) | ~3 min | crash, *stateful* |
| BUG3b (COMPUTE hang) | ~3–4 min | hang |

---

## Step 7 — Triage (8 min)

Open `resfuzz-results/.../report.html`. For each high finding you get:

- a minimised reproducer (`.rfz.min` or `.seq.min`),
- a `dumper` core and a gdb backtrace (frames in your code and in QNX `libc` resmgr),
- a one-line reproduce command.

Replay BUG1's sequence on the target (the one that needs earlier messages):

```sh
scp -i ~/.ssh/qnx_resfuzz_ed25519 \
  docs/results/qnx-run5/min/crash_*SET_NAME*.seq.min \
  root@172.31.1.N:/data/resfuzz/
ssh -i ~/.ssh/qnx_resfuzz_ed25519 root@172.31.1.N \
  '/data/resfuzz/resfuzz-agent --target /dev/buggy \
     --spawn "/data/resfuzz/buggy_rm /dev/buggy" \
     --replay /data/resfuzz/crash_38d435f06c3e3629_2.seq.min; echo exit:$?'
# expect exit:10 three times in a row
```

Decode it on the host:

```sh
./target/build/resfuzz-agent --decode docs/results/qnx-run5/min/crash_38d435f06c3e3629_2.seq.min
# → 2 ops: SET_MODE with "ADMN"/level 2, then SET_NAME claiming 0x20 bytes with 34 bytes of payload
```

Now peek at the fixes in `target/buggy_rm/buggy_logic.c`:

1. **BUG1** — copy `min(claimed, avail, sizeof name - 1)`, not `claimed` alone.
2. **BUG2** — reject indices whose table slot is NULL.
3. **BUG3** — reject or cap `n` *before* `n + 1`, and never take the slow path unboundedly on the server thread.

Rebuild, redeploy (`scripts/qnx-up.sh` rebuilds when the binary stamp changes) and
rerun: the planted bugs should be gone.

---

## Step 8 — Fuzz your own resource manager (2 min)

Checklist:

| Item | Where |
|---|---|
| Start command | `target.spawn` in the campaign toml (`--spawn` for the agent) |
| Path or name | `/dev/…` or `name:…` (`target.path`) |
| Header for dictgen | yours, or an SDP header like `<sys/dcmd_chr.h>` |
| Deadline budget | a few ms on hardware/KVM; ~100 ms under TCG |
| Coverage | rebuild *your* binary with `-fsanitize-coverage=trace-pc,trace-cmp` and link `target/covrt/cov_rt.c`; skip for closed SDP binaries |
| Character devices (pty, serial) | add `agent_args = ["--nonblock"]` so empty reads are EAGAIN, not hangs |

A worked example against the SDP's own `devc-pty` (no planted bugs, reply-signature
feedback only) is [host/examples/qemu-devc-pty.toml](../../host/examples/qemu-devc-pty.toml)
and [docs/results/qnx-pty1](../results/qnx-pty1/).

---

## Where to go next

- Wire `resfuzz local` into CI (Linux mock, no SDP needed for every commit).
- Read [docs/DESIGN.md](../DESIGN.md) for the oracles and the RFS1 sequence format.
- Read [docs/TODO-QNX.md](../TODO-QNX.md) for every QNX assumption that was checked on SDP 8.0.
- Submission status for the hackathon: [docs/SUBMISSION-CHECKLIST.md](../SUBMISSION-CHECKLIST.md).

## Licence and trademarks

This codelab is original work, Apache-2.0. "QNX" is a trademark of BlackBerry
Limited; this tutorial is an independent project and is not affiliated with or
endorsed by BlackBerry. Screenshots in `docs/img/` contain only resfuzz UI and
terminal output — no third-party logos or copyrighted music.
