<!-- SPDX-FileCopyrightText: 2026 Pedro Soto -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Data formats (reverse engineering notes, no game data)

🇪🇸 [Versión en español](../es/formats.md)

These notes describe file formats and structures discovered through interoperability research on the PAL release (`SLES_522.02`).
They contain no game data and no copied code.

## `IE\x03\x04` container (almost every .NGP/.PTR/.RTX/.TEX)
20-byte header, then a u32 name length, the ASCII name and raw deflate (`zlib -15`). See `tools/unpack_ie.py`.
The variant with `0x0a` at byte 4 is an uncompressed wrapper that can contain another IE layer; `tools/unpack_ie.py` unwraps it until it reaches the final data (verified on R/TDI.NGP and R/XDI.*).

## GIFtag (GS transfer header)
- It is a 128-bit header: `NLOOP` occupies bits 0-14, `EOP` bit 15, `PRE` bit 46, `PRIM` bits 47-57, `FLG` bits 58-59, and `NREG` bits 60-63.
- `NREG = 0` represents 16 registers; the 4-bit descriptors are read from `REGS`, starting at bit 64.
- In `PACKED`, the payload is `NLOOP * NREG` qwords; in `REGLIST`, `NLOOP * ceil(NREG / 2)` qwords; in `IMAGE`, `NLOOP` qwords.
- `tools/gif.py` validates the header, modes, and declared size without interpreting GS registers or the ADC bit yet.

## `.TEX` / `.RTX` (textures) — decompressed
- `u32 nbins, u32 first` (`first*16` = offset of the first record), then `nbins` pairs `(count, bytes_per_texture)`.
- Chained records: `u32 next` (offset in **32-bit words**, `(next & ~3)*4` bytes; 0 = end), `u32 hash`, `u64 info` with `id = lo & 0xffff` and, in `hi`:
  width `1<<(hi>>8&15)`, height `1<<(hi>>12&15)`, format `hi&0x3f`, mips `hi>>19&15`, bin `hi>>27`.
- Verified: record count = sum of the bin counts (170 in SHELL/BIKESHOP, 290 in LVL/ALP2).
- Each record: 0x80-byte header (a GIF/BITBLTBUF packet template the game fills in at run time) + `w*h*4` bytes of data.
- **Pixels (solved):** `w`,`h` are the dimensions of the PSMCT32 *upload*. The data is a **PSMT8 (8 bpp indexed) texture of `2w x 2h`** already swizzled:
  upload it linearly as CT32 (`gs.upload32`) and read the memory back as T8 (`gs.read8`). The image comes out vertically flipped.
  Swizzle tables are in `tools/gs.py`; the exporter is `tools/tex_export.py` (grayscale, no palette).
- **Palettes:** live in the `.RTX` (record chain; 16x16 = 256-colour CLUT, 8x2 = 16-colour CLUT; RGBA8 entries with alpha 0x80 = opaque).
  The texture<->CLUT link is NOT in the `.TEX`; it is assigned at load time (probably through a material in the `.NGP`).

## `.NGP` + `.PTR` (geometry) — memory image + relocation table
The game loads the decompressed `.NGP` as a memory image linked at base address `0xA00000` and uses the `.PTR` to relocate it.
- `.PTR`: `u32 n`, then `n` u32 offsets inside the NGP that hold an absolute pointer (`value - 0xA00000` = file offset);
  then `u32 nt` + `nt` offsets of texture ids (u16) to remap; then `u32 nc` + `nc` offsets of GS TEX0 words to patch (CBP/TBP).
  Verified on `LVL/ALP2`: every pointer falls in [0xA00020, base+size].
- `.NGP`: `u32 4` + 4 pointers to top-level lists; 0x30-byte records of (count, flags, hash/bbox, mesh pointer).
- Geometry lives in **VIF packets**: `UNPACK V3-32` (`0x68nnXXXX`, `n` vectors, then `n*12` bytes of floats) holds vertices;
  `UNPACK V4-32` (`0x6c..`) etc. hold the rest. `tools/extract_points.py` pulls the V3-32 blocks: ALP2 yields 243,951 vertices that draw the real track.
- Topology, UVs, colours and materials are solved in the sections below. Still open: exact reproduction of the VU1 path (see *VU1 microcode*).

