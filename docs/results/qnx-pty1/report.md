# resfuzz report: `/dev/ptyp0`

Generated 2026-11-15 11:17 EET on platform `qnx`.

| Metric | Value |
|---|---|
| Executions | 83270 (277.3/s over 300.3 s) |
| Coverage | off (reply-signature feedback) |
| Reply signatures | 8148 |
| Deadline | 100000 us |
| Max latency seen | 1235632 us |
| Target restarts | 83 |
| Unique findings | 0 crash, 0 hang, 20 deadline (0 reproducible) |

## Findings

| # | Severity | Finding | Hits | Reproducible |
|---|---|---|---|---|
| 1 | low | DEADLINE in DEVCTL_RAW DCMD_CHR_GRANTPT_EXPLICIT (270914 us) | 1 | no (stateful/flaky) |
| 2 | low | DEADLINE in LSEEK 0x00010014 (338707 us) | 1 | no (stateful/flaky) |
| 3 | low | DEADLINE in DEVCTL_RAW DCMD_CHR_PNPTEXT (442892 us) | 1 | no (stateful/flaky) |
| 4 | low | DEADLINE in DEVCTL_RAW 0x00000311 (805128 us) | 1 | no (stateful/flaky) |
| 5 | low | DEADLINE in DEVCTL 0x804c73ff (331961 us) | 1 | no (stateful/flaky) |
| 6 | low | DEADLINE in DEVCTL 0x4ec3a124 (120704 us) | 1 | no (stateful/flaky) |
| 7 | low | DEADLINE in DEVCTL_RAW DCMD_CHR_GET_DISCARD_OUTPUT (1082999 us) | 1 | no (stateful/flaky) |
| 8 | low | DEADLINE in DEVCTL 0x00000111 (129223 us) | 1 | no (stateful/flaky) |
| 9 | low | DEADLINE in DEVCTL_RAW 0xb75ccdd5 (187909 us) | 1 | no (stateful/flaky) |
| 10 | low | DEADLINE in DEVCTL_RAW 0x0000f0f8 (199581 us) | 1 | no (stateful/flaky) |
| 11 | low | DEADLINE in DEVCTL 0x08ec26b0 (419467 us) | 1 | no (stateful/flaky) |
| 12 | low | DEADLINE in DEVCTL DCMD_CHR_TCSETSID (452715 us) | 1 | no (stateful/flaky) |
| 13 | low | DEADLINE in DEVCTL_RAW DCMD_CHR_RESET (555240 us) | 1 | no (stateful/flaky) |
| 14 | low | DEADLINE in DEVCTL_RAW DCMD_CHR_GRANTPT (201189 us) | 1 | no (stateful/flaky) |
| 15 | low | DEADLINE in DEVCTL_RAW DCMD_CHR_TCSETATTR (165033 us) | 1 | no (stateful/flaky) |
| 16 | low | DEADLINE in DEVCTL_RAW DCMD_CHR_FORCE_RTS (1235632 us) | 1 | no (stateful/flaky) |
| 17 | low | DEADLINE in DEVCTL_RAW DCMD_CHR_SET_LOGGING_DIR (151459 us) | 1 | no (stateful/flaky) |
| 18 | low | DEADLINE in DEVCTL_RAW DCMD_CHR_IDLE (148831 us) | 1 | no (stateful/flaky) |
| 19 | low | DEADLINE in DEVCTL DCMD_CHR_SET_DISCARD_OUTPUT (237425 us) | 1 | no (stateful/flaky) |
| 20 | low | DEADLINE in DEVCTL_RAW 0x6224030a (319392 us) | 1 | no (stateful/flaky) |

### 1. DEADLINE in DEVCTL_RAW DCMD_CHR_GRANTPT_EXPLICIT (270914 us)

- Bucket `02733c6fb18892d5`, first seen at 257.4 s, 1 hit(s)
- Errno: 0
- Worst reply latency: 270914 us (deadline 100000 us)
- Reproduce: `resfuzz-agent --target /dev/ptyp0 --spawn "/data/resfuzz/devc-pty -n 1 -o nodaemon" --replay deadlines/deadline_02733c6fb18892d5_0.rfz`

Test case (9 ops):

