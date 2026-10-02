# .NGA animation files / archivos de animación (`BIKE/BANIM`, `R/RANIM`, `SHELL/SH*ANIM`)
Parser + evaluator: `tools/nga.py` · Tests: `tests/test_nga.py` (synthetic tracks + invariants over all 6 files when the game data is present). Citations are Ghidra addresses; uncited = hypothesis.

## File header and clip table (`FUN_0020CEE0`)
`u16 @0` (unknown: 6040 BANIM, 20104 RANIM) · `u16 @2 = count` · then `count × { u16 id, u16 pad, u32 offset }` from +4. `offset` points to the clip **header**; the loader does `item[id].clip = base + offset` (`*(id*0x3C + node[0x10] + 4)`) and `FUN_0020BE90` stamps the id into the clip at `+0x10`. Ids are **not consecutive** (BANIM ends …196, 197, 202; RANIM …204, 210, 211) and are global across files (SH?BANIM 199–201, SH?RANIM 206–208). BANIM 198 clips, RANIM 206, the four SH* files 4 each (earlier docs said 197/204: the old parser assumed consecutive ids).

## Clip header (at `offset`)
| off | type | meaning |
|----:|------|---------|
| 0 | u16 | 12 |
| 2 | u16 | clip id (game table; unique per file) |
| 4 | u32 | 0 (4 RANIM clips: an f32, unknown) |
| 8 | f32 | duration (24, 40, 60, 120, 150 …) — time unit unverified |
| 0xC | u16 | number of tracks `K` (`lhu 0xC` in `FUN_0020A258`) |
| 0x10 | u16 | runtime slot (file: 0) |
| 0x12 | u16 × K | `counts[k]`: **distance in 4-byte units** to track *k* (not a key count — earlier doc was wrong) |

## Track placement (`FUN_0020A258`, asm `0x0020A308–0x0020A36C`)
Tracks lie **before** the header, contiguously: `pos₀ = header`, `pos_k = pos_{k−1} − 4·counts[k]`, track *k* starts at `pos_k`. Verified on all 25 348 tracks: regions fill the file exactly (gap to the previous clip header 0/2/8/16 = alignment). `counts[k]·4` = `pad4(record size)` **plus 16 when track k−1 (in processing order) carries a quantisation header**, which sits 16 bytes before that track's record and therefore inside region *k*; region 0 may also contain ≤ 12 B of padding up to the header.
For each track the engine reads `u16 flags`, `u16 channel` (pose slot: `pose[channel] = value(t)`, mask bit set), then calls `FUN_0020C5E8`, which dispatches through the table `0x4FAFB8` filled by `FUN_0020CDD0`: index `(flags&7)·12 + ((flags>>3)&7)·4`. Types 2 and 3 additionally advance the per-track hint pointer (`(flags&7)−2 < 2`).

## Track record and evaluators
`u16 flags (type = &7, mode = >>3 &7) · u16 channel · u16 n · data`. Quantised tracks use the header `{f32 t0, dt, v0, dv}` at `record−0x10` (value = `q·dv+v0`, time = `q·dt+t0`). Tangent word (16 bit): `|x| < 16384 → x/16384`; top bits `01 → 16384/(32768−x)`; `10 → 16384/(−32768−x)`.

| (type,mode) | evaluator | record layout | tracks in the 6 files |
|-------------|-----------|---------------|----------------------:|
| (3,0) | stub `0x0020C5E0` (`lwc1 f0,4(a0)`) | `f32 value @+4` (the u16 `n` is its high half) | 12 374 |
| (2,2) | `FUN_00268968` | `u8 samples @+6`, uniform step `dt` from `t0`, linear interpolation, clamped | 5 626 |
| (4,2) | `FUN_00268A68` | `n × {u8 t, u8 v, u16 tan} @+6` (one tangent per key), Hermite with slope `tan` | 7 035 |
| (1,2) | `FUN_002691A8` | `n × {u8 t, u8 v, u16 tanA, u16 tanB} @+6`, Hermite in `u` with `m0=tanA_k`, `m1=tanB_k` | 313 |
| (1,1) | `FUN_00268EF8` | as (1,2) with `u16 t, u16 v` (stride 8) | 0 (not in data) |
| (1,0) | `FUN_00268DE8` | `n × {f32 t, v, tanA, tanB} @+8` | 0 |
| (0,0) | `FUN_00268D10` | `n × {f32 t, v, c3, c2, c1} @+8`, `v + x(c1 + x(c2 + x·c3))`, `x=t−t_k` | 0 |
| (2,0) / (2,1) | `FUN_00269438` / `FUN_002694E8` | f32 / u16 uniform samples | 0 |
| (4,0) | `FUN_00269600` | `n × {f32 t, v, tan} @+8` | 0 |
| (4,1) | `FUN_00269740` | not implemented | 0 |
Evidence: **25 348 tracks decode with no unknown (type,mode), all key times monotonic, all values finite, record sizes exact** (`tests/test_nga.py::RealData`). Combinations absent from the data are implemented from the decompiled code only (untested).

## Engine side
Animated nodes are looked up by `(kind, index)` in `DAT_004FAE48[scene]` (`FUN_00208F68`, `FUN_00208E60`, `FUN_00208D28`); a node's `+0x10` is an array of 0x3C-byte items, `+0x20` the channel/mask info (`u16 @+2` = channel count). Playback instances (0x58 B, types 0x21 clip / 0x22 blend) are built by `FUN_002085D0/00208978/00208B78`; `FUN_0020A4A0` dispatches: type 0x21 → `FUN_0020A258` (clip → pose floats + mask), type 0x22 → blenders `FUN_0020A6F8…` (cross-fade `1−(t/dur)` with ease modes `DAT_002C7B8C`, per-channel angle wrap when the channel's angle bit is set).

## Open
* Channel → joint mapping **solved for the rider rig** (40 Euler channels, `rider.md`; BANIM's 27 channels = pelvis…arms). Still open for the bike rig (BIKESKEL has no type-35 joints; whether BANIM channels also move bike parts is unknown).
* Time unit of the keys (duration 24–150): `FUN_00209F30` converts instance time to track time (not decoded).
* The 4 RANIM clips with a non-zero f32 at `+4`.
* Rider body mesh: found in the level NGPs (type-25 nodes, kinds 4030-4130), see `rider.md`. Joint hierarchy and channel → joint still open.
