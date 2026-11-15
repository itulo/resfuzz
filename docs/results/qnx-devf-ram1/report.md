# resfuzz report: `/dev/fs0p0`

Generated 2026-11-15 18:48 EET on platform `qnx`.

| Metric | Value |
|---|---|
| Executions | 9379 (38.8/s over 241.5 s) |
| Coverage | off (reply-signature feedback) |
| Reply signatures | 2858 |
| Deadline | 100000 us |
| Max latency seen | 3621479 us |
| Target restarts | 1 |
| Unique findings | 0 crash, 0 hang, 4 deadline (4 reproducible) |

## Findings

| # | Severity | Finding | Hits | Reproducible |
|---|---|---|---|---|
| 1 | medium | DEADLINE in DEVCTL_RAW DCMD_F3S_ERASE (707738 us) | 3 | yes |
| 2 | medium | DEADLINE in WRITE 0x00000000 (1804034 us) | 3 | yes |
| 3 | medium | DEADLINE in DEVCTL DCMD_F3S_ERASE (715481 us) | 3 | yes |
| 4 | medium | DEADLINE in DEVCTL_RAW DCMD_F3S_FORMAT (285328 us) | 1 | yes |

### 1. DEADLINE in DEVCTL_RAW DCMD_F3S_ERASE (707738 us)

- Bucket `b5eb9a655241e50c`, first seen at 32.3 s, 3 hit(s)
- Errno: 0
- Worst reply latency: 707738 us (deadline 100000 us)
- Reproduce: `resfuzz-agent --target /dev/fs0p0 --spawn "/data/resfuzz/rmwrap --wait /dev/fs0p0 --name devf-ram -- /data/resfuzz/devf-ram -s0x0,4M" --replay deadlines/deadline_b5eb9a655241e50c_0.rfz`

Test case (8 ops):

```
#0   slot0 DEVCTL     a=0x40040103 b=0x00002000 len=4 data=80c629f5
#1   slot0 READ       a=0x00000000 b=0x00000000 len=0
#2   slot0 DEVCTL_RAW a=0x800c0900 b=0x0000000c len=12 data=000000000080000000002300
#3   slot0 DEVCTL_RAW a=0x40040177 b=0x00003fff len=2 data=0184
#4   slot0 OPEN       a=0x7fffffff b=0x00000000 len=0
#5   slot0 LSEEK      a=0x05ffff05 b=0x00000000 len=0
#6   slot0 MSG_RAW    a=0x00000000 b=0x00000000 len=44 data=661e2200d6d2a4cb19b9cb9b1d7f8a52aeed8ad16a95352649880dee7f25674a...
#7   slot0 WRITE      a=0x00002000 b=0x00000000 len=19 data=da4448fd25becbe78220cc0cba08ca9bcd9460
```

### 2. DEADLINE in WRITE 0x00000000 (1804034 us)

- Bucket `e1dab1e1194643f5`, first seen at 1.5 s, 3 hit(s)
- Errno: 0
- Worst reply latency: 1804034 us (deadline 100000 us)
- Reproduce: `resfuzz-agent --target /dev/fs0p0 --spawn "/data/resfuzz/rmwrap --wait /dev/fs0p0 --name devf-ram -- /data/resfuzz/devf-ram -s0x0,4M" --replay deadlines/deadline_e1dab1e1194643f5_0.rfz`

Test case (3 ops):

```
#0   slot0 READ       a=0x00000040 b=0x00000000 len=0
#1   slot0 WRITE      a=0x00000000 b=0x00000000 len=8 data=72657366757a7a0a
#2   slot0 READ       a=0x00000040 b=0x00000000 len=0
```

### 3. DEADLINE in DEVCTL DCMD_F3S_ERASE (715481 us)

- Bucket `f25b61b869adea69`, first seen at 7.4 s, 3 hit(s)
- Errno: 0
- Worst reply latency: 715481 us (deadline 100000 us)
- Reproduce: `resfuzz-agent --target /dev/fs0p0 --spawn "/data/resfuzz/rmwrap --wait /dev/fs0p0 --name devf-ram -- /data/resfuzz/devf-ram -s0x0,4M" --replay deadlines/deadline_f25b61b869adea69_0.rfz`

Test case (2 ops):

```
#0   slot0 DEVCTL     a=0x800c0900 b=0x0000000a len=41 data=000000000a000000032c402c6a2c2c2c2c2c2c2c2c2c2c2cd12c2c4c0fc3c82c...
#1   slot3 LSEEK      a=0x7fffffff b=0x00000000 len=0
```

### 4. DEADLINE in DEVCTL_RAW DCMD_F3S_FORMAT (285328 us)

- Bucket `e3273701493ed989`, first seen at 180.0 s, 1 hit(s)
- Errno: 0
- Worst reply latency: 285328 us (deadline 100000 us)
- Reproduce: `resfuzz-agent --target /dev/fs0p0 --spawn "/data/resfuzz/rmwrap --wait /dev/fs0p0 --name devf-ram -- /data/resfuzz/devf-ram -s0x0,4M" --replay deadlines/deadline_e3273701493ed989_0.rfz`

Test case (7 ops):

```
#0   slot0 DEVCTL_RAW a=0x80000040 b=0x00000020 len=17 data=d700000000000020000000000000002000
#1   slot0 DEVCTL_RAW a=0x801c0901 b=0x0000001c len=28 data=00000000070f0000de1f2c1f0dff1f1f1f1fba1f1f58521f0d1fe6b3
#2   slot0 DEVCTL_RAW a=0xc804191d b=0x00000004 len=4 data=00807fff
#3   slot0 DEVCTL_RAW a=0xc004391c b=0x00000005 len=5 data=0000800000
#4   slot0 DEVCTL_RAW a=0xc004191d b=0x0000000f len=8 data=0080000005000000
#5   slot0 DEVCTL     a=0xc0040915 b=0x00000004 len=4 data=737ea647
#6   slot0 DEVCTL_RAW a=0x00000908 b=0x00000000 len=0
```