```
#0   slot0 DEVCTL_RAW a=0x40040327 b=0x00000004 len=4 data=00000000
#1   slot0 DEVCTL_RAW a=0x0000745e b=0x0000004c len=76 data=e7c5af8600000000000000000000000000000000000000000000000000000000...
#2   slot0 DEVCTL_RAW a=0x80040314 b=0x00000024 len=4 data=00400000
#3   slot0 DEVCTL_RAW a=0x800c032b b=0x00000011 len=0
#4   slot0 MSG_RAW    a=0x00000000 b=0x00000000 len=73 data=340f810f0000005fe6616b286b91777f037b3ef4a7d87c29e0d929d4fa199500...
#5   slot0 DEVCTL_RAW a=0x800c032b b=0x00000000 len=0
#6   slot0 MSG_RAW    a=0x00000000 b=0x00000000 len=59 data=340f810f0f00005fe6616b286b91777f037b3ef4a7d87c29e0d929d4fa199500...
#7   slot0 DEVCTL_RAW a=0x8002011f b=0xffffffff len=0
#8   slot0 DEVCTL     a=0x00008000 b=0x00000800 len=0
```

### 2. DEADLINE in LSEEK 0x00010014 (338707 us)

- Bucket `17891fbf533f2880`, first seen at 239.6 s, 1 hit(s)
- Errno: 0
- Worst reply latency: 338707 us (deadline 100000 us)
- Reproduce: `resfuzz-agent --target /dev/ptyp0 --spawn "/data/resfuzz/devc-pty -n 1 -o nodaemon" --replay deadlines/deadline_17891fbf533f2880_0.rfz`

Test case (6 ops):

```
#0   slot1 LSEEK      a=0xffff5fff b=0x00000002 len=0
#1   slot0 DEVCTL_RAW a=0x00000e4f b=0x00000000 len=0
#2   slot0 DEVCTL_RAW a=0x00000e4f b=0x00000000 len=0
#3   slot0 DEVCTL_RAW a=0x00000e4f b=0x00000000 len=0
#4   slot0 LSEEK      a=0x00010014 b=0x00000000 len=0
#5   slot0 DEVCTL_RAW a=0x00008000 b=0x80000000 len=0
```

### 3. DEADLINE in DEVCTL_RAW DCMD_CHR_PNPTEXT (442892 us)

- Bucket `18d60fb1fc298368`, first seen at 269.5 s, 1 hit(s)
- Errno: 0
- Worst reply latency: 442892 us (deadline 100000 us)
- Reproduce: `resfuzz-agent --target /dev/ptyp0 --spawn "/data/resfuzz/devc-pty -n 1 -o nodaemon" --replay deadlines/deadline_18d60fb1fc298368_0.rfz`

Test case (16 ops):

```
#0   slot0 DEVCTL_RAW a=0x00000200 b=0x0000003f len=0
#1   slot0 DEVCTL_RAW a=0x80057476 b=0x00000004 len=32 data=2f00100000989898959873a99898989898989898683caad6c9389896ac60cd89
#2   slot0 DEVCTL     a=0x4004031d b=0x00400004 len=6 data=000000000000
#3   slot0 WRITE      a=0x00000040 b=0x00000000 len=8 data=0040008000000000
#4   slot0 DEVCTL     a=0x0000100c b=0x00000003 len=3 data=001000
#5   slot0 DEVCTL_RAW a=0x40010363 b=0x00000014 len=8 data=0040000000000000
#6   slot0 DEVCTL     a=0x80045406 b=0x0000001c len=23 data=47c162085c0a482df29b626213356262ad629f9f4be87b
#7   slot0 DEVCTL_RAW a=0x804c7414 b=0x0000004c len=76 data=0008000000000000000000000000000000000000000000000000000000000004...
#8   slot0 DEVCTL_RAW a=0x0000e0ee b=0x00000400 len=0
#9   slot0 DEVCTL     a=0x23d91ec7 b=0x00000006 len=6 data=d21afdc64420
#10  slot1 READ       a=0x0000ffff b=0x00000000 len=0
#11  slot0 DEVCTL_RAW a=0x80040316 b=0x00000010 len=16 data=94000040000080000040000000100000
#12  slot0 DEVCTL     a=0x40040328 b=0x00002000 len=1 data=7a
#13  slot0 DEVCTL     a=0x8004740c b=0x00000008 len=34 data=7a31c15c5ca367c2205c905caf5c2cc3ebd05c5c5c7cff5c0c059b5c5c2b5cf8...
#14  slot0 DEVCTL     a=0x80040316 b=0x0000003f len=35 data=d9a87900400000790b2a79654c4112ff797d7aea7d647d227dc37d7d057d8e7d...
#15  slot0 DEVCTL_RAW a=0xc008030b b=0x00000002 len=3 data=6eff59
```

