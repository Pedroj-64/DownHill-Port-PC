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
- Pending: topology (strips/indices), UVs, colours, materials (texture<->CLUT link) and the VU1 microcode that consumes them.

## VIF mesh fragments (verified on LVL/ALP2)
Each fragment is: `UNPACK V3-32 xN @0xB5` (positions) → `STCYCL cl=3 wl=1` → `UNPACK V4-32 x1 @0x238` (GIF tag: NLOOP = vertex count, EOP, NREG=3 with regs ST, RGBAQ, XYZ2)
→ `UNPACK S-8 xM @0x239+3i` (**indices** into the positions, in strip order; M ≤ N: only part of the vertices is used) → `UNPACK V4-8/V4-5 xM` (colour)
→ `UNPACK V2-16 xM` (signed fixed-point ST) → `MSCNT/MSCAL` (runs VU1 microcode).
- Triangle strips have restarts (see the ADC rule below), which `tools/extract_mesh.py` applies. Rings/ribbons (shadows) come out fine; terrain still looks mixed (several stacked LODs, no instance transforms).
- Some fragments are in local space (coords ~±5: props, shadows) and some in world space (terrain, coords ±10⁴).
- Tools: `tools/vif.py` (decoder), `tools/extract_mesh.py` (→ .msh), `tools/preview_msh.py`, and the `dhview` viewer (`DH_CAM="x y z yaw pitch" DH_SHOT=out.bmp` for screenshots).

## Scene graph (first clues)
- `.NGP` header: `u32 4` + 4 pointers to tables: `0x1080` (chains/models), the node table (`0xB66F00`: `u32 7, u32 175` + 175 pointers to 0xC0-byte nodes), and two more.
  Node: `u32 0x7f0c0019`, 5 pointers, mesh pointer, **4x4 transform matrix** (floats, identity in the sample), flags.
- 0x30-byte directory entries: `[1, 0, 0x00010001, f32 ?, 4 f32 (likely a sphere: centre xyz + radius), pointer to draw chain]`.
- Pending: walk the graph (instances + LODs), UVs/colours/materials and the texture<->CLUT link. This requires reading the draw traversal in Ghidra.

## VU1 microcode (`tools/vudis.py`)
Dump: ELF section `.vutext` at `0x00269d90`, `0xA140` bytes (5160 64-bit instructions = several programs), file offset `0x160d90`. Programs are uploaded with `MPG` packets.
Disassemble with `python3 tools/vudis.py vutext.bin > vu1.asm`. Encoding reference: the public VU opcode tables (PCSX2 `DisVUmicro.h` / `DisVUops.h`, GPL).
- The first 0x400 instructions are data; real programs sit in ~0x400-0xB00 and 0xC00-0x1280. There are 20 `XGKICK` sites = 20 output paths (vertex types).
- The vertex program near `0xC24` reads per-vertex records of 3 qwords (stride 3, matching `STCYCL cl=3 wl=1`), fetches the position with `LQ vf20, 0xB5(vi9)` (`vi9` = vertex index from the S-8 array), transforms it by the matrix in `vf1..vf4` (`MADDAx/y/z`), does the perspective divide (`DIV Q, vf0w, vf25w`), converts to GS fixed point (`FTOI4`) and writes output qwords with `SQ`.
- **Strip restarts (ADC) are computed at run time** from clip flags (`CLIP`, `FSAND/FCAND`) and per-record flag words (`ILWR.w vi7`, `IAND`), then written into the `w` lane of the XYZ2 qword (`ISW.w`). The per-record flags come from small `STMASK` + `S-8 x2` patch packets that follow the index array: a patch at address `A` flags vertices `v = (A - (hdr+3))/3` and `v+1` (`hdr` = address of the `V4-32` header), i.e. the first two vertices of each strip. **Verified** on the 42-vertex sample (patches at vertices 6-7, 10-11, 14-15, ... = starts of each quad group). `tools/extract_mesh.py` now applies this rule instead of a heuristic.
- Consequence: faithful geometry needs either (a) a VU1 interpreter + VIF unpack + GIF parser (high-level emulation of the PS2 vector path) or (b) hand-reimplementing each of the ~20 programs. Plan: prototype (a) in Python to validate, then port it to C++.
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
- Pending: apply node transforms (types 3/4), choose one LOD per region by distance, link materials to textures/CLUTs.

## Textured models: materials and palettes (verified on `BIKE/BOARBIKE`, a boar-shaped mount)
- **Material table in the `.PTR`:** the second list (`nt` u16 offsets) points to **texture ids** inside the `.NGP`, and the third list (`nc` offsets) points to the **GS `TEX0` words**; both lists have the same length and order, so entry *k* of one pairs with entry *k* of the other. Each `V3-32` mesh block is preceded by one or more such pairs (alternative materials).
- `TEX0` fields (standard GS layout): `PSM` (`0x13` = PSMT8, `0x14` = PSMT4), `TW/TH` (log2 of the texture size, e.g. 6 = 64), **`CBP` = palette id** (matches the `id = lo>>16` field of an `.RTX` record).
- **Palettes:** each `.RTX` record is `{next, hash, lo, hi}` + RGBA8 data; 256-colour records are 16x16, 16-colour ones 8x2. The 256-entry CLUTs use the GS CSM1 order: swap bits 3 and 4 of the pixel index (`idx' = (i & ~0x18) | ((i & 8) << 1) | ((i & 16) >> 1)`). Alpha `0x80` = opaque.
- **Textures:** texture id *t* is the `.TEX` record with `id = t`. PSMT8 textures are `2w x 2h` of the record's CT32 upload dimensions (decoded as described above); PSMT4 (used for small detail textures) is **not implemented yet** (those triangles draw untextured).
- **UVs:** `V2-16` ST values are signed 12-bit fixed point (`/4096`) with wrap-around (`mod 1`); `v` is flipped (`1 - v`).
- Tool: `python3 tools/extract_model.py unpacked/BIKE/BOARBIKE out.mdl [--variant N]` writes a `.mdl` that `dhview` displays with textures. `--variant` picks which material of a group is used (which batch uses which material is still unknown).
