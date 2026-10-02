# Native level loader — plan / Plan del loader nativo (no implementado salvo el paso 1)
Goal: `dhview`/the game read the user's ISO (or extracted files) and the level `.NGP/.PTR/.RTX/.TEX` + `PTS/*` directly in C++, with no Python step. Python tools stay as the byte-exact oracle (every native parser is checked against them on all 54 levels, as done for step 1).
Language note: the runtime is C++20 (decided 2026-10-02; Rust was considered and dropped, `docs/en/architecture.md`); the parsers below are written as small, bounds-checked, allocation-light readers so they can be ported.

## Inputs and what each one holds
| File | Size (ALP2) | Contents | Python today |
|------|------------:|----------|--------------|
| `LVL/<N>.NGP` | 12.3 MB | Scene graph + all geometry/collision/markers. Pointers are load addresses: `offset = ptr − 0xA00000`. `u32 count` + `ptr[count]` root table at offset 0. | `scene.py`, `extract_model.py`, `collision.py`, `markers.py` |
| `LVL/<N>.PTR` | — | relocation/reference tables: texture references and TEX0 words per material (`q = 4+4·n0 …`, see `extract_model.py`) | `extract_model.py` |
| `LVL/<N>.TEX` | 9–10 MB | texture records (16 B headers, GS-swizzled data: CT32 upload of T8/T4/8H, linear T8/T4 variants) | `extract_model.py` + `gs.py` |
| `LVL/<N>.RTX` | 0.5 MB | CLUT records | `extract_model.py` |
| `PTS/<N>.PTS` + siblings | 64 KB + small | racing line (`0x09212001`, 2048 × 32 B), trackside point lists (`0x10182001`) | `pts_path.py`, `ptsext.py` |
| ISO | 2.77 GB | ISO9660 volume (primary descriptor `CD001` at sector 16, system id `PLAYSTATION`; `SLES_522.02`, dirs LVL, PTS, R, BIKE, SHELL, LOADBAR …) | external extraction |

## Structures the loader needs (all documented in `docs/formats/`)
1. **Root table & node walker** — node header `type = hdr & 0x3F`, instance `(hdr>>7)&0x7FF`, kind `hdr>>18` (markers.md); children by type: 1 (`+0x20 + 4i`, count `u16 @+8`), 2 (`+0x28 + 8i`, count `@+4`), 3 (`+0x1C + 4i`, count `@+8`), 4 (`+0x50 + 4i`, count `@+8`), 6 (`+0xC + 4i`, count byte `@+0xB`), 8 (`+4`); 7 = node table (175 × 0xC0 B, 4×4 matrix), 15/45 = object grid / probes (`ngpgrid.py`), 25 draw entry, 11 callback hook, 0x2A collision. (`scene.py`)
2. **Collision (0x2A)** — *done*: `src/ngp.hpp::collisionTris`; bit-identical to `tools/collision.py` on all 54 levels (`tests/ngp_col_check.cpp`). Feeds `Ground` (`src/ground.hpp`).
3. **Markers** — gate planes (kinds 8050–8052) and start grids (8060–8068): trivial given the walker (`markers.py`, `src/gates.hpp`).
4. **PTS** — 8-byte header + 2048 × 32 B records (`x,y,z`, `a0..a7`), chain via `a6`; siblings by magic `0x10182001` (`ptsext.py`).
5. **Mesh leaves** — type-1 leaf `+0x20` → chain of VIF packets: `V3-32` positions + `S-8` indices (strip, ADC flags suppress the kick, parity from batch start), `V4-8/V4-5` colour+alpha, UV, per-block TEX0 inheritance; node matrices (types 3/4) and selector distances (type 2) for LOD; layers (`u16 @+0xA`), `Owners` subtree choice (`scene.py`, `vif.py`, `gif.py`, `unpack_ie.py`).
6. **Textures** — TEX record fmt rules (0 = CT32 upload swizzled, 19 = linear T8, 20 = linear T4, T8H by high byte), GS addressing (`gs.py`: `addr32/addr8/addr4`, `upload32`, `read8/read4/read8h`), CLUT index swap `(i & ~0x18) | ((i&8)<<1) | ((i&16)>>1)`.
7. **Sky** — leaf layer 2 (backdrop) drawn first without depth write.

## Pipeline steps removed once the loader exists
`iso extraction → extract_model.py → .mdl`, `collision.py → .col`, `markers.py → .gates/.pts`, `pts_path.py → chain0.pts`, `tools/play_level.sh`. Remaining offline-only: `ghidra` scripts, `contact_sheet.py`, `loadbar_export.py`, research tools.

## Implementation order (each step ends with a byte/number-exact check against the Python oracle on all 54 levels)
0. **ISO9660 reader** (list + extract a file; sector size 2048 — the `;1` version suffix and name level were not checked) — small, unblocks "no manual extraction"; needs the ISO only, no game data in the repo.
1. ✔ `ngp::Reader` + **0x2A collision** (done: 54/54 identical).
2. Root table + node walker + `Owners`/layers (compare node counts per type with `scene.py`).
3. Markers/gates/start grid + PTS chain (compare with `markers.py`, `pts_path.py`).
4. TEX/RTX/GS swizzle → RGBA textures (compare PNG hashes with `extract_model.py` output).
5. VIF/GIF leaf decode → vertex/index buffers (compare triangle sets; this is the largest step).
6. Materials/vertex colour/alpha, selectors/LOD, sky; then GL upload replacing the `.mdl` path in `dhview`.
7. Animation (`.NGA`) when the sampler is found.

## Risks
* **Untrusted binary input**: every pointer/count must be bounds-checked (the Python code mostly trusts the data). Step 1's `Reader` returns 0/false instead of reading out of range; keep that pattern. Fuzz each parser with truncated/mutated NGPs before it is used on user files.
* **VIF/GIF decode** (step 5) has the most undocumented behaviour (ADC, multi-batch blocks, nested unpack): keep `tests/test_vif.py`-style golden vectors and compare triangle sets, not just counts.
* **Pointer base** `0xA00000` is inferred from `.PTR`; levels loaded at other bases (other regions/ISOs) are untested — assert it at load.
* **Memory**: ALP2 `.mdl` is 56 MB; the native path can stream chunks (DHM3 chunks/selectors exist for that).
* **Instance transforms of collision nodes** are ignored (identity assumed, collision.md); moving/rotated instances will need the 0x4C-byte instance records decoded before dynamic props collide.
* **Unit/axis conventions** (coordinates.md): Z-up in the NGP, 10 u = 1 m is a hypothesis. Keep conversion in one place (`Ground::load`, `toYUp`).
* **Legal/hygiene**: the loader reads the user's ISO at runtime; nothing derived from it is committed or packaged (guard hooks stay).