### 4. DEADLINE in DEVCTL_RAW 0x00000311 (805128 us)

- Bucket `2359f77c57f21387`, first seen at 163.3 s, 1 hit(s)
- Errno: 0
- Worst reply latency: 805128 us (deadline 100000 us)
- Reproduce: `resfuzz-agent --target /dev/ptyp0 --spawn "/data/resfuzz/devc-pty -n 1 -o nodaemon" --replay deadlines/deadline_2359f77c57f21387_0.rfz`

Test case (3 ops):

```
#0   slot0 DEVCTL_RAW a=0x00000311 b=0x00000000 len=0
#1   slot0 DEVCTL_RAW a=0x7ffffffa b=0x0000002b len=0
#2   slot0 DEVCTL_RAW a=0x00008208 b=0x00000000 len=0
```

### 5. DEADLINE in DEVCTL 0x804c73ff (331961 us)

- Bucket `23b1e08e9af47850`, first seen at 238.7 s, 1 hit(s)
- Errno: 0
- Worst reply latency: 331961 us (deadline 100000 us)
- Reproduce: `resfuzz-agent --target /dev/ptyp0 --spawn "/data/resfuzz/devc-pty -n 1 -o nodaemon" --replay deadlines/deadline_23b1e08e9af47850_0.rfz`

Test case (8 ops):

```
#0   slot0 DEVCTL_RAW a=0x0000032a b=0x00000000 len=0
#1   slot0 DEVCTL     a=0x804c73ff b=0x4000004c len=68 data=d160d3f1f48dd57e1c18ebb5dd672047e59f108e7bc0405babe142e46a930011...
#2   slot0 WRITE      a=0x00000000 b=0x00000000 len=73 data=bd9c0d502a62c269c35f8c5439280ba796c2380080000057e6b3be119eecfe86...
#3   slot0 DEVCTL_RAW a=0x05ffff05 b=0x00000027 len=0
#4   slot0 DEVCTL_RAW a=0x80047410 b=0x00000001 len=4 data=00800000
#5   slot0 DEVCTL_RAW a=0x80045406 b=0x00000004 len=4 data=989898dc
#6   slot0 DEVCTL_RAW a=0x00000321 b=0x00000004 len=0
#7   slot0 DEVCTL_RAW a=0x00001000 b=0x00000000 len=0
```

### 6. DEADLINE in DEVCTL 0x4ec3a124 (120704 us)

- Bucket `354de11640507290`, first seen at 258.5 s, 1 hit(s)
- Errno: 0
- Worst reply latency: 120704 us (deadline 100000 us)
- Reproduce: `resfuzz-agent --target /dev/ptyp0 --spawn "/data/resfuzz/devc-pty -n 1 -o nodaemon" --replay deadlines/deadline_354de11640507290_0.rfz`

Test case (10 ops):

```
#0   slot0 DEVCTL_RAW a=0x00000007 b=0x00000000 len=9 data=0f0f00003873737373
#1   slot0 DEVCTL_RAW a=0x40047477 b=0x00000004 len=4 data=00800000
#2   slot3 WRITE      a=0x00000000 b=0x00000000 len=4 data=f0f00000
#3   slot0 DEVCTL_RAW a=0x00000879 b=0x7fffffff len=0
#4   slot0 DEVCTL_RAW a=0x4204039d b=0x00000004 len=4 data=f9ffbf05
#5   slot0 DEVCTL     a=0x8008032c b=0x0000007f len=12 data=100080002000000500000000
#6   slot2 MSG_RAW    a=0x00000000 b=0x00000000 len=43 data=000638e3f46abfa5b0dc982b78403b950020000081f17ccfadbbfe2c4468723d...
#7   slot0 DEVCTL     a=0x40047473 b=0x00000004 len=4 data=59edb9d0
#8   slot0 DEVCTL     a=0x80040329 b=0x0000002f len=4 data=05ffbf05
#9   slot0 DEVCTL     a=0x4ec3a124 b=0x00000014 len=20 data=cfc795555ad4f694ed0d5a7b90f5483007723201
```

### 7. DEADLINE in DEVCTL_RAW DCMD_CHR_GET_DISCARD_OUTPUT (1082999 us)

