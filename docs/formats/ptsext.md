# PTS sibling files / Hermanos del .PTS (`APT HLI DPT FPT PED PKP BHS BRD RPL HDT`)

Parser: `tools/ptsext.py` · Test: `tests/test_ptsext.py` (synthetic data, no game assets).

## Point lists / Listas de puntos (all but HDT)
`u32 magic = 0x10182001 · u32 count · count × record` (file size = 8 + count·stride, verified on all 415 files of the ISO).
Stride 32 B (RPL: 48 B). Record / registro:

| off | type | meaning / significado |
|----:|------|-----------------------|
| 0 | f32×3 | x, y, z — **same world space as the .mdl and .PTS** (overlay in dhview matches the terrain) |
| 12 | f32 | w: 0 except BRD (≈0.73–0.80, unknown) and PKP (non-finite-looking packed value, unknown) |
| 16 | u32 | subtype/type id (APT: 0,5,10,25…; DPT: 0,2,15,25; BRD: 0–50). Per-file enumeration unknown |
| 20 | u16 | flag (mostly 0/1) |
| 22 | u16 | 1 or 2 (lane / side? unconfirmed) |
| 24 | u16 | id / link index (≤ count, behaves like PTS `a6`) |
| 26 | u16 | small, mostly 0 (unknown) |
| 28 | u32 | mostly 0 (unknown) |

**Finding:** these are *not* collision geometry. Plotted over ALP2 (APT 531, PED 170, PKP 96, HLI 91 points) they all lie along the track corridor: they are trackside objects/triggers (probable roles from names: APT=?, PED=pedestrians/spectators?, PKP=pickups?, HLI=highlights?, DPT/FPT=?; **names are guesses, unconfirmed against the ELF**). Real collision data is still to be found (not in these files).

## HDT
`u32 n · n × (u32 kind, u16 start, u16 count) · payload`. Groups are contiguous (`start_i+1 = start_i+count_i`) on ALP/ALP2/JUNGLE/JUNGLE2; payload (≈18.6 KB, ~1.7k non-zero dwords) undecoded. Total count equals the HLI count on ALP (91) but not on JUNGLE (107 vs 68) → coincidence; indexes some other table. MOAB/MOAB2 (6176 B) do **not** follow this layout.
