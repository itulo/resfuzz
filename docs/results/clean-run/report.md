# resfuzz report: `/dev/buggy`

Generated 2026-11-15 12:09 EET on platform `qnx`.

| Metric | Value |
|---|---|
| Executions | 241138 (401.9/s over 600.0 s) |
| Coverage | on, 114 edges |
| Reply signatures | 8082 |
| Deadline | 100000 us |
| Max latency seen | 16350744 us |
| Target restarts | 372 |
| Unique findings | 4 crash, 1 hang, 3 deadline (7 reproducible) |

## Findings

| # | Severity | Finding | Hits | Reproducible |
|---|---|---|---|---|
| 1 | high | CRASH in DEVCTL DCMD_BUGGY_SET_NAME (SIGSEGV) at buggy_devctl | 3 | yes, stateful (3-case sequence) |
| 2 | high | CRASH in DEVCTL DCMD_BUGGY_GET_STATS (SIGSEGV) at buggy_devctl | 3 | yes |
| 3 | high | CRASH in DEVCTL_RAW DCMD_BUGGY_SET_NAME (SIGSEGV) at buggy_devctl | 3 | yes, stateful (73-case sequence) |
| 4 | high | CRASH in DEVCTL_RAW DCMD_BUGGY_GET_STATS (SIGSEGV) at buggy_devctl | 3 | yes |
| 5 | medium | HANG in DEVCTL_RAW DCMD_BUGGY_COMPUTE | 2 | yes |
| 6 | medium | DEADLINE in DEVCTL DCMD_BUGGY_COMPUTE (1108758 us) | 3 | yes |
| 7 | medium | DEADLINE in DEVCTL_RAW DCMD_BUGGY_COMPUTE (1019744 us) | 3 | yes |
| 8 | low | DEADLINE in DEVCTL_RAW DCMD_BUGGY_GET_STATS (111436 us) | 1 | no (stateful/flaky) |

### 1. CRASH in DEVCTL DCMD_BUGGY_SET_NAME (SIGSEGV) at buggy_devctl

- Bucket `38d435f06c3e3629`, first seen at 160.3 s, 3 hit(s)
- Errno: 3 No such process
- Signal: SIGSEGV
- Reproduce: `resfuzz-agent --target /dev/buggy --spawn "/data/resfuzz/buggy_rm /dev/buggy" --replay crashes/crash_38d435f06c3e3629_2.seq`
- Stateful: needs earlier messages; reproduces deterministically when the recorded 3-case history is replayed against a fresh server

Minimised reproducer (2 ops):

```
#0   slot0 DEVCTL_RAW a=0x80080540 b=0x00000008 len=8 data=41444d4e02000000
#1   slot0 DEVCTL     a=0x80200541 b=0x00000020 len=33 data=0000000000000000000000000000000000000000000000000000000000000000...
```

Backtrace:

```
#0 buggy_devctl at buggy_rm/buggy_logic.c:112
#1 io_devctl at buggy_rm/buggy_rm.c:108
#2 io_devctl at buggy_rm/buggy_rm.c:75
#3 _resmgr_invoke_handler at /builds/workspace/QNXOS_805/build_x86_64/lib/c/resmgr/_resmgr_io_handler.c:68
#4 _resmgr_io_handler at /builds/workspace/QNXOS_805/build_x86_64/lib/c/resmgr/_resmgr_io_handler.c:308
#5 _resmgr_handler at /builds/workspace/QNXOS_805/build_x86_64/lib/c/resmgr/_resmgr_handler.c:198
#6 _resmgr_msg_handler at /builds/workspace/QNXOS_805/build_x86_64/lib/c/dispatch/resmgr.c:311
#7 main at buggy_rm/buggy_rm.c:154
```

### 2. CRASH in DEVCTL DCMD_BUGGY_GET_STATS (SIGSEGV) at buggy_devctl

- Bucket `3cf05774be8055ff`, first seen at 11.0 s, 3 hit(s)
- Errno: 3 No such process
- Signal: SIGSEGV
- Reproduce: `resfuzz-agent --target /dev/buggy --spawn "/data/resfuzz/buggy_rm /dev/buggy" --replay crashes/crash_3cf05774be8055ff_0.rfz`

Minimised reproducer (1 ops):

```
#0   slot0 DEVCTL     a=0xc0100543 b=0x00000010 len=1 data=06
```

Backtrace:

```
#0 buggy_devctl at buggy_rm/buggy_logic.c:124
#1 io_devctl at buggy_rm/buggy_rm.c:108
#2 io_devctl at buggy_rm/buggy_rm.c:75
#3 _resmgr_invoke_handler at /builds/workspace/QNXOS_805/build_x86_64/lib/c/resmgr/_resmgr_io_handler.c:68
#4 _resmgr_io_handler at /builds/workspace/QNXOS_805/build_x86_64/lib/c/resmgr/_resmgr_io_handler.c:308
#5 _resmgr_handler at /builds/workspace/QNXOS_805/build_x86_64/lib/c/resmgr/_resmgr_handler.c:198
#6 _resmgr_msg_handler at /builds/workspace/QNXOS_805/build_x86_64/lib/c/dispatch/resmgr.c:311
#7 main at buggy_rm/buggy_rm.c:154
```

### 3. CRASH in DEVCTL_RAW DCMD_BUGGY_SET_NAME (SIGSEGV) at buggy_devctl

- Bucket `8beea00558ffa42c`, first seen at 164.9 s, 3 hit(s)
- Errno: 3 No such process
- Signal: SIGSEGV
- Reproduce: `resfuzz-agent --target /dev/buggy --spawn "/data/resfuzz/buggy_rm /dev/buggy" --replay crashes/crash_8beea00558ffa42c_2.seq`
- Stateful: needs earlier messages; reproduces deterministically when the recorded 73-case history is replayed against a fresh server

Minimised reproducer (2 ops):

```
#0   slot0 DEVCTL_RAW a=0x80080540 b=0x00000003 len=8 data=41444d4e02000000
#1   slot0 DEVCTL_RAW a=0x80200541 b=0x00000024 len=0
```

Backtrace:

```
#0 buggy_devctl at buggy_rm/buggy_logic.c:112
#1 io_devctl at buggy_rm/buggy_rm.c:108
#2 io_devctl at buggy_rm/buggy_rm.c:75
#3 _resmgr_invoke_handler at /builds/workspace/QNXOS_805/build_x86_64/lib/c/resmgr/_resmgr_io_handler.c:68
#4 _resmgr_io_handler at /builds/workspace/QNXOS_805/build_x86_64/lib/c/resmgr/_resmgr_io_handler.c:308
#5 _resmgr_handler at /builds/workspace/QNXOS_805/build_x86_64/lib/c/resmgr/_resmgr_handler.c:198
#6 _resmgr_msg_handler at /builds/workspace/QNXOS_805/build_x86_64/lib/c/dispatch/resmgr.c:311
#7 main at buggy_rm/buggy_rm.c:154
```

### 4. CRASH in DEVCTL_RAW DCMD_BUGGY_GET_STATS (SIGSEGV) at buggy_devctl

- Bucket `a72696122a46574a`, first seen at 10.0 s, 3 hit(s)
- Errno: 3 No such process
- Signal: SIGSEGV
- Reproduce: `resfuzz-agent --target /dev/buggy --spawn "/data/resfuzz/buggy_rm /dev/buggy" --replay crashes/crash_a72696122a46574a_0.rfz`

Minimised reproducer (1 ops):

```
#0   slot0 DEVCTL_RAW a=0xc0100543 b=0x00000003 len=4 data=04000000
```

Backtrace:

```
#0 buggy_devctl at buggy_rm/buggy_logic.c:124
#1 io_devctl at buggy_rm/buggy_rm.c:108
#2 io_devctl at buggy_rm/buggy_rm.c:75
#3 _resmgr_invoke_handler at /builds/workspace/QNXOS_805/build_x86_64/lib/c/resmgr/_resmgr_io_handler.c:68
#4 _resmgr_io_handler at /builds/workspace/QNXOS_805/build_x86_64/lib/c/resmgr/_resmgr_io_handler.c:308
#5 _resmgr_handler at /builds/workspace/QNXOS_805/build_x86_64/lib/c/resmgr/_resmgr_handler.c:198
#6 _resmgr_msg_handler at /builds/workspace/QNXOS_805/build_x86_64/lib/c/dispatch/resmgr.c:311
#7 main at buggy_rm/buggy_rm.c:154
```

### 5. HANG in DEVCTL_RAW DCMD_BUGGY_COMPUTE

- Bucket `0f165cad4d76fa21`, first seen at 293.4 s, 2 hit(s)
- Errno: 260 Connection timed out
- Reproduce: `resfuzz-agent --target /dev/buggy --spawn "/data/resfuzz/buggy_rm /dev/buggy" --replay hangs/hang_0f165cad4d76fa21_0.rfz`

Minimised reproducer (1 ops):

```
#0   slot0 DEVCTL_RAW a=0xc00c0544 b=0x00000005 len=8 data=0000009701000000
```

### 6. DEADLINE in DEVCTL DCMD_BUGGY_COMPUTE (1108758 us)

- Bucket `0ccd5fc43ce54c11`, first seen at 3.3 s, 3 hit(s)
- Errno: 0
- Worst reply latency: 1108758 us (deadline 100000 us)
- Reproduce: `resfuzz-agent --target /dev/buggy --spawn "/data/resfuzz/buggy_rm /dev/buggy" --replay deadlines/deadline_0ccd5fc43ce54c11_0.rfz`