- Bucket `371121e6a36d00b2`, first seen at 259.9 s, 1 hit(s)
- Errno: 0
- Worst reply latency: 1082999 us (deadline 100000 us)
- Reproduce: `resfuzz-agent --target /dev/ptyp0 --spawn "/data/resfuzz/devc-pty -n 1 -o nodaemon" --replay deadlines/deadline_371121e6a36d00b2_0.rfz`

Test case (2 ops):

```
#0   slot0 DEVCTL_RAW a=0x40040328 b=0x00000000 len=0
#1   slot0 DEVCTL_RAW a=0x00000040 b=0x00000004 len=0
```

### 8. DEADLINE in DEVCTL 0x00000111 (129223 us)

- Bucket `4b64e30f70ebb338`, first seen at 204.0 s, 1 hit(s)
- Errno: 0
- Worst reply latency: 129223 us (deadline 100000 us)
- Reproduce: `resfuzz-agent --target /dev/ptyp0 --spawn "/data/resfuzz/devc-pty -n 1 -o nodaemon" --replay deadlines/deadline_4b64e30f70ebb338_0.rfz`

Test case (6 ops):

```
#0   slot0 DEVCTL_RAW a=0x4044031c b=0x00000050 len=17 data=0400400000000000400000000000000000
#1   slot0 DEVCTL_RAW a=0xd7381244 b=0x0000002c len=7 data=000100000000ff
#2   slot0 DEVCTL_RAW a=0xd7381244 b=0x00000003 len=11 data=0001000f0f0000000000ff
#3   slot0 DEVCTL     a=0x00008000 b=0x00000800 len=0
#4   slot0 DEVCTL     a=0x00000111 b=0x00000000 len=0
#5   slot0 DEVCTL     a=0x00000111 b=0x00000000 len=0
```

### 9. DEADLINE in DEVCTL_RAW 0xb75ccdd5 (187909 us)

- Bucket `55faf7a65a950b1c`, first seen at 254.1 s, 1 hit(s)
- Errno: 0
- Worst reply latency: 187909 us (deadline 100000 us)
- Reproduce: `resfuzz-agent --target /dev/ptyp0 --spawn "/data/resfuzz/devc-pty -n 1 -o nodaemon" --replay deadlines/deadline_55faf7a65a950b1c_0.rfz`

Test case (16 ops):

```
#0   slot0 DEVCTL     a=0x40087468 b=0x00000007 len=7 data=00400000000000
#1   slot0 DEVCTL_RAW a=0x80040325 b=0x00000004 len=4 data=f0f00000
#2   slot0 DEVCTL_RAW a=0x00000200 b=0x00000000 len=0
#3   slot0 DEVCTL_RAW a=0x00000000 b=0x00000000 len=0
#4   slot0 DEVCTL_RAW a=0xb75ccdd5 b=0x00000003 len=17 data=f0f0000000800000c5ad85b9fc9e7e25d4
#5   slot0 MSG_RAW    a=0x00000000 b=0x00000000 len=17 data=00200000292cf69b6f89ba50e13e819268
#6   slot0 DEVCTL     a=0x804c7415 b=0x0000004c len=76 data=6100f0f00000006c459a459a6f00000000000000000000000000000000000000...
#7   slot0 DEVCTL_RAW a=0x00000200 b=0x000007ff len=5 data=006c459a6f
#8   slot0 DEVCTL     a=0xe5918048 b=0x00000036 len=36 data=55168ed05f5e02cc5d76a2e82869013211bc919e12002944b420d5fbe388042c...
#9   slot0 DEVCTL_RAW a=0xfa0000fa b=0x00000000 len=0
#10  slot0 LSEEK      a=0x7fffffff b=0x00000000 len=0
#11  slot0 DEVCTL     a=0x40087468 b=0x00000000 len=13 data=61000100006c0071004001006f
#12  slot0 DEVCTL_RAW a=0xf92ac0d8 b=0x00000005 len=12 data=3273e532dafb8b738181d7e5
#13  slot0 DEVCTL_RAW a=0x00082000 b=0x00000006 len=24 data=53cb711a84ca84844f7a1584d6842f848484848484848484
#14  slot0 DEVCTL_RAW a=0x80047476 b=0x00000003 len=4 data=ee9abcff
#15  slot0 DEVCTL_RAW a=0x80047476 b=0x00000003 len=8 data=ee9abcf0f00000ff
```

