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

## Sweep implemented in C++ (`src/ground.hpp`, `Ground::sweep`) — what is faithful and what is not
Port of the narrow phase and the selection rule; **no 30-u constant exists any more** (`groundQuery(x,y,z,r,reach)` takes radius and reach from the caller; dhview passes the mesh depth).

| Step | Engine | Port |
|------|--------|------|
| Selection | `FUN_00217450`: smallest `hit+0x1C`; tie → lower address | smallest `frac`, tie → lower triangle index |
| Swept box | `FUN_0021A908`: `[min(p0,p1)−r, max(p0,p1)+r]`, cells by `FUN_00217E00` | same box; own XZ grid of 100 u instead of the object grid (the engine's cells list *objects* registered at load; static result is identical because every triangle whose box touches the swept box is tested) |
| Triangle prefilter | `FUN_00219970` box test per axis | same |
| Face | `FUN_002193C0`: n=(v2−v1)×(v0−v1) normalised, one-sided (`dist0<0` → no hit); if `dist0−r ≤ 0` overlap at start (contact `p0+n·(0.01−dist)−r·n`, `frac=−FLT_MAX`, pen `0.01−dist`); else `dist1=n·p1−d−r > 0` → no hit (no edge test), `t=dist/(dist−dist1)`, `tb=t−0.01/(dist−dist1)`, inside test `c0·c1>0 ∧ c0·c2>0` | identical |
| Edges / vertices | `FUN_00218FB8` + `FUN_00218E00`: per-axis box reject, initial overlap with the edge segment, cylinder quadratic, then sphere at vertex `v_i`; min fraction of the three edges | identical formulas |
| Node type 0x0A (convex planes) | `FUN_0021A4D8` | **not ported** (no type-10 node reachable in ALP2; hypothesis: only props) |
| `FUN_00216A10` variant (extra params `p[5],p[6]`) and object transforms (`FUN_00218268`/`FUN_00218310`/`FUN_002183F0`: world↔local with matrix at `obj+0x30…`) | | **not ported**: collision vertices are used as world coordinates (identity transform). Consistent with the visual mesh (see validation) but moving/rotated instances would be misplaced |
| Multiple overlapping hits | the engine appends all hits with `frac ≤ 0` to the buffer and depenetrates over them (`FUN_001344F0`) | only the minimum is returned |
| Response `FUN_00134630` | impulse `FUN_00238648/FUN_002384B8`, surface class `(word0>>0x35)&0x3F` | `src/ride.hpp` removes the inward velocity component (inelastic); surface id is returned untouched |
Winding sign: `(v2−v1)×(v0−v1)` is the VU `OPMULA/OPMSUB` pair as decompiled; I could not settle the operand order from the manual, so it was fixed **empirically**: with it the start platform has a +Z (up) mean normal (coordinates.md item 3) and the validation below passes; the opposite sign would make the ground back-facing.

## Validation (ALP2) — reproducible with `ground_validate`, `ground_test`, `ride_demo`
1. **Independent of the .PTS**: `tests/ground_validate.cpp`, vertical sweep (r = 0) from above the mesh over an XZ grid, compared with the nearest sheet of the visual `.mdl` (two-sided): step 100 u → 3 036 samples, **mean 0.03 u, p95 0.04 u, max 17.3 u, 2 samples > 1 u**; step 37 u → 22 180 samples, **mean 0.02 u, p95 0.04 u, max 24.0 u, 6 samples > 1 u** (0.03 %). Outliers sit at high altitude (Z 2400–4600) — the visual mesh has a different sheet there (hypothesis: coarse LOD vs collision); no sample lacks a visual sheet. Collision = visual terrain surface to < 0.1 u in 99.97 % of samples.
2. **Unit tests** (`tests/ground_test.cpp`, synthetic): overlapping sheets and bridges (regression of the previous behaviour: pick the correct sheet from above/between/below), cell borders at x = 99.99/100/100.01, oblique segment (hit at the analytic x = 5), edge hit (centre stops at √0.75 above, normal (−0.5, 0.87, 0)), vertex hit, exact on-edge point with r = 0 (no hit: strict `c0·c1 > 0`), initial overlap (`frac = −FLT_MAX`, pen 0.51), back face ignored.
3. **Secondary, the .PTS line** (a guide ~±12 u off the ground): earlier 611-point test was axis-blind (coordinates.md item 5) and is dropped; the line is used only to drive the demo.
4. **Dynamic (PCSX2)**: not run here; `tools/pcsx2/README.md` has the breakpoint recipe and `tools/pcsx2/compare_trace.py` compares your log with `Ground::sweep`.

## Demo (`ride_demo`, `dhview` ride mode)
Kinematic sphere (r = 3 u, g = 98 u/s², two sweeps per step like `FUN_001340D8`), autopilot toward the line 5 points ahead (a **demo aid**, not engine behaviour: horizontal turn limit 2.5 rad/s on ground, 0.6 in air plus `airPull` toward the target while airborne).
Result on ALP2 (start at line point 3 because the start gate bar — surface 0x681D — is closed in the static mesh and overlaps the start): **28/28 course gates crossed in order and the finish plane (8052) at t = 62.7 s, 3 761 steps, 0 stuck, 4 depenetrations**, same gate times in `dhview` (1.4, 5.0, 6.5, 8.3 … s). Visual-mesh crossings of the sphere centre: 6 steps, **all within 17 u (1.7 m) above the collision surface** (floating decorative sheets; none below the ground). Caveat: the sphere is airborne 89 % of the time (the course has long drops; the frictionless sphere leaves convex edges) so this demonstrates *not tunnelling and not getting stuck*, not realistic riding; unit scale (10 u = 1 m) remains a hypothesis.