## VIF mesh fragments (verified on LVL/ALP2)
Each fragment is: `UNPACK V3-32 xN @0xB5` (positions) → `STCYCL cl=3 wl=1` → `UNPACK V4-32 x1 @0x238` (GIF tag: NLOOP = vertex count, EOP, NREG=3 with regs ST, RGBAQ, XYZ2)
→ `UNPACK S-8 xM @0x239+3i` (**indices** into the positions, in strip order; M ≤ N: only part of the vertices is used) → `UNPACK V4-8/V4-5 xM` (colour)
→ `UNPACK V2-16 xM` (signed fixed-point ST) → `MSCNT/MSCAL` (runs VU1 microcode).
- Triangle strips have restarts (see the ADC rule below), which `tools/extract_mesh.py` applies. Rings/ribbons (shadows) come out fine, and with the scene-graph filter (below) the whole terrain does too.
- Some fragments are in local space (coords ~±5: props, shadows) and some in world space (terrain, coords ±10⁴).
- Tools: `tools/vif.py` (decoder), `tools/extract_mesh.py` (→ .msh), `tools/preview_msh.py`, and the `dhview` viewer (`DH_CAM="x y z yaw pitch" DH_SHOT=out.bmp` for screenshots).

## Scene graph (first clues)
- `.NGP` header: `u32 4` + 4 pointers to tables: `0x1080` (chains/models), the node table (`0xB66F00`: `u32 7, u32 175` + 175 pointers to 0xC0-byte nodes), and two more.
  Node: `u32 0x7f0c0019`, 5 pointers, mesh pointer, **4x4 transform matrix** (floats, identity in the sample), flags.
- 0x30-byte directory entries: `[1, 0, 0x00010001, f32 ?, 4 f32 (likely a sphere: centre xyz + radius), pointer to draw chain]`.
- The graph walk, instances, materials and the texture<->CLUT link are done: see *Scene graph traversal* and *Textured models* below.

## VU1 microcode (`tools/vudis.py`)
Dump: ELF section `.vutext` at `0x00269d90`, `0xA140` bytes (5160 64-bit instructions = several programs), file offset `0x160d90`. Programs are uploaded with `MPG` packets.
Disassemble with `python3 tools/vudis.py vutext.bin > vu1.asm`. Encoding reference: the public VU opcode tables (PCSX2 `DisVUmicro.h` / `DisVUops.h`, GPL).
- The first 0x400 instructions are data; real programs sit in ~0x400-0xB00 and 0xC00-0x1280. There are 20 `XGKICK` sites = 20 output paths (vertex types).
- The vertex program near `0xC24` reads per-vertex records of 3 qwords (stride 3, matching `STCYCL cl=3 wl=1`), fetches the position with `LQ vf20, 0xB5(vi9)` (`vi9` = vertex index from the S-8 array), transforms it by the matrix in `vf1..vf4` (`MADDAx/y/z`), does the perspective divide (`DIV Q, vf0w, vf25w`), converts to GS fixed point (`FTOI4`) and writes output qwords with `SQ`.
- **Strip restarts (ADC) are computed at run time** from clip flags (`CLIP`, `FSAND/FCAND`) and per-record flag words (`ILWR.w vi7`, `IAND`), then written into the `w` lane of the XYZ2 qword (`ISW.w`). The per-record flags come from small `STMASK` + `S-8 x2` patch packets that follow the index array: a patch at address `A` flags vertices `v = (A - (hdr+3))/3` and `v+1` (`hdr` = address of the `V4-32` header). **Correction (verified by rendering ALP2):** a flagged vertex only *suppresses the kick* (no triangle ends on it); the strip itself continues and the winding parity counts from the start of the batch. Both tools apply `if j in flag: skip`, parity `j & 1`.
- Consequence: faithful geometry needs either (a) a VU1 interpreter + VIF unpack + GIF parser (high-level emulation of the PS2 vector path) or (b) hand-reimplementing each of the ~20 programs. Current status: **not needed so far** — the static-geometry rules above reproduce the terrain without a VU1 interpreter. It may still be required for animated objects and the bike. See [`architecture.md`](architecture.md) for where the runtime implementation will live.
- Matrices (`vf1..vf4`), the camera and per-draw constants are written by the game's CPU code before the chain runs; they are not in the `.NGP`.
- The ELF has no direct references to the DMA/VIF1 registers (sending goes through library routines), so the draw traversal must be found through data structures (node `0x7f0c0019...`), not registers.