### 10. DEADLINE in DEVCTL_RAW 0x0000f0f8 (199581 us)

- Bucket `5ee606522b2828db`, first seen at 8.3 s, 1 hit(s)
- Errno: 0
- Worst reply latency: 199581 us (deadline 100000 us)
- Reproduce: `resfuzz-agent --target /dev/ptyp0 --spawn "/data/resfuzz/devc-pty -n 1 -o nodaemon" --replay deadlines/deadline_5ee606522b2828db_0.rfz`

Test case (8 ops):

```
#0   slot0 DEVCTL_RAW a=0x0000f0f8 b=0x00000000 len=0
#1   slot0 DEVCTL_RAW a=0x00000325 b=0x0000000e len=0
#2   slot0 DEVCTL_RAW a=0x0000f0f8 b=0x00000000 len=0
#3   slot0 READ       a=0x0000007f b=0x00000000 len=0
#4   slot0 DEVCTL_RAW a=0x404c7413 b=0x0000004c len=76 data=84785ab3d5d74b85b83b4dc4d3318bd24108e516433b5a4d6a00e755e0847fa6...
#5   slot0 DEVCTL     a=0x75f983d8 b=0x0000001d len=29 data=350a24085de7a8dfd16141a8e1a226aef01e17076f84f5812b701237ac
#6   slot0 DEVCTL     a=0x80000000 b=0x00000040 len=13 data=c75dc095d6982bd1ff98987a98
#7   slot0 DEVCTL     a=0x8004031e b=0x00000004 len=4 data=f010f010
```

### 11. DEADLINE in DEVCTL 0x08ec26b0 (419467 us)

- Bucket `5f7c363d77236134`, first seen at 218.9 s, 1 hit(s)
- Errno: 0
- Worst reply latency: 419467 us (deadline 100000 us)
- Reproduce: `resfuzz-agent --target /dev/ptyp0 --spawn "/data/resfuzz/devc-pty -n 1 -o nodaemon" --replay deadlines/deadline_5f7c363d77236134_0.rfz`

Test case (8 ops):

```
#0   slot0 DEVCTL     a=0x08ec26b0 b=0x00000027 len=43 data=198abbf40bd823f2a7f7a8849b9d82d659c9767094c197cca5f88c643a39fb0c...
#1   slot0 DEVCTL     a=0x08ec26b0 b=0x00000027 len=39 data=198abbf40bd823f2a7f7a8849b9d82d659c9767094c197cca5f88cfa3a39fb0c...
#2   slot0 DEVCTL_RAW a=0x40047473 b=0x00002000 len=7 data=1bdede7ee37ee3
#3   slot0 DEVCTL_RAW a=0x80040308 b=0xffffffff len=4 data=00000000
#4   slot0 DEVCTL     a=0x4004667f b=0x00000030 len=40 data=000000000000000000000000000000ffff0064000000000000000000000000e0...
#5   slot0 DEVCTL_RAW a=0x80040326 b=0x00000004 len=4 data=00100000
#6   slot0 DEVCTL_RAW a=0x00000200 b=0x00000004 len=4 data=4a01940a
#7   slot0 DEVCTL_RAW a=0x056eff05 b=0x000001ff len=0
```

### 12. DEADLINE in DEVCTL DCMD_CHR_TCSETSID (452715 us)

- Bucket `6ac8bdb3af05cdab`, first seen at 174.5 s, 1 hit(s)
- Errno: 0
- Worst reply latency: 452715 us (deadline 100000 us)
- Reproduce: `resfuzz-agent --target /dev/ptyp0 --spawn "/data/resfuzz/devc-pty -n 1 -o nodaemon" --replay deadlines/deadline_6ac8bdb3af05cdab_0.rfz`

Test case (12 ops):

```
#0   slot0 DEVCTL     a=0x80040308 b=0x00000030 len=2 data=dbda
#1   slot0 DEVCTL     a=0x80040316 b=0x00000004 len=4 data=774bc8a1
#2   slot0 DEVCTL_RAW a=0x00000100 b=0x00000000 len=0
#3   slot0 DEVCTL_RAW a=0x00000100 b=0x00000033 len=0
#4   slot0 DEVCTL_RAW a=0x00000400 b=0x00000100 len=0
#5   slot0 DEVCTL     a=0xfdff77ff b=0x00000035 len=0
#6   slot0 DEVCTL     a=0x00010800 b=0x00000007 len=0
#7   slot0 DEVCTL_RAW a=0x02008323 b=0x00000000 len=0
#8   slot0 DEVCTL_RAW a=0x80040308 b=0x00000006 len=12 data=f0e0edffffff080000000000
#9   slot0 DEVCTL_RAW a=0x00000318 b=0x000001ff len=0
#10  slot0 DEVCTL_RAW a=0x0000031f b=0x00000000 len=0
#11  slot0 DEVCTL_RAW a=0xa004030f b=0x00000008 len=4 data=ce080000
```

