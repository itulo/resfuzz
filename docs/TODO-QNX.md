# QNX verification log (SDP 8.0)

Checked on 2026-11-05 with QNX SDP 8.0 (baseline 8.0.0.00141, qcc/gcc 12.2.0) and a
`mkqnximage --type=qemu --arch=x86_64` target under QEMU 10.0 **TCG** (no KVM, see I2).
Each item keeps the original question and, under **Result**, what was actually run.
`[x]` = verified on the target, `[~]` = partly verified, `[ ]` = still open.
Helper programs used: `target/tests/qnx/qnx_checks.c`, `target/tests/qnx/devctl_len.c`.

## A. Toolchain and build
- [x] **A1** `qcc -Vgcc_ntox86_64` (and `gcc_ntoaarch64le`) selects the target
  ([qcc](https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.utilities/topic/q/qcc.html)).
  Check with `qcc -V`. Used in `target/Makefile`.
  **Result:** `qcc -V` lists gcc 12.2.0 for gcc_ntox86_64 (default), gcc_ntoaarch64le and the _cxx/_gpp variants. Only x86_64 was built.
- [x] **A2** `make -C target qnx` builds with `-Wall` and no warnings. Watch for `getopt_long`
  (agent_main.c uses it; is it in QNX libc?), `pthread` linkage, `-lrt`,
  and `posix_spawn` ([posix_spawn](https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.lib_ref/topic/p/posix_spawn.html)).
  **Fixed:** `make -C target qnx` is clean with -Wall (and -Wextra). getopt_long, pthread and posix_spawn are in libc; no -lrt is needed. buggy_rm's io_read took `RESMGR_OCB_T *` (void by default, sys/resmgr.h) and dereferenced it; it now casts to `iofunc_ocb_t *`.
- [x] **A3** Header names: `<sys/iofunc.h>`, `<sys/dispatch.h>`, `<sys/neutrino.h>`,
  `<sys/syspage.h>`, `<devctl.h>`, and whether `<sys/iomsg.h>` is needed for `_IO_DEVCTL`.
  **Result:** all the headers exist. `_IO_DEVCTL` comes from `<sys/iomsg.h>`, which `<sys/iofunc.h>` already includes.

## C. Coverage on QNX
- [x] **C1** Does QNX's gcc accept `-fsanitize-coverage=trace-pc,trace-cmp`? If not:
  try `-finstrument-functions` (function-level coverage), or fall back to blind mode and say so.
  **Result:** accepted. objdump of buggy_rm shows ~80 `__sanitizer_cov_trace_pc` calls plus cmp/const_cmp/switch hooks.
- [x] **C2** `cov_rt.c` uses `__thread`, `shm_open(RESFUZZ_SHM)` + `mmap`, and `getenv` in a
  constructor. Check these work in a resource manager started by the agent with `posix_spawn`.
  **Result:** works. In the orchestrated campaigns the agent reported `coverage=1`, and the buggy_rm it spawned fed edges through the shm map (run1: 9,831 edges).
- [x] **C3** Is the shm name format `/resfuzz_cov_<pid>` accepted ([shm_open](https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.lib_ref/topic/s/shm_open.html))?
  **Result:** `shm_open("/resfuzz_cov_<pid>", O_CREAT|O_RDWR|O_EXCL)` + ftruncate + mmap succeed (qnx_checks C3).

## D. Message passing and the resmgr API
- [x] **D1** `struct _io_devctl` field order and sizes (type, combine_len, dcmd, nbytes, zero) and
  `struct _io_devctl_reply` ([iofunc_devctl](https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.lib_ref/topic/i/iofunc_devctl.html)).
  Check with a `static_assert(sizeof…)` and `offsetof` in a test program.
  **Result:** both structs are 16 bytes with the assumed offsets; the static_asserts in qnx_checks.c pass.
- [x] **D2** The `combine_len` value that libc's `devctl()` puts in the header. Capture a real
  `devctl()` in buggy_rm and dump `msg->i`; `platform_qnx.c` currently uses `sizeof(header)`.
  **Result:** libc devctl() sends combine_len = 16 (= sizeof header), srcmsglen = 16 + nbytes and offset 0, the same as platform_qnx.c. msg_max_size reads 2088 although 2048 was requested.
- [x] **D3** `devctl()` returns EOK or an errno directly, per the examples on the
  [devctl](https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.lib_ref/topic/d/devctl.html) page. Check how `dev_info_ptr` is filled.
  **Result:** it returns 0/EOK or the errno directly (an unknown dcmd gives 25, ENOTTY); `*dev_info_ptr` was 0.
- [x] **D4** Calling `MsgSend()` on an fd returned by `open()`: the [MsgSend](https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.lib_ref/topic/m/msgsend.html) page
  says open() establishes a usable coid. Confirm with DEVCTL_RAW against buggy_rm.
  **Result:** MsgSend on an open() fd works (GET_NAME: rc 0, ret_val 0, nbytes 32, data "buggy"). DEVCTL_RAW also triggers BUG1/BUG2 while fuzzing.
