# NGP root nodes 15 and 45 / Nodos raíz 15 y 45 del .NGP
Parser: `tools/ngpgrid.py` · Test: `tests/test_ngpgrid.py` (synthetic). Pointers are load addresses (file offset = ptr − 0xA00000).
Derived from the decompiled consumers `FUN_00216ca8` (grid) and `FUN_00226b08` (probes); the scene walker only stores the pointers (`FUN_00216c88` → gp-0x5528, `FUN_00226af8` → gp-0x5490).

## Type 15 — object grid (broadphase), **not** collision
| off | type | meaning |
|----:|------|---------|
| 0 | u32 | 15 |
| 4 | u16 ×2 | width, height in cells (ALP2: 117 × 145) |
| 8 | f32 | cell size (100 u) |
| 0xC / 0x10 | f32 | x0 / z0 origin (ALP2 −4198 / −3255) |
| 0x14 / 0x18 | f32 | x1 / z1 (ALP2 7487 / 11199) — x1−x0 ≈ w·cell (unconfirmed role) |
| 0x1C | ptr | runtime cell table (relocated; lists of dynamic objects are linked here) |
| 0x30 | u32 × w·h | per cell `(start<<10) | count`; `start` cumulative (sum of previous counts), **verified**: ALP2 total 75 146 entries, node size 0x30+4·w·h = distance to the next root. Likely a pre-sized pool for the runtime linked lists. |
Callers of the radius query `FUN_002170f8` (`FUN_001579d8`, `FUN_001809b8`, `FUN_00145c70`, `FUN_001828f8`, `FUN_00124778`, `FUN_0012e8c0`, `FUN_00155ab0`) pass **kind ranges** (0x4B0–0x1193, 0x51E–0x528, 0x4EC–0x5B4…) so the grid is a per-cell **object registry filtered by kind** (`hdr>>18`), and the physics body re-registers itself via `FUN_00216fe8` → `FUN_00216ca8`. The same grid header is what the collision sweep uses to pick cells (`FUN_00217e00`, see collision.md). The game uses it to register dynamic objects per cell and query by radius (`FUN_00216ca8` / `FUN_002170f8`, cell = ⌊(pos−origin)/cell⌋).

## Type 45 — baked light probes (nearest-point ambient colour)
`u32 45 · u32 count · f32×3 origin(0) · f32 scale(1.0) · count × 8 B` from +0x18: `i16 x,y,z · u16 c`, `c`: R = (c>>1)&31, G = (c>>6)&31, B = c>>11, each × 1/16 (1.0 = 16, up to 1.94). Sorted by the third component (binary search) then expanded to the nearest neighbour (`FUN_00226b08`). ALP2: 46 512 probes up to end of file.
Unknown: which axis is "up" and the exact relation to world coordinates (probe ranges x[−3152,4712] y[−2148,7558] z[−11562,6615] do not match the terrain extents directly; probably another local space/axis order).

**Consequence:** the collision *triangles* are not in these nodes — they are the type-0x2A nodes, see `collision.md`. The probe extents equal the collision mesh extents (**hypothesis**: per-vertex baked light of that mesh).