### 13. DEADLINE in DEVCTL_RAW DCMD_CHR_RESET (555240 us)

- Bucket `6e6fac000c9a443d`, first seen at 170.5 s, 1 hit(s)
- Errno: 0
- Worst reply latency: 555240 us (deadline 100000 us)
- Reproduce: `resfuzz-agent --target /dev/ptyp0 --spawn "/data/resfuzz/devc-pty -n 1 -o nodaemon" --replay deadlines/deadline_6e6fac000c9a443d_0.rfz`

Test case (11 ops):

```
#0   slot0 DEVCTL_RAW a=0x80040362 b=0x00000400 len=4 data=04030000
#1   slot0 DEVCTL_RAW a=0xfbbf800d b=0xfffffff0 len=0
#2   slot0 READ       a=0x0000000f b=0x00000000 len=0
#3   slot0 DEVCTL_RAW a=0x0000031f b=0x00000000 len=0
#4   slot0 DEVCTL_RAW a=0x80047476 b=0x00000004 len=8 data=0000000040000000
#5   slot0 READ       a=0x00002000 b=0x00000000 len=0
#6   slot0 READ       a=0x00002000 b=0x0000ffff len=0
#7   slot0 DEVCTL_RAW a=0x00047410 b=0x00000004 len=4 data=00cf0000
#8   slot0 DEVCTL_RAW a=0x00047410 b=0x00000004 len=4 data=00cf0000
#9   slot0 DEVCTL_RAW a=0x00047410 b=0x00000004 len=4 data=00cf0000
#10  slot0 DEVCTL     a=0x00100007 b=0x00000004 len=22 data=00800041413c41413f3441a3fc4041c1a30019d12253
```

### 14. DEADLINE in DEVCTL_RAW DCMD_CHR_GRANTPT (201189 us)

- Bucket `81e4011651e0d2a8`, first seen at 25.3 s, 1 hit(s)
- Errno: 0
- Worst reply latency: 201189 us (deadline 100000 us)
- Reproduce: `resfuzz-agent --target /dev/ptyp0 --spawn "/data/resfuzz/devc-pty -n 1 -o nodaemon" --replay deadlines/deadline_81e4011651e0d2a8_0.rfz`

Test case (10 ops):

```
#0   slot0 DEVCTL_RAW a=0x40440331 b=0x00000028 len=4 data=11200002
#1   slot0 DEVCTL_RAW a=0x40040325 b=0x00008000 len=25 data=0000000f0f0022cdaf58fbcdcdcd011bf4f2651b67281bc900
#2   slot0 DEVCTL_RAW a=0x80040326 b=0x00000004 len=4 data=0000000f
#3   slot0 DEVCTL     a=0x5490976d b=0x0000f0f0 len=9 data=0080000023db16799c
#4   slot0 DEVCTL_RAW a=0x80040317 b=0x00000004 len=8 data=ffffff000800007f
#5   slot0 WRITE      a=0x00000000 b=0x00000000 len=45 data=dee29231d8c04b83c0b5d030ab87df9efe04e4acb06bfcdc54a44b7c61748cd5...
#6   slot0 DEVCTL_RAW a=0x0000032a b=0x00000000 len=0
#7   slot0 DEVCTL_RAW a=0x40010363 b=0x00000002 len=6 data=606000200000
#8   slot0 DEVCTL_RAW a=0x40010363 b=0x00000002 len=6 data=606000200000
#9   slot0 DEVCTL_RAW a=0x00000f0f b=0x00000000 len=0
```

### 15. DEADLINE in DEVCTL_RAW DCMD_CHR_TCSETATTR (165033 us)

- Bucket `a205ea4bce7ecdd7`, first seen at 6.4 s, 1 hit(s)
- Errno: 0
- Worst reply latency: 165033 us (deadline 100000 us)
- Reproduce: `resfuzz-agent --target /dev/ptyp0 --spawn "/data/resfuzz/devc-pty -n 1 -o nodaemon" --replay deadlines/deadline_a205ea4bce7ecdd7_0.rfz`

