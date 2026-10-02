# Scene graph payloads and instancing / Cargas útiles del grafo e instanciación
**Bug fixed (2026-10-02):** the exporter treated only type-1 nodes with ≤1 child as mesh "leaves" and gave each a single matrix. Props (trees, flags, signs) are **payload nodes of type 0** reached many times through type-3/4 placement nodes, so they were exported once, unplaced, at the origin (or not at all near the player). `tools/scene.py: walk_payloads` now yields **one entry per visit** of every type-0 node (matrix, selector ranges, layer/kind from the nearest type-1 ancestor); `Owners` keeps all instances (`by_ptr`) and `tools/extract_model.py` emits one copy per instance. ALPINE: 1 941 payload nodes, **3 874 visits** (max 77 per model: the tree models), 429 826 triangles (was ~325 000), 3 822 chunks (was 137).

## Layout facts (decompiled walker `FUN_0020EDB8`, verified on data)
* **Type 1** = group: `u16 @+8` = child count, `u16 @+0xA` = layer mask (`uGpffffaa58 & mask != 0` → node disabled), children pointers from `+0x20` (a 1-child group's pointer is a single payload/child). Terrain cells = layer 3 with up to 30 children; each cell has a twin group of **layer 99 = vegetation/props** with similar centre/radius.
* **Type 3** = translation `+0x10..0x18`, count `u32 @+8`, children from `+0x1C`. **Type 4** = 3×3 at `+0x10/+0x20/+0x30`, translation `+0x40`, children from `+0x50`.
* A **tree** = type-1 group (3 children): two type-2 distance selectors (LOD) → type-0 payloads, and a type-11 hook with kind 1629–1631. The same group is referenced by ~70–80 placement nodes.
* **Type 0** = payload node: VIF chain data follows it up to the next node.

## Evidence / how it was found
1. In the game's own savestates the camera is at EE RAM `0x3A8D00` (`tools/p2s_camera.py`, `tools/compare_view.py` render dhview from that camera next to the game's screenshot; FOV ≈ 1.0 rad vertical validated with the `BRD` bird points projecting onto the birds).
2. **Oracle experiment** (PCSX2 + PINE): zeroing 64 KB blocks of the level image in RAM and screenshotting showed that one block (file `0x640000–0x64FFFF`) held the scene-graph nodes of the player's cell and made every tree disappear while terrain/cabin/flags stayed.
3. That block contains the layer-99 group `0x648900` whose 12 children are type-3/4 placements of models `0x9009A0`, `0x8FE4F0`, … ; a full DAG walk visits model `0x9009A0` 77 times (the old `instances()` saw it 0 times).
4. After the fix the same camera shows conifers, the log cabin and the diamond flags in the same layout as the game (not committed: images are game material).

## Still different from the game (not fixed)
Sky/fog colour (game: dusk purple + fog; dhview: flat colour), lighting tint, the rider/bike are not drawn in this view, translucent ground quad near the camera, HUD pieces from the library still stack at the origin (they are UI meshes placed by HUD code), kinds ≠ 0 (rider parts 4000–4499, objects 1200–1499 hooks) are excluded by default (`DH_KINDS`).
