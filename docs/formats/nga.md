# .NGA animation container / contenedor de animaciones (`BIKE/BANIM`, `R/RANIM`, `SHELL/SH*ANIM`)
Parser: `tools/nga.py` · Test: `tests/test_nga.py` (synthetic). Checked on all 6 files of the ISO (197 + 204 + 4 + 4 + 4 + 4 clips).

## Clip table
`u32 magic` (≠ constant: 0xC61798 BANIM, 0xCE4E88 RANIM; probably a load address/base, unconfirmed) then `(u32 id, u32 offset)` pairs: ids are consecutive and **global across files** (BANIM 1–197, SH?BANIM 198–201, RANIM 1–204, SH?RANIM 205–208). Offsets ascending; a clip runs to the next offset (last: EOF). The table is followed by padding to the first clip (BANIM 0x700, RANIM 0xAD0).

## Clip header (0x12 bytes + counts)
| off | type | meaning |
|----:|------|---------|
| 0 | u16 | 12 always (header size / version) |
| 2 | u16 | clip id in game tables (12, 24, 28, 76, 80, 84… 988; unique per file) — mapping to names unknown |
| 4 | u32 | 0 (in 4 RANIM clips an f32, e.g. 10.0 — unknown) |
| 8 | f32 | duration (24, 40, 60, 120, 150 → frames or ticks; unit unconfirmed) |
| 12 | u32 | number of tracks (bike clips 2–27; rider clips up to 109) |
| 0x12 | u16 × tracks | per-track small count (2…11; probably key count or channel descriptor) |
| … | | track payload — **NOT decoded** |
Payload evidence: per track descriptors like `1410 1a00 0300…` and f32 triples (e.g. 0.157, −0.706, 0.0055) interleaved with packed bytes/halves → a custom compressed curve format. The ELF function that reads it (rider anim handle lookup: `FUN_001affa8` → `FUN_00208f68`/`FUN_00208e60`) is the next thing to decompile.

Rider: R/ meshes (e.g. `CAM`…`XSW`, 4 outfits' arms + handlebar) are first-person arms; no full-body rider mesh found (searched SHELL, REP (`.ORB/.REP`: replay files, `.ORB` has an ASCII rider name e.g. "KonradB"), RST (`.RST` per level and `.RRS` per rider: sparse 16 KB parameter tables with magic `0x4E9692A5` and f32 values like 0.55/1.01 — likely rider/level tuning, not meshes; 2358 of 16384 bytes non-zero in ALP2.RST), BIKE (`<RIDER>.RRS`, 32 KB, same family)).

## Engine side (partial, Ghidra) — what is known about how clips are bound
- Animated nodes are looked up by `(kind, index)` = `(hdr>>18, (hdr>>7)&0x7FF)` in the loaded-scene table `DAT_004FAE48[scene]` (`+4` node count, `+8` node pointers): `FUN_00208F68` (find node), `FUN_00208E60` / `FUN_00208D28` (find node **and** the animation item). The node's `+0x10` points to an array of **0x3C-byte animation items**; `FUN_00208CD8` maps an id to an item index by scanning the node's `+0x20` list (u16 count, then `(ptr, …)` pairs whose target header `>>18` equals the id). So clip ids ↔ node kinds.
- `FUN_001AFFA8` (rider setup) calls these for rider handles 1..0xFD and logs `"can't find anim handle for rider - %d"` (`0x2A99F8`) when the first lookup fails.
- Instances: `FUN_002085D0` / `FUN_00208978` / `FUN_00208B78` allocate 0x58-byte animation instances (type 0x22 / 0x21 blend chain, time `DAT_002C8388`, item pointer at `+0x1C`).
- **Not found:** the function that decodes the per-track curve payload (probably sampled in VU code or deeper in the pool functions `FUN_00209xxx`). Decoder stays blocked; `tools/nga.py` only exposes the container.