Test case (5 ops):

```
#0   slot0 DEVCTL_RAW a=0x0000032a b=0x00000000 len=0
#1   slot0 DEVCTL_RAW a=0x80040324 b=0x00000004 len=4 data=2dda8b61
#2   slot3 WRITE      a=0x00000000 b=0x80000000 len=69 data=11d4931bd46fb57c4eb9af44ee8a0cfa94c6f8b1510db2001260ff29dadd19a3...
#3   slot0 DEVCTL     a=0x80040326 b=0x00000004 len=16 data=200000ff0004200000ff000400000000
#4   slot0 DEVCTL_RAW a=0x804c7414 b=0x0000001c len=4 data=00800000
```

### 16. DEADLINE in DEVCTL_RAW DCMD_CHR_FORCE_RTS (1235632 us)

- Bucket `a4f5c45b0a786904`, first seen at 189.9 s, 1 hit(s)
- Errno: 0
- Worst reply latency: 1235632 us (deadline 100000 us)
- Reproduce: `resfuzz-agent --target /dev/ptyp0 --spawn "/data/resfuzz/devc-pty -n 1 -o nodaemon" --replay deadlines/deadline_a4f5c45b0a786904_0.rfz`

Test case (16 ops):

```
#0   slot0 DEVCTL_RAW a=0x80040322 b=0x00000004 len=4 data=0010ff00
#1   slot0 DEVCTL_RAW a=0x80087467 b=0x00000012 len=0
#2   slot0 DEVCTL_RAW a=0x80040316 b=0x00000004 len=4 data=00000018
#3   slot0 LSEEK      a=0x7fffffff b=0x00000000 len=0
#4   slot0 DEVCTL     a=0x00001880 b=0x0000001f len=0
#5   slot0 DEVCTL     a=0x4204746a b=0x00000004 len=9 data=0002000000400000ff
#6   slot0 DEVCTL_RAW a=0x40010354 b=0x00000002 len=3 data=000200
#7   slot0 DEVCTL_RAW a=0x00000080 b=0x0000f0f0 len=13 data=00040a00f0f000000040000000
#8   slot0 DEVCTL_RAW a=0x40087468 b=0x00000008 len=8 data=0000000000000000
#9   slot0 DEVCTL_RAW a=0x00000c00 b=0x00000003 len=0
#10  slot0 DEVCTL     a=0x277596e7 b=0x00000011 len=36 data=2b9d2d9c2de32516de0010000024609d0f2f00002516de2460dcdc9f63aa9ff0...
#11  slot0 DEVCTL     a=0xffff7fff b=0x00000000 len=0
#12  slot0 DEVCTL     a=0x00008000 b=0x00000000 len=0
#13  slot0 DEVCTL_RAW a=0x0200031a b=0x00000009 len=0
#14  slot0 DEVCTL     a=0x0200032a b=0x00000000 len=0
#15  slot0 DEVCTL_RAW a=0x010058c0 b=0x000000ff len=0
```

### 17. DEADLINE in DEVCTL_RAW DCMD_CHR_SET_LOGGING_DIR (151459 us)

- Bucket `cedd302cc32ac559`, first seen at 89.3 s, 1 hit(s)
- Errno: 0
- Worst reply latency: 151459 us (deadline 100000 us)
- Reproduce: `resfuzz-agent --target /dev/ptyp0 --spawn "/data/resfuzz/devc-pty -n 1 -o nodaemon" --replay deadlines/deadline_cedd302cc32ac559_0.rfz`

Test case (8 ops):

```
#0   slot0 DEVCTL_RAW a=0x00000f0f b=0x00000080 len=0
#1   slot0 DEVCTL_RAW a=0x00000f0f b=0x00000010 len=0
#2   slot0 DEVCTL     a=0x40040327 b=0x00000003 len=4 data=6c18bf33
#3   slot0 DEVCTL_RAW a=0x80080323 b=0x00010000 len=0
#4   slot0 SLEEP      a=0x00000482 b=0x00000000 len=0
#5   slot0 DEVCTL_RAW a=0x8004746a b=0x00000036 len=8 data=2e0010000060cd89
#6   slot0 DEVCTL_RAW a=0x40040328 b=0x0000004c len=76 data=0000000000000000000000000000000000000000000000000000000000000000...
#7   slot0 DEVCTL     a=0x9cee142d b=0x00000022 len=34 data=25dd7f432bec236609c21039c233bd71ac27eff9ffb8846e278f0eebef28088f...
```