## Scene graph traversal (verified from the game's own walker; `tools/scene.py`)
The `.NGP` header is `u32 n` followed by `n` pointers to roots (4 in ALP2). The first dword of every node: bits 0-5 = **node type**, bits 18+ = **payload kind** (dispatched by a callback).
- Roots of type 7 are registration tables (the 175-entry table at `0xB66F00`); the others are walked.
- Node types and child layout: **1** group (`u16` child count at `+8`, child pointer array at `+0x20`; a `u16` mask at `+10` hides the node); **2** selector/LOD (`u32` count at `+4`, 8-byte entries at `+0x28`, first field = pointer); **3** translate (`f32 x,y,z` at `+0x10`, count `u32` at `+8`, children at `+0x1c`); **4** matrix (matrix at `+0x10`, translation at `+0x40`, count at `+8`, children at `+0x50`); **6** list (count byte at `+0xb`, children at `+0xc`); **8** link (pointer at `+4`); the rest are leaves. Traversal keeps a transform stack (up to 150 entries).
- Payload kinds `1` and `2` are static meshes; kinds above `0x3E8` are game objects (triggers, props, etc.).
- Mesh group nodes have 0x30-byte leaf entries: `[hdr=1, 0, flags, hash, f32 centre xyz, f32 radius, ..., pointer to a VIF chain at +0x20]`.
  In `LVL/ALP2`, nine large-sphere leaves (radius 3000-13000) point to coarse far-LOD chains from `0x1A7F90` on; a tenth subtree holds the fine detail near the start of the file. Rendering everything at once therefore stacks several LODs.
