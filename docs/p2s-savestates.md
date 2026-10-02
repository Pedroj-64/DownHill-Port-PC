# PCSX2 savestates as ground truth / savestates de PCSX2 como referencia
Tools: `tools/p2s.py` (reader), `tools/p2s_check.py` (comparison), `tests/test_p2s.py` (synthetic). **Savestates and RAM dumps never go into the repo**: the tools read a path passed on the command line (default location of the Flatpak: `~/.var/app/net.pcsx2.PCSX2/config/PCSX2/sstates/`).

## Container (PCSX2 v2.8.2, `SLES-52202 (73671EFD).NN.p2s`)
A `.p2s` is a ZIP (entries compressed with zstd, method 93; Python ≥ 3.14 `zipfile` reads it, `7z` too): `PCSX2 Savestate Version.id` (36 B), `PCSX2 Internal Structures.dat` (3.8–6.5 MB), **`eeMemory.bin` (33 554 432 B = 32 MB EE RAM; file offset = EE physical address)**, `iopMemory.bin` (2 MB), `eeHwRegs.bin`/`iopHwRegs.bin` (64 KB), `Scratchpad.bin` (16 KB), `vu0Memory.bin` (4 KB), `vu1Memory.bin` (16 KB), `vu0MicroMem.bin`, `vu1MicroMem.bin`, `SPU2.bin` (2.1 MB), `USB.bin`, `PAD.bin`, `GS.bin` (4.2 MB), `Screenshot.png`.
ELF addresses equal RAM offsets (the ELF is loaded at its virtual addresses; Ghidra == PCSX2).

## Findings
* **The two states provided are not ALP2.** State `.01` (rolling, 31 km/h, 00:10) is **ALPINEMX / ALPX2** (Alpine Mountain-Cross): the player's position is 1.0 u from `ALPINEMX.PTS` record 32 and from nothing in `ALP2/ALPINE.PTS` (322 u). `.01.backup` is the pre-race intro cinematic (no riders: `DAT_00329668 = 0`). Collision for the comparison is therefore `ALPINEMX.NGP` (54 257 triangles; `ALPX2` is identical, `cmp` equal).
* **Rider array**: `DAT_00329668` = number of riders (6); rider *i* at `0x2D94C0 + i·0x7BD0` (FUN_001A2738, FUN_0016CF70); `+0x7A5C` = type (1 = player, 3 = AI); `+0x792C` → **scene node of the body** whose `+0x10` is the world position (vec4) and `+0x40/+0x50/+0x60` the 3×3 rotation rows (row-vector convention: `world = loc.x·row0 + loc.y·row1 + loc.z·row2 + pos`, FUN_001348A0).
* **Bike physics module** (the `param_1` of FUN_001340D8/FUN_00134630): `rider + 0x6420` (back-pointer `+4` = rider, found by scanning RAM for pointers to the rider). `+0x12C` = contact-point count (**4**), `+0x150 + 16·i` local contact point, `+0x15C + 16·i` radius (**0.75 u** each), `+0x1D0` frame counter; rigid body at `+0x10`: position `+0x50` (rigid, differs from the node position by the centre-of-mass offset), velocity `+0xC0`, `+0x34`/`+0x38` pointers to the node position / matrix. Contact points of the player: wheels at local `(0, 2.5, −2.6)` and `(0, −1.4, −2.8)`, two body points `(0, 1.5, 0.5)` and `(0, −0.75, 0)`.
* **Scale: 1 u = 1 ft (0.3048 m).** Player rigid-body speed 29.0 u/s = 8.8 m/s = 31.8 km/h against 31 km/h on the HUD (screenshot). The earlier "10 u = 1 m" hypothesis is wrong (demo parameters changed accordingly).
* **Surviving engine hit records**: the physics stack frame still holds the 0x30-byte hit records of the last sub-step (layout confirmed: `u16 1 @+0`, `surface u16 @+6`, `pen f32 @+8`, `point @+0x10`, `frac @+0x1C`, `normal @+0x20`, `d @+0x2C`).

## Comparison (state `.01`, ALPINEMX collision; `python3 tools/p2s_check.py out/ALPINEMX.col state.p2s --sweep-cli build/sweep_cli`)
**A. Contact points vs the mesh** (clearance = distance to the nearest front-facing triangle − radius; the engine leaves a ≈0.01 u skin):

