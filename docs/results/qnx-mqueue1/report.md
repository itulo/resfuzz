# resfuzz report: `/dev/mqueue/rfz`

Generated 2026-11-15 18:40 EET on platform `qnx`.

| Metric | Value |
|---|---|
| Executions | 59087 (328.0/s over 180.1 s) |
| Coverage | off (reply-signature feedback) |
| Reply signatures | 8179 |
| Deadline | 100000 us |
| Max latency seen | 126761 us |
| Target restarts | 0 |
| Unique findings | 0 crash, 0 hang, 2 deadline (0 reproducible) |

## Findings

| # | Severity | Finding | Hits | Reproducible |
|---|---|---|---|---|
| 1 | low | DEADLINE in DEVCTL 0x40040104 (126761 us) | 1 | no (stateful/flaky) |
| 2 | low | DEADLINE in DEVCTL_RAW 0x91190106 (122212 us) | 1 | no (stateful/flaky) |

### 1. DEADLINE in DEVCTL 0x40040104 (126761 us)

- Bucket `89ee84daea591565`, first seen at 173.3 s, 1 hit(s)
- Errno: 0
- Worst reply latency: 126761 us (deadline 100000 us)
- Reproduce: `resfuzz-agent --target /dev/mqueue/rfz --spawn "/data/resfuzz/mqwrap /data/resfuzz/mqueue /rfz" --replay deadlines/deadline_89ee84daea591565_0.rfz`

Test case (14 ops):

```
#0   slot0 DEVCTL     a=0x80180d02 b=0xffff7fff len=24 data=7958fab900fa29d6a2ab2eae869027cdb197847228ec1abe
#1   slot0 DEVCTL_RAW a=0x40180501 b=0x00000003 len=24 data=61a5f0e34a86a80fab4f9b9b83bba54e359c7c5f42c6e03d
#2   slot0 DEVCTL     a=0x80180506 b=0xfffffff5 len=24 data=0000afaf5fafafe3ec47afb3af98af59af47af49af710000
#3   slot0 DEVCTL     a=0x80400504 b=0x00000004 len=1 data=c9
#4   slot0 DEVCTL     a=0x40100514 b=0x00000010 len=16 data=4631be0fddcf548b3e2b83778c03c53a
#5   slot0 DEVCTL     a=0x801800f8 b=0xfffffff5 len=24 data=0000afe4d0472159e4e4f56be46e48e4e420e4ade4dcfa37
#6   slot0 DEVCTL_RAW a=0xffff7fff b=0x00000002 len=1 data=10
#7   slot0 DEVCTL     a=0x40040104 b=0x00000004 len=4 data=df00000f
#8   slot0 DEVCTL     a=0x40040103 b=0x00000049 len=4 data=df000000
#9   slot0 DEVCTL     a=0x80180109 b=0x00000049 len=22 data=df0000ff000000000000000000000000000000000000
#10  slot0 DEVCTL_RAW a=0x7b013859 b=0x00000016 len=22 data=5fb2642fd0cbb59fa3cb47284d0a176a9e4b8792dd4f
#11  slot0 DEVCTL     a=0x84040122 b=0x00000004 len=4 data=4d266495
#12  slot0 DEVCTL     a=0x80400504 b=0x00000039 len=57 data=4d9e2e46e5494060693eab51e83104cf8e136dcd5157cdfdee6c1b0335af3ffd...
#13  slot0 DEVCTL     a=0x80180106 b=0x00000012 len=24 data=0000000000000000000000000b000000000000b200000000
```

### 2. DEADLINE in DEVCTL_RAW 0x91190106 (122212 us)

- Bucket `980dfd3135269648`, first seen at 104.8 s, 1 hit(s)
- Errno: 0
- Worst reply latency: 122212 us (deadline 100000 us)
- Reproduce: `resfuzz-agent --target /dev/mqueue/rfz --spawn "/data/resfuzz/mqwrap /data/resfuzz/mqueue /rfz" --replay deadlines/deadline_980dfd3135269648_0.rfz`

Test case (6 ops):

```
#0   slot0 DEVCTL_RAW a=0x40180501 b=0x00000004 len=4 data=c19de89d
#1   slot0 LSEEK      a=0x00008000 b=0x00000001 len=0
#2   slot0 DEVCTL_RAW a=0x05ffff05 b=0x00000048 len=20 data=0000000000000000d20400000100e1ff08000000
#3   slot0 DEVCTL     a=0x00010000 b=0x00000031 len=43 data=0000007c00000000d200000001000000000000d2000000010000000100000000...
#4   slot0 DEVCTL_RAW a=0x91190106 b=0x00000023 len=34 data=040001ac724f55acb646f3d5fdacfcb646f3d5acac0001000000dd617070140a...
#5   slot0 DEVCTL_RAW a=0x91190106 b=0x02000000 len=34 data=040001ac724f55acb646f3d5acacfcb646f3d5acac0001000000ddbc7070140a...
```