- **Batches:** one `V3-32` position block is reused by several draw batches (each = `V4-32` header + `S-8` indices + colour + ST + `MSCNT`). Reading only the first batch loses most triangles; `tools/extract_mesh.py` now extracts all batches (13,069 batches / 274 k triangles in ALP2) and the terrain surface comes out coherent.
- **Applied and verified (`tools/scene.py`: `instances`, `Owners`):** node 3/4 matrices are orthonormal rotation + translation (row 3), but only 70 of 184 leaves have one and they cover 7 % of the geometry. The real cause of the "soup" was drawing everything at once: the root subtree with payload **1** (`0xEE0` in ALP2) is the level's **fine detail** (each cell is a type-2 node with `d2` = max squared draw distance, e.g. 25e6 = 5000 u); payload **2** (`0x50`) is a tiny far LOD (928 vertices); the rest are game objects. Each `V3-32` block belongs to the leaf with the greatest chain start <= its offset. Fine detail only (208 k vertices, 227 k triangles) gives a recognisable course.
- **ADC fixed:** the ADC bit (S-8 x2 patch on the UV slot) marks vertices that **do not kick** a triangle, but the strip continues and winding parity counts from the batch start; treating it as a restart lost the first triangle of every strip (227 k -> 273 k triangles in ALP2) and left holes.
- **Level materials (see also the TEX record formats below):** a block without its own `TEX0` inherits the previous block's material (persistent GS state); this textures 100 % of vertices (was 13 % untextured). PSMT4 is now decoded (`tools/gs.py: read4`). `extract_model.py` uses the same filter/transform for `LVL/*`.
- **Per-vertex colour and alpha (verified in ALP2):** `V4-8` = RGBA8 and `V4-5` = RGBA 5551 (channel <<3); 0x80 = 1.0 (PS2 modulation). They hold the **baked lighting** and the **alpha** of overlay layers (shadows: black texture with vertex alpha ~36-45/128). The `.mdl` becomes `DHM2` (10 f32 per vertex: `x y z u v r g b a tex`). UVs are no longer reduced with per-vertex `mod 1` (it broke interpolation; `GL_REPEAT` handles tiling).
- **Viewer (`dhview`):** alpha blending + `GL_LEQUAL` (layers share the ground's position), near plane proportional to scale. With `.mdl`: `F` toggles fly/walk; walking uses an XZ triangle grid (Y = up), gravity, jump (Space) and a 0.6*eye max step. `DH_WALK=1` starts in walk mode.
- Pending: runtime LOD by distance (an optimisation: the fine-detail subtree does not stack LODs), objects/props (nodes 25/11), PSMT8H, bike physics.

## `.PTS` (course point graph) — `unpacked/PTS/<LEVEL>.PTS`
`8` header bytes + `2048` records of `32` bytes: `f32 x, y, z` (same world space as the level mesh, verified by overlaying them on ALP2), then eight `i16` fields `a0..a7`, then 4 padding bytes.
- **`a6` = index of the next record** (median distance to it 38 u; chains merge, 64 chain heads in ALP2). Following `a6` from record 0 gives a single clean racing line of 611 points / ~55,000 u that follows the trail along the whole course (`tools/pts_path.py --chain 0`).
- `a0`, `a1`: increasing counters that roughly track progress along the course (not strictly monotone in file order; unit unconfirmed). `a2`: small category (5 most common, then 6, 10, 2, 1, 7). `a5`: 1 or 2. `a4`: flag-like (0, 2, 8, 512, 16384, ...). `a7`: another index (949 distinct values, far-away records): meaning unknown.
- Not yet decoded: which records are start/finish/checkpoints, lane widths, the sibling files in `PTS/` (`.APT .BHS .BLM .BPT .BRD .DPT .FPT .HDT .HLI .HWK .JPT .PED .PKP .RPL .SPT`; most start with the same `01 20 18 10` header and contain f32 coordinates).
- Use: ride/AI line, progress and lap logic, start position, camera rails. Tool: `python3 tools/pts_path.py LEVEL.PTS out.pts [--chain N] [--stats]`; view with `DH_PTS=out.pts dhview level.mdl` (points drawn on top of the model).

## Backdrop / sky (verified in ALP2, JUNGLE, MOAB, PERU, GLACIERT)
Every level's fine-detail subtree has leaf entries whose header word at `+8` holds a *layer* in its high half: `3` terrain cells, `5` other, and exactly one leaf with `2` = **backdrop/sky** (a dome with clouds in JUNGLE, a distant valley in ALP2). `extract_model.py` writes it to `<name>.sky.mdl`; `dhview` loads it automatically and draws it first without writing depth (`DH_NOSKY=1` hides it). `extract_mesh.py` skips it unless `DH_SKY=1`.

## `.TEX` record formats (corrects the earlier "all T8 are 2w x 2h" note) — verified on LOADBAR, LVL
The record's `fmt` field (`hi & 0x3f`) is the **GS format used to upload** the data; the pixel layout depends on it:
- `fmt 0` (PSMCT32 upload, 20,321 records): `w x h` words with the **GS swizzle** applied. The real texture (from the material's `TEX0.PSM`) is T8 `2w x 2h`, T4 or T8H (`gs.read8`, `gs.read4`, `gs.read8h`: for PSMT8H the palette index is the high byte of each 32-bit pixel).
- `fmt 19` (`0x13`, PSMT8 upload, 111 records): **linear** `w x h` bytes, one palette index per pixel, no swizzle. All 78 loading screens use it (512x512 background).
- `fmt 20` (`0x14`, PSMT4 upload, 441 records, all in `LVL`): **linear** `w x h` nibbles, two pixels per byte, low nibble first.
- Other values appear once each in `R/` (16, 18, 10, 28, 47, 30, 40, 32, 8, 14, 7, 51, 42): not decoded.
- Data length is `w*h*4` (fmt 0), `w*h` (19) or `w*h/2` (20). Reading `w*h*4` for a fmt-19 record read into the next record and produced the garbled, striped images seen in the first loading-screen renders.
- Bug fixed in `gs.upload32`: it allocated only `len(data)` bytes, but the GS writes whole 8 KB pages, so textures smaller than a page (e.g. 64x16) addressed past the end and decoded wrongly. It now allocates full pages.
- CLUT: 256-entry palettes use the CSM1 bit-3/4 swap for any 8-bit texture (T8, T8H, linear T8); 16-entry palettes are used as is.

## Full scene graph and visibility ranges (ALP2: 184 mesh leaves, 106 with ranges) — `tools/scene.py: leaf_info`
- Exporting only the fine-detail subtree leaves big holes (start platform, far mountains). The faithful scene is the **whole graph**: every leaf with its accumulated matrix **and** the type-2 selector nodes above it: `(centre xyz, radius, max squared distance)` (400 u, 800 u, 1,000 u, 5,000 u and 6,500 u ranges occur). A leaf is drawn only while the camera is within `sqrt(d2) + radius` of every selector above it.
- `extract_model.py` now writes **`DHM3`**: `DHM2` plus, after the indices, `u32 nchunks` and per chunk `u32 first_index, u32 index_count, u32 nsel, nsel * (cx, cy, cz, radius, dist2_max)`. One chunk per mesh leaf. `dhview` applies the ranges every frame (`DH_NOCULL=1` draws everything). `DH_SUB=fine` restores the old fine-detail-only export.
- Back-face culling does **not** work (tried both windings, both lose half the triangles): the meshes mix winding, so the game draws them double-sided. Keep culling off.
- **Scale:** the 3 lanes of the `.PTS` line are ~22 u apart, which points to about **10 u = 1 m** (a 55,000 u course would be 5.5 km and a 7,500 u drop 750 m; plausible for an alpine downhill). Not confirmed. At this scale the viewer shows a recognisable dirt trail with grass verges and fir trees (alpha-cut textures).

## Textured models: materials and palettes (verified on `BIKE/BOARBIKE`, a boar-shaped mount)
- **Material table in the `.PTR`:** the second list (`nt` u16 offsets) points to **texture ids** inside the `.NGP`, and the third list (`nc` offsets) points to the **GS `TEX0` words**; both lists have the same length and order, so entry *k* of one pairs with entry *k* of the other. Each `V3-32` mesh block is preceded by one or more such pairs (alternative materials).
- `TEX0` fields (standard GS layout): `PSM` (`0x13` = PSMT8, `0x14` = PSMT4), `TW/TH` (log2 of the texture size, e.g. 6 = 64), **`CBP` = palette id** (matches the `id = lo>>16` field of an `.RTX` record).
- **Palettes:** each `.RTX` record is `{next, hash, lo, hi}` + RGBA8 data; 256-colour records are 16x16, 16-colour ones 8x2. The 256-entry CLUTs use the GS CSM1 order: swap bits 3 and 4 of the pixel index (`idx' = (i & ~0x18) | ((i & 8) << 1) | ((i & 16) >> 1)`). Alpha `0x80` = opaque.
- **Textures:** texture id *t* is the `.TEX` record with `id = t`. PSMT8 textures are `2w x 2h` of the record's CT32 upload dimensions (decoded as described above); PSMT4 (small detail textures) is decoded with `gs.read4` (nibble addressing, 32x16 blocks). PSMT8H (`0x1b`, 3 materials in ALP2) is not handled.
- **UVs:** `V2-16` ST values are signed 12-bit fixed point (`/4096`) and are written **unreduced** (no `mod 1` per vertex: it breaks interpolation on triangles that cross a tile edge; the viewer's `GL_REPEAT` does the tiling); `v` is flipped (`1 - v`).
- Tool: `python3 tools/extract_model.py unpacked/BIKE/BOARBIKE out.mdl [--variant N]` writes a `.mdl` that `dhview` displays with textures. `--variant` picks which material of a group is used (which batch uses which material is still unknown). For `LVL/*` groups the tool automatically keeps only the fine-detail subtree and applies the graph transforms (override with `DH_SUB=all` or `DH_SUB=0x50,0xee0`).

## Output formats of the tools (ours, not the game's)
All little-endian.
- **`.msh`** (`extract_mesh.py`): `u32 nv, u32 ni`, `nv * 3 f32` (x y z), `ni * u32` triangle indices. Y is up.
- **`.mdl`, magic `DHM2`** (`extract_model.py`): `u32 ntex, u32 nv, u32 ni`; per texture `u32 w, u32 h` + `w*h*4` RGBA8 bytes; `nv * 10 f32` = `x y z u v r g b a tex` (`r g b a` in 0..~2, 1.0 = neutral; `tex` = texture index or -1); `ni * u32` indices. Textures repeat; `v` is already flipped. (`DHM1` had 9 floats and no alpha; `dhview` reads only `DHM2`.)
- Debug environment variables of the tools: `DH_SUB` (subtrees to keep: empty = fine detail, `all`, or a node list), `DH_NOXFORM=1` (skip graph transforms, `extract_mesh.py`), `DH_MIN/DH_MAX/DH_OFF0/DH_OFF1/DH_RADIUS` (filters). Viewer: `DH_CAM`, `DH_SHOT`, `DH_WALK`.