- [ ] **D5** `name_open()` / `name_close()` semantics when the target is a name instead of a path.
  **Open:** name_open() targets were not tested (buggy_rm only attaches a path).
- [x] **D6** In buggy_rm `io_devctl`: available payload = `srcmsglen − offset − sizeof(msg->i)`.
  This is borrowed from the io_write sample and unchecked for devctl. Also check whether messages larger
  than the receive buffer need `resmgr_msgget()` (the current code copies only what was received;
  `msg_max_size = 2048`).
  **Fixed:** avail = srcmsglen - offset - 16 is right, but srcmsglen is what the client *sent*. A 4096-byte SET_NAME arrives with msglen 2088 (= msg_max_size). io_devctl now fetches the payload with `resmgr_msgget()` instead of copying from the receive buffer (tests/qnx/devctl_len.c with BUGGY_TRACE=1).
- [x] **D7** `_RESMGR_PTR(ctp, &msg->o, sizeof(msg->o) + outlen)` as the reply for devctl, and setting
  `msg->o.ret_val` / `nbytes` as in the sample.
  **Result:** the _RESMGR_PTR reply works. GET_NAME returns ret_val 0, nbytes 32 and the data (D4, devctl_len.c).
- [x] **D8** `iofunc_devctl_default()` handling and its `_RESMGR_DEFAULT` return value.
  **Result:** iofunc_devctl_default() returns _RESMGR_DEFAULT for our dcmds, and unknown dcmds come back as ENOTTY.
- [~] **D9** `MSG_RAW` with arbitrary first u16 types: does the kernel or libc reject some before
  they reach the server? Record any that are rejected.
  **Partial:** types 0..0x13f (except _IO_CONNECT) all went through MsgSend with no unusual kernel errno. Replies were ENOSYS/EINVAL/EBADMSG/EBADF/EPERM, 0x106 gave ENOTTY and 0x10f gave errno 240. From the client side I can't tell whether the kernel or the server rejected them.

## E. Timing and oracles
- [x] **E1** `TimerTimeout()` and EINVAL: the Errors section says ns ≥ 1e9 is EINVAL, but the page's
  own example uses 10 s ([timertimeout](https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.lib_ref/topic/t/timertimeout.html)).
  We clamp to 999,999,999 ns. Test a 2 s timeout.
  **Fixed:** a 2 s and a 0.5 s timeout both return 0, and the blocked call fails with ETIMEDOUT after 2.001 s / 0.501 s. The 999,999,999 ns clamp in arm_timeout() was removed.
- [x] **E2** Watchdog: check that it actually unblocks a hung `MsgSend` when the server has
  `_NTO_CHF_UNBLOCK` set, and that SIGABRT on the spawned server produces a core.
  **Result:** works. BUG3b (endless loop) is reported as a HANG with ETIMEDOUT. The agent kills the server with SIGABRT, and dumper writes a core whose backtrace ends in `compute` (buggy_logic.c:77).
- [x] **E3** `ClockCycles()` and `SYSPAGE_ENTRY(qtime)->cycles_per_sec` under QEMU: is it stable
  across a run? Per the docs, the x86_64 frequency "may vary from boot to boot"
  ([clockcycles](https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.lib_ref/topic/c/clockcycles.html)).
  Choose `deadline_us` with margin over QEMU jitter, and check that BUG3a is clearly separated from noise.
  **Result:** cycles_per_sec was 2.397-2.400 GHz; it varies per boot but stays within +/-0.005% during a run. TCG jitter is large: ordinary replies take up to ~60 ms (one MSG_RAW reply took 2.8 s), and BUG3a takes 640-900 ms. Under TCG, deadline_us = 100000 separates them (host/examples/qemu.toml); use a few ms on KVM or hardware.
- [x] **E4** Signal numbers on QNX differ from Linux; `triage.SIGNALS` is the Linux table. Add a QNX table.
  **Fixed:** triage.py has SIGNALS_QNX (ILL 4, TRAP 5, ABRT 6, EMT 7, FPE 8, KILL 9, BUS 10, SEGV 11, SYS 12, PIPE 13, USR1 16, USR2 17). It picks the table from the agent's `platform=` start event and rates severity by signal name, since SIGBUS is 10 on QNX.

## I. Image, networking and transfer (mkqnximage)
- [x] **I1** `mkqnximage --type=qemu --arch=x86_64 --build`, then `--run=-h`, then `--getip`. Check the output format
  that `MkQnxImage.get_ip()` parses (it takes the last IPv4 in stdout)
  ([mkqnximage](https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.utilities/topic/m/mkqnximage.html)).
  **Fixed:** --build, --run=-h and --getip work, and getip prints just the IP (it changes per boot). `--run=-h` leaves QEMU holding stdout/stderr, so a captured run never returned; MkQnxImage.start() now logs to output/run.log. The default /data (61 MB) ran out of space after ~3000 corpus files, so images are built with `--data-size=512 --data-inodes=65536`.