Test case (8 ops):

```
#0   slot0 DEVCTL_RAW a=0xd1ab2b51 b=0x00000002 len=2 data=d31d
#1   slot0 DEVCTL     a=0xc00c0544 b=0x00000080 len=12 data=0000000041444d4e00000000
#2   slot0 DEVCTL     a=0xc00c0544 b=0x0000000c len=16 data=23911678010000002e02ff7f187755f7
#3   slot0 DEVCTL     a=0x8ac12c6d b=0x00000038 len=56 data=6509e8d63747e98f26a164bbbef63b5380bed89df98a5862676e2735ff02d9aa...
#4   slot0 DEVCTL_RAW a=0x80080540 b=0x00000008 len=8 data=10040000f4ffffff
#5   slot0 DEVCTL     a=0x0000000f b=0x00000400 len=12 data=000800000000001700000000
#6   slot0 DEVCTL     a=0xd1ab2b71 b=0x0000001a len=26 data=d31dc070ee591eb3888528d4c92c951234da3a05ffff05aab0a0
#7   slot0 WRITE      a=0x00000000 b=0x00000000 len=2 data=1f01
```

### 7. DEADLINE in DEVCTL_RAW DCMD_BUGGY_COMPUTE (1019744 us)

- Bucket `318b3194d4fb79fc`, first seen at 5.7 s, 3 hit(s)
- Errno: 0
- Worst reply latency: 1019744 us (deadline 100000 us)
- Reproduce: `resfuzz-agent --target /dev/buggy --spawn "/data/resfuzz/buggy_rm /dev/buggy" --replay deadlines/deadline_318b3194d4fb79fc_0.rfz`

Test case (3 ops):

```
#0   slot0 DEVCTL_RAW a=0xfa0000fa b=0x00000034 len=12 data=00004505007f0094cf82cf00
#1   slot0 DEVCTL_RAW a=0xc00c0544 b=0x00000009 len=16 data=6d15777401000000ab7d823d1671c921
#2   slot0 DEVCTL_RAW a=0x00000545 b=0xc00c0544 len=27 data=001e01000000cf82cf321d3e9c3d0dca0d708a304734a620c56080
```

### 8. DEADLINE in DEVCTL_RAW DCMD_BUGGY_GET_STATS (111436 us)

- Bucket `79ed64cc7b5e3407`, first seen at 528.8 s, 1 hit(s)
- Errno: 0
- Worst reply latency: 111436 us (deadline 100000 us)
- Reproduce: `resfuzz-agent --target /dev/buggy --spawn "/data/resfuzz/buggy_rm /dev/buggy" --replay deadlines/deadline_79ed64cc7b5e3407_0.rfz`

Test case (16 ops):

```
#0   slot0 DEVCTL_RAW a=0xc0100543 b=0x0000000a len=14 data=001effffffffe803657365740ae6
#1   slot0 DEVCTL_RAW a=0x4e4d4441 b=0x00003fff len=0
#2   slot0 DEVCTL_RAW a=0x00000545 b=0x00000000 len=0
#3   slot0 DEVCTL_RAW a=0x00000545 b=0x00003fff len=0
#4   slot0 READ       a=0x00000021 b=0x00000000 len=0
#5   slot0 DEVCTL_RAW a=0xc00c0544 b=0x00000042 len=7 data=00204042052040
#6   slot0 DEVCTL_RAW a=0xe00c0536 b=0x00000008 len=11 data=001f01000082657365740a
#7   slot0 DEVCTL     a=0x80080540 b=0x00000008 len=8 data=fd410520bcff0300
#8   slot0 DEVCTL_RAW a=0x0000ffff b=0x00000004 len=18 data=fdff03004086c7ab30a13ff28a3da8b36230
#9   slot1 DEVCTL     a=0xc00c0539 b=0x0000000c len=17 data=e443436cff00086cff0008000000000000
#10  slot0 DEVCTL_RAW a=0xc0100543 b=0x80000000 len=2 data=0000
#11  slot0 DEVCTL_RAW a=0xc0100543 b=0x00000004 len=34 data=6d1577747c3002fa0000fa3d167100080000001010000000000000008000c91e...
#12  slot0 DEVCTL     a=0x40200542 b=0x00000020 len=59 data=ff0ae8911001000000000000bc39cef5473734924ed3c1e264dbff7b1687532e...
#13  slot0 DEVCTL_RAW a=0xe00e0539 b=0x00003fff len=4 data=20000000
#14  slot0 DEVCTL     a=0x00000545 b=0x00000000 len=0
#15  slot0 DEVCTL     a=0xeb91cde8 b=0x00000027 len=4 data=436ab127
```
