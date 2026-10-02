# NGP node types 25, 11 and 42 (partial) / nodos 25, 11 y 42 (parcial)
Source: scene walker `sg2.c` (`FUN_0020effc` cases `0x19`, `0xb`, `0x2a`) + ALP2 data. **No parser yet** — only what the engine code proves.

| type | engine behaviour | file layout known |
|-----:|------------------|-------------------|
| 25 (0x19) | draw-list entry: copies the accumulated world matrix + `payload = node+0x2c`, `count = u32 @ +0x14` into a 0x60-byte slot (max 150 per frame); skipped if count = 0. A mesh container different from type 1 leaves. | `+0x14` count, `+0x2c` payload |
| 11 (0xb) | **callback hook**: if `hdr>>18 != 0` calls the registered callback `(*param_2)(id, &pos, &xform)` with the node's world position — spawn/placement markers for objects. | `hdr = 0x0b \| kind<<18`; ALP2: 98 nodes, kinds 8050 ×18, 8051 ×10, 1629 ×8, 1630 ×7, 1631 ×6, 1603 ×3, 1612 ×3, kind 0 ×25 … |
| 42 (0x2a) | render: same callback path as 11 (never drawn). **Physics: this is the collision mesh node** (`FUN_00216780`), see `collision.md` | ALP2: 777 nodes (18 reachable from the tree) |
See `markers.md` for the kind dispatcher (`FUN_00159910`), gate planes (8050/8051) and start grids (8060–8068). Remaining unknown: meaning of other `kind`s, which ones are start/finish/checkpoints/props, the float block at `+0x2c` (type 11 looks like `x?,y?,z?` + two floats ≈ 50/66; coordinates did not match the terrain directly). Next step: decompile the callback registered by the level loader and map kinds → object types.
