# resfuzz report: `/dev/pipe`

Generated 2026-11-15 18:56 EET on platform `qnx`.

| Metric | Value |
|---|---|
| Executions | 118254 (394.1/s over 300.1 s) |
| Coverage | off (reply-signature feedback) |
| Reply signatures | 8185 |
| Deadline | 100000 us |
| Max latency seen | 566819 us |
| Target restarts | 0 |
| Unique findings | 0 crash, 0 hang, 2 deadline (0 reproducible) |

## Findings

| # | Severity | Finding | Hits | Reproducible |
|---|---|---|---|---|
| 1 | medium | DEADLINE in DEVCTL_RAW 0x02008000 (119153 us) | 1 | not checked |
| 2 | medium | DEADLINE in DEVCTL_RAW 0x00000002 (566819 us) | 1 | not checked |

### 1. DEADLINE in DEVCTL_RAW 0x02008000 (119153 us)

- Bucket `6773abcc745f39d9`, first seen at 4.4 s, 1 hit(s)
- Errno: 0
- Worst reply latency: 119153 us (deadline 100000 us)
- Reproduce: `resfuzz-agent --target /dev/pipe --replay deadlines/deadline_6773abcc745f39d9_0.rfz`

Test case (6 ops):

```
#0   slot0 DEVCTL_RAW a=0x02008000 b=0x00080028 len=49 data=00001f000000000000000000004000000000000000ff00000000000e00000000...
#1   slot0 DEVCTL_RAW a=0x80040105 b=0x00000004 len=4 data=00002020
#2   slot0 DEVCTL_RAW a=0x00000004 b=0x00000000 len=25 data=1233a94e4e8d4e4e4e494e4ea1ee4e4efc184e84644e4ea986
#3   slot0 DEVCTL_RAW a=0x40000400 b=0x00000001 len=51 data=1287b0b9875a71878787878703a58787878787f9874f96a987f9874f96878787...
#4   slot0 DEVCTL_RAW a=0x0000000e b=0xfffffff0 len=4 data=00000200
#5   slot0 DEVCTL_RAW a=0x40040104 b=0x00000017 len=4 data=00000200
```

### 2. DEADLINE in DEVCTL_RAW 0x00000002 (566819 us)

- Bucket `b95e1c8e4dcb3e51`, first seen at 121.7 s, 1 hit(s)
- Errno: 0
- Worst reply latency: 566819 us (deadline 100000 us)
- Reproduce: `resfuzz-agent --target /dev/pipe --replay deadlines/deadline_b95e1c8e4dcb3e51_0.rfz`

Test case (16 ops):

```
#0   slot0 DEVCTL_RAW a=0xfa0002fa b=0x00000008 len=13 data=33100e363648008036362f0000
#1   slot0 DEVCTL_RAW a=0x40040103 b=0x00000019 len=4 data=05e94282
#2   slot0 DEVCTL     a=0x80040105 b=0x0000003e len=62 data=78d0265264e28151aca7628b5ac899fab6f7446a01cfc8ad80708e379a8c727c...
#3   slot0 DEVCTL     a=0x2d8f20c5 b=0x00004000 len=36 data=a04b504650506b349a800070b64823e44e6bd17f2c03268f0f2d7711b47fffff...
#4   slot0 DEVCTL_RAW a=0x0d8f20c5 b=0x0000002e len=65 data=a04b504650506b349a3e6070ab0211ab98abab8bababab57abab9d6dfac0ab17...
#5   slot0 DEVCTL_RAW a=0x80040105 b=0x00001018 len=24 data=008520000000000000000000000000f300000000552c0000
#6   slot0 DEVCTL_RAW a=0x40040104 b=0x00000004 len=4 data=05e94282
#7   slot0 DEVCTL     a=0x40040103 b=0x0000003e len=62 data=78d0265264e28151aca7628b5ac899fab6f7446a01cfc8ad80708e379a8c727c...
#8   slot0 DEVCTL_RAW a=0x40040103 b=0x00000004 len=4 data=86ff7f90
#9   slot0 DEVCTL     a=0x00000007 b=0x00000004 len=6 data=dd4f4fd4a48e
#10  slot0 DEVCTL_RAW a=0x00000002 b=0xfa0000fa len=27 data=008520000000c2236d000000000000000000f300000000552c0000
#11  slot0 DEVCTL     a=0x00008000 b=0x0000003e len=62 data=78d0265264e28151aca7628b5ac899fab6f7446a01cfc8ad80708e379a8c727c...
#12  slot0 DEVCTL     a=0x0d8f20c5 b=0x00000040 len=36 data=a04b504650506b349a3e6070b64823e44e6bd17f2c03268f0f2d7711b47fffff...
#13  slot0 DEVCTL_RAW a=0x0d8f20c5 b=0x0000002e len=36 data=a04b504650506b349a3e6070b64823e44e6bd17f2c03268f0f2d7711b47fffff...
#14  slot0 DEVCTL_RAW a=0x80040105 b=0xfa0000fa len=24 data=008520000000000000000000000000f300000000552c0000
#15  slot0 DEVCTL     a=0x80040102 b=0x00000007 len=4 data=00850000
```
