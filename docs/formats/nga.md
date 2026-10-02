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

Rider: R/ meshes (e.g. `CAM`…`XSW`, 4 outfits' arms + handlebar) are first-person arms; no full-body rider mesh found (searched SHELL, REP (`.ORB/.REP` are replay data), RST (`.RST/.RRS` 16 KB fixed tables, not meshes), BIKE).
