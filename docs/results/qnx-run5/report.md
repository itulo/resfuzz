# resfuzz report: `/dev/buggy`

Generated 2026-11-15 10:57 EET on platform `qnx`.

| Metric | Value |
|---|---|
| Executions | 136106 (226.8/s over 600.2 s) |
| Coverage | on, 107 edges |
| Reply signatures | 8090 |
| Deadline | 100000 us |
| Max latency seen | 4269037 us |
| Target restarts | 227 |
| Unique findings | 3 crash, 1 hang, 5 deadline (6 reproducible) |

## Findings

| # | Severity | Finding | Hits | Reproducible |
|---|---|---|---|---|
| 1 | high | CRASH in DEVCTL DCMD_BUGGY_SET_NAME (SIGSEGV) at buggy_devctl | 3 | yes, stateful (271-case sequence) |
| 2 | high | CRASH in DEVCTL DCMD_BUGGY_GET_STATS (SIGSEGV) at buggy_devctl | 3 | yes |
| 3 | high | CRASH in DEVCTL_RAW DCMD_BUGGY_GET_STATS (SIGSEGV) at buggy_devctl | 3 | yes |
| 4 | medium | HANG in DEVCTL_RAW DCMD_BUGGY_COMPUTE at compute | 1 | yes |
| 5 | medium | DEADLINE in DEVCTL DCMD_BUGGY_COMPUTE (778035 us) | 3 | yes |
| 6 | medium | DEADLINE in DEVCTL_RAW DCMD_BUGGY_COMPUTE (991297 us) | 3 | yes |
| 7 | low | DEADLINE in DEVCTL_RAW 0xffffffff (103863 us) | 1 | no (stateful/flaky) |
| 8 | low | DEADLINE in DEVCTL_RAW 0x80081145 (106867 us) | 1 | no (stateful/flaky) |
| 9 | low | DEADLINE in DEVCTL_RAW 0x05ffff05 (188674 us) | 1 | no (stateful/flaky) |

### 1. CRASH in DEVCTL DCMD_BUGGY_SET_NAME (SIGSEGV) at buggy_devctl

- Bucket `38d435f06c3e3629`, first seen at 173.4 s, 3 hit(s)
- Errno: 3 No such process
- Signal: SIGSEGV
- Reproduce: `resfuzz-agent --target /dev/buggy --spawn "/data/resfuzz/buggy_rm /dev/buggy" --replay crashes/crash_38d435f06c3e3629_2.seq`
- Stateful: needs earlier messages; reproduces deterministically when the recorded 271-case history is replayed against a fresh server

Minimised reproducer (2 ops):

```
#0   slot0 DEVCTL_RAW a=0x80080540 b=0x00000004 len=8 data=41444d4e02000000
#1   slot0 DEVCTL     a=0x80200541 b=0x00000020 len=34 data=0000000000000000000000000000000000000000000000000000000000000000...
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

- Bucket `3cf05774be8055ff`, first seen at 31.2 s, 3 hit(s)
- Errno: 3 No such process
- Signal: SIGSEGV
- Reproduce: `resfuzz-agent --target /dev/buggy --spawn "/data/resfuzz/buggy_rm /dev/buggy" --replay crashes/crash_3cf05774be8055ff_0.rfz`

Minimised reproducer (1 ops):

```
#0   slot0 DEVCTL     a=0xc0100543 b=0x0000001d len=1 data=07
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

### 3. CRASH in DEVCTL_RAW DCMD_BUGGY_GET_STATS (SIGSEGV) at buggy_devctl

- Bucket `a72696122a46574a`, first seen at 12.6 s, 3 hit(s)
- Errno: 3 No such process
- Signal: SIGSEGV
- Reproduce: `resfuzz-agent --target /dev/buggy --spawn "/data/resfuzz/buggy_rm /dev/buggy" --replay crashes/crash_a72696122a46574a_0.rfz`

Minimised reproducer (1 ops):

