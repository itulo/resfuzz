# What to verify once the QNX SDP 8.0 is installed

Nothing in `target/platform/platform_qnx.c`, `target/buggy_rm/buggy_rm.c`, the
`qnx` Makefile target or `QemuCampaign` has been compiled or run yet. Each item
below gives the assumption, where it is used, and how to check it. Tick items off and
remove their UNVERIFIED markers from the code as you go.

## A. Toolchain and build
- [ ] **A1** `qcc -Vgcc_ntox86_64` (and `gcc_ntoaarch64le`) selects the target
  ([qcc](https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.utilities/topic/q/qcc.html)).
  Check with `qcc -V`. Used in `target/Makefile`.
- [ ] **A2** `make -C target qnx` builds with `-Wall` and no warnings. Watch for `getopt_long`
  (agent_main.c uses it; is it in QNX libc?), `pthread` linkage, `-lrt`,
  and `posix_spawn` ([posix_spawn](https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.lib_ref/topic/p/posix_spawn.html)).
- [ ] **A3** Header names: `<sys/iofunc.h>`, `<sys/dispatch.h>`, `<sys/neutrino.h>`,
  `<sys/syspage.h>`, `<devctl.h>`, and whether `<sys/iomsg.h>` is needed for `_IO_DEVCTL`.

## C. Coverage on QNX
- [ ] **C1** Does QNX's gcc accept `-fsanitize-coverage=trace-pc,trace-cmp`? If not:
  try `-finstrument-functions` (function-level coverage), or fall back to blind mode and say so.
- [ ] **C2** `cov_rt.c` uses `__thread`, `shm_open(RESFUZZ_SHM)` + `mmap`, and `getenv` in a
  constructor. Check these work in a resource manager started by the agent with `posix_spawn`.
- [ ] **C3** Is the shm name format `/resfuzz_cov_<pid>` accepted ([shm_open](https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.lib_ref/topic/s/shm_open.html))?

## D. Message passing and the resmgr API
- [ ] **D1** `struct _io_devctl` field order and sizes (type, combine_len, dcmd, nbytes, zero) and
  `struct _io_devctl_reply` ([iofunc_devctl](https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.lib_ref/topic/i/iofunc_devctl.html)).
  Check with a `static_assert(sizeof…)` and `offsetof` in a test program.
- [ ] **D2** The `combine_len` value that libc's `devctl()` puts in the header. Capture a real
  `devctl()` in buggy_rm and dump `msg->i`; `platform_qnx.c` currently uses `sizeof(header)`.
- [ ] **D3** `devctl()` returns EOK or an errno directly, per the examples on the
  [devctl](https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.lib_ref/topic/d/devctl.html) page. Check how `dev_info_ptr` is filled.
- [ ] **D4** Calling `MsgSend()` on an fd returned by `open()`: the [MsgSend](https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.lib_ref/topic/m/msgsend.html) page
  says open() establishes a usable coid. Confirm with DEVCTL_RAW against buggy_rm.
- [ ] **D5** `name_open()` / `name_close()` semantics when the target is a name instead of a path.
- [ ] **D6** In buggy_rm `io_devctl`: available payload = `srcmsglen − offset − sizeof(msg->i)`.
  This is borrowed from the io_write sample and unchecked for devctl. Also check whether messages larger
  than the receive buffer need `resmgr_msgget()` (the current code copies only what was received;
  `msg_max_size = 2048`).
- [ ] **D7** `_RESMGR_PTR(ctp, &msg->o, sizeof(msg->o) + outlen)` as the reply for devctl, and setting
  `msg->o.ret_val` / `nbytes` as in the sample.
- [ ] **D8** `iofunc_devctl_default()` handling and its `_RESMGR_DEFAULT` return value.
- [ ] **D9** `MSG_RAW` with arbitrary first u16 types: does the kernel or libc reject some before
  they reach the server? Record any that are rejected.

## E. Timing and oracles
- [ ] **E1** `TimerTimeout()` and EINVAL: the Errors section says ns ≥ 1e9 is EINVAL, but the page's
  own example uses 10 s ([timertimeout](https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.lib_ref/topic/t/timertimeout.html)).
  We clamp to 999,999,999 ns. Test a 2 s timeout.
- [ ] **E2** Watchdog: check that it actually unblocks a hung `MsgSend` when the server has
  `_NTO_CHF_UNBLOCK` set, and that SIGABRT on the spawned server produces a core.
- [ ] **E3** `ClockCycles()` and `SYSPAGE_ENTRY(qtime)->cycles_per_sec` under QEMU: is it stable
  across a run? Per the docs, the x86_64 frequency "may vary from boot to boot"
  ([clockcycles](https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.lib_ref/topic/c/clockcycles.html)).
  Choose `deadline_us` with margin over QEMU jitter, and check that BUG3a is clearly separated from noise.
- [ ] **E4** Signal numbers on QNX differ from Linux; `triage.SIGNALS` is the Linux table. Add a QNX table.

## I. Image, networking and transfer (mkqnximage)
- [ ] **I1** `mkqnximage --type=qemu --arch=x86_64 --build`, then `--run=-h`, then `--getip`. Check the output format
  that `MkQnxImage.get_ip()` parses (it takes the last IPv4 in stdout)
  ([mkqnximage](https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.utilities/topic/m/mkqnximage.html)).
- [ ] **I2** The host needs QEMU plus bridge or networking setup (supported host: Ubuntu x86-64 or AArch64). Document the exact apt packages.
- [ ] **I3** `--ssh-ident=path`: does it expect a public or a private key? Is root ssh login with that key
  enough, or do we need password = username (root/root)?
- [ ] **I4** `scp -r` from the target works (sshd and the sftp subsystem); `/data` is writable.
- [ ] **I5** Add the agent and buggy_rm to the image instead of copying them with scp, using `--extra-dirs` or a
  `data_files` snippet (mkqnx6fsimg syntax). This is optional and makes reboots faster.
- [ ] **I6** Serial fallback: how to reach the QEMU console from the host. `output/qemu.out` is
  output only; check whether a socket or pty is available.

## T. Triage tools
- [ ] **T1** `dumper -d DIR` ([dumper](https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.utilities/topic/d/dumper.html)):
  is a dumper already running on mkqnximage images, and what happens if a second one starts? What is the
  core file naming (we don't rely on it)?
- [ ] **T2** The name of the host cross gdb in the SDP (`ntox86_64-gdb`?). Check that it reads QNX
  cores (`gdb program core`) and that `-batch -ex bt` prints frames in the format `triage._FRAME` parses.
- [ ] **T3** Whether `buggy_rm` needs `-g` and an unstripped binary on the host for symbols.

## R. Dictionary
- [ ] **R1** Run the dictgen resolver on QNX against `buggy_rm.h` and replace the **stub** dcmd values
  (the Linux `linux_compat/devctl.h` encoding is our own) with the real ones in a `buggy_rm.qnx.dict`.
- [ ] **R2** Check `_DCMD_MISC` and the `__DIOT/__DIOF/__DIOTF/__DION` argument order against the real header.