| rider | point | clearance (u) | ground normal | surface |
|-------|-------|--------------:|---------------|---------|
| 0 (player) | 0 front wheel | +0.068 | (0.059, 0.525, 0.849) | 0x1844 |
| 0 | 1 rear wheel | −0.115 | same | 0x1844 |
| 0 | 2 body | +3.162 | — (airborne) | — |
| 0 | 3 body | +2.673 | — | — |
| 5 (AI, moving) | 0–3 | +0.205, +0.602, +0.047, +0.331 | (0.73,−0.40,0.56)… | 0x184B / 0x1844 |
| 1–4 (AI, velocity 0) | all | +6.7 … +18.9 | — | not in contact (their bodies are not simulated by the physics at this instant) |
Both player wheels touch the ground within 0.12 u and the other two points are 2.7–3.2 u above it, exactly the pattern of a bike on two wheels.

**B. Engine hit records vs my triangle** (nearest triangle to the engine's contact point):

| record | surface engine / mine | frac | max\|Δn\| | d engine / mine | point→triangle |
|--------|----------------------|------|----------|----------------|---------------|
| 0x2DD780 (rear wheel) | 0x1844 / 0x1844 | 0.1855 | 4.9e-8 | −4496.900 / −4496.900 | 0.0108 u |
| 0x2DD890 (front wheel) | 0x1844 / 0x1844 | 0.1857 | 4.9e-8 | −4496.900 / −4496.900 | 0.0107 u |
| 0x2DD910 (copy of 0x2DD780) | same | 0.1855 | 4.9e-8 | same | 0.0108 u |
→ **Same triangle (index 597), identical normal to float precision, identical plane constant `d = n·v1`, identical 16-bit surface id.** The normal is stored by the engine as `(v2−v1)×(v0−v1)` normalised with `w = n·v1` (FUN_002193C0), so the **winding sign is now confirmed, not empirical**. The contact point lies 0.0108 u from the triangle = the 0.01 skin (`FUN_002193C0`, FUN_00218E00).

**C. `Ground::sweep` at the player's contact points** (`sweep_cli`, sphere r = 0.75, 2 u down): wheels → `HIT` triangle 597, normal (0.0590, 0.5248, 0.8492), surface 6212 = 0x1844; the two airborne body points → `NOHIT`. Identical triangle/normal/surface to the engine.

## Not verifiable with these two states
* **Fraction and segment**: the segment buffers (start/end/radius) are not left on the stack, and a single frame does not give the sub-step start position; `frac = 0.1855/0.1857` cannot be reproduced. Needed: **two savestates exactly one frame apart** (frame-advance) while rolling.
* Airborne behaviour of a wheel, other surface classes (only 0x1844/0x184B/0x1819/0x1800/0x1859 seen), initial-overlap (`frac < 0`) and edge/vertex hits.
* The engine has no persistent "ground height" field: its ground state lives in the transient hit records above (and `+0x1D0` frame counter), so there is no stored height to compare with `groundQuery`.

## Live capture via PINE (no manual play) — ALPINEMX, slot 1, 30 s
`tools/pcsx2/pine_capture.py` loads the savestate and samples the RAM at every physics step (position of the player's node changes each step); `tools/pcsx2/analyze_capture.py` compares with `ALPINEMX.col`. PINE socket seen from the host: `$XDG_RUNTIME_DIR/.flatpak/net.pcsx2.PCSX2/xdg-run/pcsx2.sock` (inside the Flatpak it is `$XDG_RUNTIME_DIR/pcsx2.sock`). The per-step counters at `physics+0x1D0/+0x1D4` do **not** advance; do not use them for sync.
Result: 728 consecutive steps (14.5 s of game time, 0 torn reads), **1 686 distinct engine hit records near the player's contact points** (stack leftovers at `0x2DD600…`), surfaces seen `0x1, 0x1003, 0x1800, 0x180B, 0x1844, 0x1859, 0x18A4`:

| Check against the mesh | Records | % |
|---|---:|---:|
| surface + normal (<1e-4) + plane constant d + contact point <0.05 u from the triangle: all equal | 1 292 | 77 % |
| surface and normal equal (d and/or point differ) | 1 602 | 95 % |
| surface or normal differ | 84 | 5 % |
Contact-point clearance of the player's points that touch the ground (<1 u, 994 readings): mean −0.03 u, p5 −0.55, p95 +0.67, min −0.74, max +1.0 u (includes bounces after impacts).
**Not explained (hypotheses):** (a) 310 records have a different `w` (plane constant) although surface/normal match — for edge/vertex hits `w` is not the face constant (FUN_002193C0 stores the face `w` only in the face branch); (b) records whose contact point is >0.05 u from my triangle probably belong to collision instances with a transform (FUN_00218268/00218310/002183F0, not ported) or to node type 0x0A; (c) 64 records with another surface are probably stale stack data or such instances. Fraction check: only 1 usable case (|Δfrac| 0.0007), too few to conclude.
Automatically saved intermediate savestates: PINE `MsgSaveState` (slots 2–9) while the game runs from slot 1; the bike stops after ~15 s (slots 6+ show the same position).