### 18. DEADLINE in DEVCTL_RAW DCMD_CHR_IDLE (148831 us)

- Bucket `dd4d45095794614e`, first seen at 43.6 s, 1 hit(s)
- Errno: 0
- Worst reply latency: 148831 us (deadline 100000 us)
- Reproduce: `resfuzz-agent --target /dev/ptyp0 --spawn "/data/resfuzz/devc-pty -n 1 -o nodaemon" --replay deadlines/deadline_dd4d45095794614e_0.rfz`

Test case (6 ops):

```
#0   slot0 DEVCTL_RAW a=0x00000320 b=0x00000004 len=5 data=0000228000
#1   slot0 OPEN       a=0x00000001 b=0x00000000 len=0
#2   slot0 OPEN       a=0x00000001 b=0x00000000 len=0
#3   slot2 DEVCTL     a=0x0000032a b=0x00000000 len=0
#4   slot0 DEVCTL     a=0x19ec34dd b=0x00000029 len=68 data=5ed55330f28700f4b26e896c20c29df1cb1dcd000800009430bda67bfc48af1d...
#5   slot0 DEVCTL     a=0x40047477 b=0x00000004 len=4 data=00000000
```

### 19. DEADLINE in DEVCTL DCMD_CHR_SET_DISCARD_OUTPUT (237425 us)

- Bucket `e3f2c52d4f1ddc62`, first seen at 256.4 s, 1 hit(s)
- Errno: 0
- Worst reply latency: 237425 us (deadline 100000 us)
- Reproduce: `resfuzz-agent --target /dev/ptyp0 --spawn "/data/resfuzz/devc-pty -n 1 -o nodaemon" --replay deadlines/deadline_e3f2c52d4f1ddc62_0.rfz`

Test case (5 ops):

```
#0   slot0 DEVCTL     a=0x644ebb21 b=0x00000022 len=40 data=561f17994eeff7625f4d7651f912768a6c8c1e3dee225f4d7651f912ec456ba9...
#1   slot0 DEVCTL_RAW a=0x804c7418 b=0x00000074 len=76 data=45451e4506ec4a00000000000000000000000000000000000000000000000000...
#2   slot0 DEVCTL_RAW a=0x804c7415 b=0x00002000 len=1 data=92
#3   slot0 DEVCTL     a=0x80040329 b=0x00001000 len=8 data=82004000006e96cf
#4   slot0 DEVCTL_RAW a=0x0000ffff b=0x00000000 len=7 data=c8c85d7ac8c8f8
```

### 20. DEADLINE in DEVCTL_RAW 0x6224030a (319392 us)

- Bucket `e9e45a66767b7f32`, first seen at 257.9 s, 1 hit(s)
- Errno: 0
- Worst reply latency: 319392 us (deadline 100000 us)
- Reproduce: `resfuzz-agent --target /dev/ptyp0 --spawn "/data/resfuzz/devc-pty -n 1 -o nodaemon" --replay deadlines/deadline_e9e45a66767b7f32_0.rfz`

Test case (8 ops):

```
#0   slot0 DEVCTL_RAW a=0x6224030a b=0x00000053 len=55 data=0001000000f000040065730045c99f3452525252521f004000005241ab528e09...
#1   slot0 DEVCTL_RAW a=0x40040328 b=0x00000000 len=0
#2   slot0 DEVCTL     a=0x80040316 b=0x0000001e len=4 data=00042020
#3   slot0 DEVCTL_RAW a=0x0000031f b=0x00000001 len=1 data=eb
#4   slot0 DEVCTL     a=0x80045406 b=0x00000200 len=5 data=eb00200000
#5   slot2 WRITE      a=0x00000000 b=0x00000000 len=39 data=635ce55e0605f6af434939f8fdab6d51e89bc60306969268014c55c09c36abb3...
#6   slot0 DEVCTL_RAW a=0x98ff21ae b=0x00000008 len=33 data=2fe07cf60b0b160f46171741171c0d1717170e17f4ba741717c0175017c61717...
#7   slot0 DEVCTL_RAW a=0x98ff21ae b=0x00000008 len=4 data=ff7fffff
```