- [x] **I2** The host needs QEMU plus bridge or networking setup (supported host: Ubuntu x86-64 or AArch64). Document the exact apt packages.
  **Result:** apt `qemu-system-x86 qemu-utils bridge-utils dnsmasq-base net-tools iputils-ping` (getip needs ping), plus `allow br0` in /etc/qemu/bridge.conf, a setuid qemu-bridge-helper, and `$QNX_HOST/../../common/mkqnximage/qemu/net.sh <helper> /etc/qemu/bridge.conf nat br0` for br0 + dnsmasq. On this box, creating a KVM vCPU hit a host kernel BUG (kvm/x86.c, vmx_vcpu_create), so I removed the box user's /dev/kvm access and QEMU runs TCG `--cpu max` (~270-380 execs/s). scripts/qnx-up.sh refuses to start if /dev/kvm is accessible.
- [x] **I3** `--ssh-ident=path`: does it expect a public or a private key? Is root ssh login with that key
  enough, or do we need password = username (root/root)?
  **Fixed:** --ssh-ident takes the PUBLIC key, and root login with the matching private key works. OpenSSH 10's sshd in the image needs /usr/libexec/sshd-auth, which the default image lacks. `qnx-image/local/snippets/ifs_files.custom` adds `/usr/libexec/sshd-auth=usr/libexec/sshd-auth`; the target path must be absolute, because a relative one lands in /proc/boot.
- [x] **I4** `scp -r` from the target works (sshd and the sftp subsystem); `/data` is writable.
  **Result:** scp to and from /data, and `scp -r` of the result dir, work.
- [ ] **I5** Add the agent and buggy_rm to the image instead of copying them with scp, using `--extra-dirs` or a
  `data_files` snippet (mkqnx6fsimg syntax). This is optional and makes reboots faster.
  **Open:** binaries are still copied with scp on every run (fast enough).
- [ ] **I6** Serial fallback: how to reach the QEMU console from the host. `output/qemu.out` is
  output only; check whether a socket or pty is available.
  **Open:** headless mode only writes the console to output/qemu.out. A `-serial unix:` socket via local/snippets/qemu_options.custom was not tried.

## T. Triage tools
- [x] **T1** `dumper -d DIR` ([dumper](https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.utilities/topic/d/dumper.html)):
  is a dumper already running on mkqnximage images, and what happens if a second one starts? What is the
  core file naming (we don't rely on it)?
  **Fixed:** the image already runs `dumper -U 30:30 -d /data/var/dumper -M 1000 -D cores`. A second `dumper -d DIR` exits silently and the first one keeps running. Cores are named `<progname>.core` (no pid) and overwritten. dump_dir now defaults to /data/var/dumper, and replay_for_core empties it before each replay.
- [x] **T2** The name of the host cross gdb in the SDP (`ntox86_64-gdb`?). Check that it reads QNX
  cores (`gdb program core`) and that `-batch -ex bt` prints frames in the format `triage._FRAME` parses.
  **Fixed:** `$QNX_HOST/usr/bin/ntox86_64-gdb -batch -ex bt buggy_rm core` reads QNX cores. gdb prints frame #0 once when it loads the core and again in bt, so parse_backtrace now restarts at each new #0. The default is now `target.gdb = "ntox86_64-gdb"`.
- [x] **T3** Whether `buggy_rm` needs `-g` and an unstripped binary on the host for symbols.
  **Result:** the qnx build uses -g and the host copy is unstripped. Backtraces show file:line for buggy_logic.c, buggy_rm.c and libc.

## R. Dictionary
- [x] **R1** Run the dictgen resolver on QNX against `buggy_rm.h` and replace the **stub** dcmd values
  (the Linux `linux_compat/devctl.h` encoding is our own) with the real ones in a `buggy_rm.qnx.dict`.
  **Result:** target/examples/buggy_rm.qnx.dict, resolved on the target: SET_MODE 0x80080540, SET_NAME 0x80200541, GET_NAME 0x40200542, GET_STATS 0xc0100543, COMPUTE 0xc00c0544, RESET 0x00000545.
- [x] **R2** Check `_DCMD_MISC` and the `__DIOT/__DIOF/__DIOTF/__DION` argument order against the real header.
  **Result:** _DCMD_MISC = 0x05, DEVDIR_TO = 0x80000000, DEVDIR_FROM = 0x40000000, size << 16, class << 8. The macros take (class, cmd, data), as assumed.

## F. Found while fuzzing on QNX
- [x] **F1** read() on an O_WRONLY fd and write() on an O_RDONLY fd return EBADF from iofunc. The agent treated
  EBADF as "server gone" and reported these as signal-0 crashes. **Fixed:** exec.c tracks each slot's open mode and treats
  that EBADF as an ordinary error. Linux `make test` still passes.
- [x] **F2** /data survives reboots, and the agent appends to events.jsonl, so a second campaign's results mixed
  with the first's. **Fixed:** QemuCampaign.deploy() recreates the remote out/ before each run.
- [ ] **F3** BUG1 crashes depend on server state (admin mode set by an earlier test case on the same server
  instance). Replay against a fresh server often doesn't reproduce them, so they have no core/minimised case.
  A "replay the last N cases" mode would help.
