# Game-object kinds, gate planes and start grids (NGP nodes with `hdr>>18 != 0`)
Parser: `tools/markers.py` · Test: `tests/test_markers.py` (synthetic). Citations are Ghidra function addresses; uncited = **hypothesis**.

## Node header bit layout (from `FUN_00159910`, `FUN_001a33d0`)
`bits 0-5` node type · `bits 7-17` (11 bits) instance index inside its kind (`(hdr>>7)&0x7FF`, `FUN_001a33d0`) · `bits 18+` **kind** (`hdr>>18`).
The render walker `FUN_0020edb8` calls a per-frame callback (`FUN_00159910`, passed by `FUN_001595e8`/`FUN_001597a8`) for every node whose kind ≠ 0 (cases 0/1/6/0xB/0x2A…); the callback dispatches on the kind:

| kind range | handler (`FUN_00159910`) |
|-----------:|--------------------------|
| 1, 2, 4, 0x10–0x12, 0x14, 0x1D–0x1F | small system kinds: `FUN_002431a8`/`FUN_002431f8`, `FUN_001e0c00`, `FUN_00215f28`, `FUN_00192b58`, `FUN_0011a9e0` (0x13 → node disabled: type set to 9) |
| 0x3E9–0x4AF | `FUN_001afed0` (per-rider object, `DAT_002db300 + idx*0x7BD0`) |
| 0x4B0–0x5DB | `FUN_001443e8` (0x4D8–0x4E2) else `FUN_00164a30(0x2C7F68)` — the generic point-list objects of the PTS siblings (`FUN_00164d58`) |
| 0x5DC–0x76B, 0x820–0x9B0 | no callback (pure scenery; 1603/1612/1629–1631 in ALP2 land here) |
| 4000–4499 `FUN_00195b80`, 5000–5100 `FUN_00199478`, 0x1450–0x14B3 `FUN_00216348`, 6000–6002 `FUN_001eefb0`, 7000–7100 `FUN_0022dd10`, 0x1CE8… `FUN_001477b0`, 12000–12499 `FUN_001b4ce0`, … | other subsystems (not decoded) |
| **0x1F72, 0x1F73 (8050, 8051)** | `FUN_001a33d0(manager 0x4691D0, node)` — **gate planes** |
| **0x1F7C–0x1F84 (8060–8068)** | `FUN_001a3480(0x4691D0, node)` — **start grid** |
| 8000–8499 else | `FUN_00177458(0x3DC4F0)` |
The manager at `0x4691D0` is the race manager (it is also what `FUN_0012c4f8` asks for the start position via `FUN_001a39c0`, and what the results screens `FUN_001a3b50…` read).

## Gate planes — kinds 8050 / 8051 (`FUN_001a33d0`)
Type-11 node; `node+4` → a record whose `vec4 @+0x10` is copied to `manager+0x230 + idx*16`; if `(hdr & 0xFFFC0000) == 0x7DCC0000` (**kind 8051**) `manager+0x460+idx = 1` and `manager+0x18 = 1`; `manager+0x484++`; the node is remembered at `manager+0x1A0+idx*4` and its kind cleared. Kind 8052 also exists (1 node in ALP2) — handler not traced.
**Hypothesis (supported by data, not by a traced consumer):** the vec4 is a plane `(nx, ny, nz, d)`. ALP2: 29 planes (18×8050, 10×8051, 1×8052), all with |n| = 1.000, indices 0–27 interleaved; the `.PTS` racing line crosses them in strictly decreasing line-index order as the index grows (idx 0→298, 1→262, 2→241, 3→235, 4→214, 6→159 …), i.e. progress/checkpoint gates (8051 = the flagged subset, maybe bonus/sector gates — unknown). The crossing consumer was not located (not in `FUN_001a3350…FUN_001a3ed0`, which are results/UI).

## Start grid — kinds 8060–8068 (`FUN_001a3480`, consumed by `FUN_001a39c0`)
Node type 3 (translation at `+0x10`) or 4 (3×3 at `+0x10/+0x20/+0x30`, translation `+0x40`). Slots: 8060→12, 8061→4, 8062→6, 8063→6, 8064→8, 8065→6, **8066→10**, 8068→2. First half uses `A`, second half `B`, each shifted by ±6 u per slot (x for kinds 8064–8066 with `A=(3,9.7,4)`, `B=(−3,9.7,4)`; y otherwise with `A=(5,3,4)`, `B=(5,−3,4)`), then rotated by the node matrix and translated. Stored at `manager+0xB0 + slot*16` (orientation matrix `manager+0x70`); `FUN_001a39c0` returns slot `manager+0xC` and increments it.
ALP2 (8066 @ file 0x186AB0, matrix rows (0.02,−1,0),(1,0.02,0),(0,0,1), translation (−2548, −1318, 6208)): 10 slots from (−2538,−1321,6212) to (−2539,−1291,6212); first three are **12–15 u** from `.PTS` records 4/3 (start of the racing line). Note the slots spread along world **Y** with 6 u spacing (the matrix maps local x to −Y): the up-axis of the start line is therefore not obvious — unresolved.

## Not done
Props (kinds 1603–1631 etc. have no callback: they are ordinary scenery already in the mesh), finish gate identification, 8052/8060–8068 other layouts, the plane consumer.
