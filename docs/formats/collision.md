# Level collision mesh (NGP node type 42 / 0x2A) — decoded from the engine
Parser: `tools/collision.py` · Runtime: `src/ground.hpp` (`Ground::query`) · Tests: `tests/test_collision.py`, `tests/ground_test.cpp` (synthetic) · Real-data check: `tests/ground_pts_check.cpp`.
Rule for this doc: every statement cites the ELF function (Ghidra address); anything uncited is marked **hypothesis**.

## How the bike finds ground (call chain)
1. `FUN_001340d8` (body step) — per sub-step sweeps the body's contact points (16-B vectors from `body+0x150`, count `body+0x12c`) from the previous to the new position, with a radius, into `FUN_0021a908`; then `FUN_00217450` picks **the hit with the smallest fraction `hit+0x1c`** (tie: lower address). Hits are 0x30 B. If the earliest hit has fraction < 0 → `FUN_001344f0`; otherwise `FUN_001344d0 → FUN_00134630` applies the contact response.
2. `FUN_0021a908` — builds the swept AABB (min/max of start/end ± radius), `FUN_00217e00` converts it to a cell range of the current grid (`DAT_002c7c48`: `+4,+6` i16 width/height, `+8` cell size, `+0xC/+0x10` x0/z0 — same header as NGP root node 15), `FUN_00218170/00217f48` iterate the cells' objects, and for each candidate calls `FUN_00216780` (or `FUN_00216a10` with extra params).
3. `FUN_00216780` dispatches on the candidate node type: **0x0A** → `FUN_0021a4d8` (single primitive; no node 10 occurs in the walked ALP2 tree: unknown shape), **0x2A** → loop over its parts calling `FUN_00219970` (swept sphere vs triangle part). After hits: `hit.u16[3] = surfaceTable[triMaterial]` (`puVar4[3] = *(u16*)(mat*2 + node[2])`).
4. `FUN_00134630` (response) reads the contact: point `+0x10`, normal `+0x20`, and **surface class = `(u64 word0 >> 0x35) & 0x3F`** = `(surface_u16 >> 5) & 0x3F`, passed to the effects function `FUN_001b9a50` (dust/sound). Impulse maths in `FUN_00238648` / `FUN_002384b8` (restitution `param_1+1.0`, tangential friction by `body+0x118`, ×1.25 when mode 2).

## Node 0x2A layout (`FUN_00216780`, branch 0x2A)
| off | type | meaning |
|----:|------|---------|
| 0 | u32 | `0x2A` (exact; the render walker `FUN_0020edb8` case 0x2A treats it as a callback hook, it never draws it) |
| 4 | u16 | number of parts `n` |
| 6 | u16 | unknown (4 in ALP2 example) |
| 8 | ptr | surface table: `u16[material]` |
| 0xC | ptr × n | parts |
Part layout (`FUN_00219970`): `+0 ptr` vertex array (f32 ×3, **u8 indices → ≤ 256 vertices**, array sits just before the part header), `+4 u16` triangle count, `+6 u16` BVH node count (0 → test all), `+8 f32×3` origin, `+0x14 f32` quantisation scale (`(p − origin)·scale` → i16), `+0x18` BVH nodes × 14 B (`i16[6]` min/max box + `u16`: bit0 = leaf, bits1-4 = triangle count, bits5-13 = first triangle or child index), then triangles × 4 B `(v0, v1, v2, material)`. **Verified on ALP2**: parts are contiguous (`part end` → next vertex array → next part …) and the surface table starts exactly where the last part ends.

## Data findings (ALP2, `tools/collision.py`)
- 777 type-0x2A nodes → **111 738 triangles**, extent x[−3152,4713] y[−2149,7558] z[−11563,6621]. Only 18 nodes hang from the tree `scene.walk` covers; the rest are referenced from 0x4C-byte records (e.g. file 0xAC4C68…) — **hypothesis**: object-instance records that the engine registers in the grid; the scan (`find_node42`) uses exact header 0x2A + pointer validation instead.
- Surface u16 values: 39 distinct in ALP2 (e.g. 0x1800 ×38 984, 0x0001 ×15 198, 0x1003 ×14 909, 0x0880 ×10 673). Only bits 5-10 are proven to be the effect class (`FUN_00134630`); other bits (flags?) **hypothesis/unknown**. No names for classes (dirt, rock, water…) yet.
- The NGP root node 45 (46 512 i16 points + RGB555-like word, `FUN_00226b08`) has the **same extents** as this mesh (x[−3152,4712] z[−11562,6615]): **hypothesis** = baked per-vertex lighting of the collision mesh sampled by nearest point.

## Game criterion reproduced in `Ground::query(x,y,z,margin=30)`
The game sweeps a segment and takes the earliest hit; a static query has no history, so we sweep a vertical segment from `y+margin` downward: the **highest triangle at or below `y+margin`** wins (earliest time of impact). Normal is oriented to +Y (winding is mixed). `margin` 30 u (3 m) is a calibration knob chosen from the PTS test below, not an engine constant.

## Validation (ALP2, `.PTS` chain from record 0, 611 points; the line is a guide ≈ ±12 u off the ground, not exact ground truth)
- 0 points without ground; **|h − y|: mean 11.4 u, p95 27.8 u, max 278 u** (max = places where the line passes under/over another sheet).
- Python nearest-sheet error (any sheet): mean abs 9.7 u, max 97 u. 377 of 611 points have > 1 sheet under them (so selection matters).
- Visual: `DH_COL=out/ALP2.col DH_PTS=out/chain0.pts dhview` draws the mesh in green over the course; it follows the corridor of the racing line and the visual terrain.
- Dynamic confirmation (PCSX2 debugger) **not done**: PCSX2 is not installed in this environment.