```
#0   slot0 DEVCTL_RAW a=0xc0100543 b=0x00000017 len=4 data=07000000
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

### 4. HANG in DEVCTL_RAW DCMD_BUGGY_COMPUTE at compute

- Bucket `0f165cad4d76fa21`, first seen at 198.5 s, 1 hit(s)
- Errno: 260 Connection timed out
- Reproduce: `resfuzz-agent --target /dev/buggy --spawn "/data/resfuzz/buggy_rm /dev/buggy" --replay hangs/hang_0f165cad4d76fa21_0.rfz`

Minimised reproducer (1 ops):

```
#0   slot0 DEVCTL_RAW a=0xc00c0544 b=0x00000021 len=8 data=ffffffff01000000
```

Backtrace:

```
#0 compute at buggy_rm/buggy_logic.c:77
#1 buggy_devctl at buggy_rm/buggy_logic.c:133
#2 io_devctl at buggy_rm/buggy_rm.c:108
#3 io_devctl at buggy_rm/buggy_rm.c:75
#4 _resmgr_invoke_handler at /builds/workspace/QNXOS_805/build_x86_64/lib/c/resmgr/_resmgr_io_handler.c:68
#5 _resmgr_io_handler at /builds/workspace/QNXOS_805/build_x86_64/lib/c/resmgr/_resmgr_io_handler.c:308
#6 _resmgr_handler at /builds/workspace/QNXOS_805/build_x86_64/lib/c/resmgr/_resmgr_handler.c:198
#7 _resmgr_msg_handler at /builds/workspace/QNXOS_805/build_x86_64/lib/c/dispatch/resmgr.c:311
#8 main at buggy_rm/buggy_rm.c:154
```

### 5. DEADLINE in DEVCTL DCMD_BUGGY_COMPUTE (778035 us)

- Bucket `0ccd5fc43ce54c11`, first seen at 9.7 s, 3 hit(s)
- Errno: 0
- Worst reply latency: 778035 us (deadline 100000 us)
- Reproduce: `resfuzz-agent --target /dev/buggy --spawn "/data/resfuzz/buggy_rm /dev/buggy" --replay deadlines/deadline_0ccd5fc43ce54c11_0.rfz`

Test case (2 ops):

```
#0   slot0 DEVCTL_RAW a=0x00000546 b=0x00000000 len=0
#1   slot0 DEVCTL     a=0xc00c0544 b=0x0000000c len=16 data=4305bac0010000000000000000000000
```

### 6. DEADLINE in DEVCTL_RAW DCMD_BUGGY_COMPUTE (991297 us)

- Bucket `318b3194d4fb79fc`, first seen at 7.7 s, 3 hit(s)
- Errno: 0
- Worst reply latency: 991297 us (deadline 100000 us)
- Reproduce: `resfuzz-agent --target /dev/buggy --spawn "/data/resfuzz/buggy_rm /dev/buggy" --replay deadlines/deadline_318b3194d4fb79fc_0.rfz`

Test case (2 ops):

```
#0   slot0 DEVCTL_RAW a=0xc00c0544 b=0x00000005 len=12 data=00007b000100000000800000
#1   slot0 DEVCTL     a=0xc0100543 b=0x0000000c len=38 data=101000000000000000ec44784444ae5d44104444446340444444db44440e9044...
```

### 7. DEADLINE in DEVCTL_RAW 0xffffffff (103863 us)

- Bucket `af6558d70bb37faf`, first seen at 348.8 s, 1 hit(s)
- Errno: 0
- Worst reply latency: 103863 us (deadline 100000 us)
- Reproduce: `resfuzz-agent --target /dev/buggy --spawn "/data/resfuzz/buggy_rm /dev/buggy" --replay deadlines/deadline_af6558d70bb37faf_0.rfz`

Test case (9 ops):

```
#0   slot0 DEVCTL     a=0xbeb1c18a b=0x0000002b len=62 data=470e916c1b050f27d58543362fac68d269b223551d345ccf6e6970f843362fac...
#1   slot0 DEVCTL_RAW a=0xffffffff b=0x7ffffff9 len=7 data=40051380c084d4
#2   slot0 DEVCTL_RAW a=0xffffffff b=0x00000001 len=7 data=40051380c084d4
#3   slot0 DEVCTL_RAW a=0xffffffff b=0x7fffffff len=7 data=40051380c084d4
#4   slot0 DEVCTL_RAW a=0x80080540 b=0x00000008 len=8 data=0000000000000000
#5   slot0 LSEEK      a=0x00010000 b=0x00000000 len=0
#6   slot0 DEVCTL_RAW a=0x0000ffff b=0x00000000 len=0
#7   slot0 DEVCTL     a=0x80080540 b=0x00000ffe len=17 data=ec1bb609a30905430510c0c30909091c09
#8   slot0 READ       a=0x00000080 b=0x00000000 len=0
```

### 8. DEADLINE in DEVCTL_RAW 0x80081145 (106867 us)

- Bucket `f14b3f579817d679`, first seen at 218.0 s, 1 hit(s)
- Errno: 0
- Worst reply latency: 106867 us (deadline 100000 us)
- Reproduce: `resfuzz-agent --target /dev/buggy --spawn "/data/resfuzz/buggy_rm /dev/buggy" --replay deadlines/deadline_f14b3f579817d679_0.rfz`

Test case (6 ops):

```
#0   slot0 DEVCTL     a=0x672fe07c b=0x0000ffff len=98 data=de448cbe91b99efa2da885a15ffd5aeb4ed9f22f6465762f62756767796053a7...
#1   slot0 DEVCTL_RAW a=0x40200542 b=0x00000020 len=34 data=41444d4e344d4e42425b20407c00000000000000000000000000000000000000...
#2   slot0 DEVCTL     a=0x80200541 b=0x00000010 len=32 data=2441444d4e35e17df0b53f0cef4276ec9ebeec11000000000000000000000000
#3   slot0 DEVCTL_RAW a=0x80081145 b=0x00010000 len=8 data=1f017365740a0000
#4   slot0 DEVCTL     a=0xe96e9e92 b=0x00003fff len=58 data=bf8b127f4778a7f97c8d4e73cce22d678fad408b127f4778a7f97c8d4e73cce2...
#5   slot0 DEVCTL_RAW a=0x00000186 b=0x80020000 len=0
```

### 9. DEADLINE in DEVCTL_RAW 0x05ffff05 (188674 us)

- Bucket `fd6b54fc065c0d87`, first seen at 384.9 s, 1 hit(s)
- Errno: 0
- Worst reply latency: 188674 us (deadline 100000 us)
- Reproduce: `resfuzz-agent --target /dev/buggy --spawn "/data/resfuzz/buggy_rm /dev/buggy" --replay deadlines/deadline_fd6b54fc065c0d87_0.rfz`

Test case (16 ops):

```
#0   slot0 DEVCTL_RAW a=0x0011021e b=0x7fffffff len=48 data=41444d4e42052040e8c1c1000010100000000000e1ff00000000e1ff6d800000...
#1   slot0 DEVCTL_RAW a=0x0011021e b=0x7fffffff len=40 data=41444d4e42052040e8c1c1000010100000000000e1ff00000000e1ff6d800000...
#2   slot0 DEVCTL_RAW a=0xc00c0544 b=0x0000000c len=11 data=060040bdee1e01eeee91ee
#3   slot0 DEVCTL     a=0xe96e8ea0 b=0x80000019 len=71 data=bf8b127f4778a7f97c8d4e73cce2cce216bf8b127f4778a7f97c8d4e73cce216...
#4   slot0 DEVCTL_RAW a=0x7fffffff b=0x01000008 len=8 data=0000000100000000
#5   slot0 DEVCTL_RAW a=0x7fffffff b=0x00000001 len=13 data=07000010100000000000000400
#6   slot0 DEVCTL_RAW a=0x80200541 b=0x00000008 len=41 data=04f2848706b7a6bba6a6e8a68ab55b87a6a6a63fa2a681a6a6a6a66aa0ae5d01...
#7   slot0 DEVCTL_RAW a=0x90082139 b=0x0000003b len=7 data=001f0100000000
#8   slot0 DEVCTL_RAW a=0x80200541 b=0x00000018 len=24 data=0be70b640b0b4d792bdf0b0b0b0b45200bffe1009d159d9d
#9   slot0 DEVCTL     a=0x80200541 b=0x0000008a len=144 data=00008000000000000000000042052040c0080000000000000000000040050880...
#10  slot0 DEVCTL_RAW a=0x00008000 b=0x00000015 len=5 data=0000006400
#11  slot0 OPEN       a=0x00000000 b=0x00000000 len=0
#12  slot0 DEVCTL     a=0xc00c054e b=0x00000009 len=20 data=0000000100000000000000010000000000000000
#13  slot0 DEVCTL_RAW a=0xc00c0554 b=0x0000005b len=42 data=000000000000000000000000000000e500000002ffbfff00042f627540050880...
#14  slot0 READ       a=0x0000001d b=0x00000000 len=0
#15  slot0 DEVCTL_RAW a=0x05ffff05 b=0x00000030 len=48 data=0000000000000000000000000000000000000000000000000000000000000000...
```
