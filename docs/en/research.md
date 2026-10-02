# Research workflow

🇪🇸 [Versión en español](../es/research.md)

Copyright (C) 2026 Pedro Soto and the DownHill-Port-PC contributors. Licensed under GPL-3.0-or-later.

This document explains how findings in `formats.md` were obtained, so anyone can reproduce and extend them.
**Never commit game data, the game executable, extracted assets, Ghidra projects or decompiler output.**

## 1. Prepare the data (your own disc image)

```sh
7z x "Downhill Domination.iso" -oiso_extract        # disc contents (git-ignored)
python3 tools/unpack_ie.py iso_extract unpacked     # inflate the IE containers (git-ignored)
```

The disc is the PAL release (`SLES_522.02`). The executable is a stripped MIPS R5900 ELF (entry `0x10a008`)
with VU microcode in `.vutext`.

## 2. Ghidra setup

1. Install Ghidra (`pacman -S ghidra` on Arch).
2. Install the [ghidra-emotionengine-reloaded](https://github.com/chaoticgd/ghidra-emotionengine-reloaded) release matching your Ghidra
   version into `~/.config/ghidra/<ghidra_version>_DEV/Extensions/`. Plain MIPS produces broken output for the R5900 (MMI, 128-bit ops).
3. Import with language `r5900:LE:32:default`:

```sh
mkdir ghidra_projects
ghidra-analyzeHeadless ghidra_projects dh -import iso_extract/SLES_522.02 -processor "r5900:LE:32:default" -overwrite
```

## 3. Scripts (`tools/ghidra/`)

| Script | Use |
|--------|-----|
| `run.sh Script.java args...` | Runs a script against the imported program without re-analysing |
| `StrRefs.java <substr> <out> [n]` | Finds defined strings containing `<substr>` and decompiles the functions that reference them |
| `DecompFn.java <out> <addr>...` | Decompiles the functions at the given hex addresses into `<out>` |
| `RefsTo.java <out> <addr>...` | Lists references to addresses (e.g. extension strings) |
| `RefsSym.java <out> <substr>...` | Lists references to symbols by name |

Write outputs under `decomp/` (git-ignored).

## 4. Method that worked

The game ships many debug strings (`"Texture Setup w %d h %d ..."`, `"CLUTs Loaded = ..."`). Search strings → follow xrefs →
decompile the function → confirm the structure against real data with a small Python script → document it in `formats.md`.
Examples: the texture chain layout, the `.PTR` relocation table and the load order NGP/RTX/TEX/PTR all came from this.

## 5. Visual checks

`dhview` can capture a frame for review:

```sh
DH_CAM="x y z yaw pitch" DH_SHOT=out.bmp ./build/dhview mesh.msh
```

For `.mdl` files add `DH_WALK=1` to start in walk mode (terrain collision, gravity); `F` toggles fly/walk interactively.

## Viewer controls and debug variables (`dhview`)
- `.mdl`: WASD + mouse, Shift = faster, `F` fly/walk (gravity, terrain collision), `R` **bike mode** (prototype), Space = jump, Esc = quit. A sibling `<name>.sky.mdl` is loaded automatically.
- **Bike mode (prototype):** a point mass that slides down the terrain: gravity along the slope, rolling and air resistance, W = pedal, S = brake, A/D = steer (less at speed), Space = hop, 3rd-person camera, red pyramid marker. It starts at the first point of the `DH_PTS` line that has ground below it (2 m/s push-off) and respawns at the last good point if it leaves the map. Assumes **100 units = 1 m** (unconfirmed; `DH_UNIT=n` changes it).
- **Known limit:** collision uses the *visual* mesh, whose sheets overlap in XZ (overhangs, cliff faces, the coarse far hull), so the rider can climb onto or get stuck on the wrong sheet; the full ALP2 line cannot be ridden yet. Needs the game's own collision data (candidates: the `PTS/` sibling files, e.g. `.HDT` = table of 38 `(start:u16, count:u16)` groups). Constraining the ground search to the height of the nearest `.PTS` point was tried and made it worse (the line overlaps itself where the course crosses over itself).
- Debug variables: `DH_CAM="x y z yaw pitch"`, `DH_SHOT=file.bmp` (save a frame and exit), `DH_FRAMES=n` (frame to save), `DH_DT=seconds` (fixed time step), `DH_WALK=1`, `DH_RIDE=1`, `DH_RIDE_AT=i` (start at line point i), `DH_AUTOSTEER=1` (autopilot along the line), `DH_AUTOPEDAL=1`, `DH_PTS=file` (draw raw `f32 x,y,z` points over the model, e.g. from `tools/pts_path.py`), `DH_NOSKY=1`, `DH_DEBUG=1` (ride trace).
- `extract_mesh.py` can dump the owning leaf of each triangle with `DH_OWNERS=file` (u32 per triangle) to find out which graph leaf a surface belongs to.
- Observed in ALP2: at one XZ position the visual mesh has up to 8 vertically stacked sheets (e.g. -4154, -2691, -1197, -24, 276, 3167): the huge coarse leaf `0x860da0` contributes the top/bottom shells; leaf `0x368bf0` contributes cliff faces.

## Asset directories, decoded and viewed (2026-10-02; `tools/contact_sheet.py`)
- **`LOADBAR/`** (78 groups): the **loading screens**, `L<LEVEL><LANG>` (`AR FR FS MX SC SE TD` = languages/regions, e.g. `LALPSE`). A flat quad mesh (XZ plane, Y up) with a 512x512 linear-T8 background (`fmt 19`) plus small textures for frames/bars; some quads have no material at all (vertex colour only). They render with the original artwork (Alps, canyon, city, glacier, jungle, Moab, Peru, ...); `LJUMPSE`/`LTRAINSE` are the controller help screens and `LSHELL` the title. `LBIKESHP`, `LPODIUM`, `LOADBAR1`, `MCSTART` look empty and are still unexplained.
- **`SHELL/`** (10 groups): `UI*` (5 languages) is the circular menu ring; `BIKESHOP*` is the bike-shop room (`F G I S` language variants). Both are 3D scenes, not flat images.
- **`BIKE/`** (70 groups): **bike parts**, not whole bikes: frames (`CANFIELD`, `GIAAC16`...), forks (`FOX125R`, `MARZ...`), handlebars (`MAN100`...), wheels (`WH...`), plus `BIKESKEL` (a skeleton/assembly model, to verify) and the animation files `BANIM.NGA` (+ `CARTER.RRS`). The bike is assembled from parts at run time.
- **`R/`** (194 groups): rider assets. Name = pilot letter + outfit code. Each group is a set of body segments (arms with gloves, legs) in local space to be posed by a skeleton, so a rider needs the skeleton and its animation (`NGA`) before it can be shown assembled.
- `SKAT/` (`.SKX`, `.CTL`, 97 MB), `REP/`, `RST/`, `SND/`, `VAG/`, `MOV/` (38 `.PSS` videos, 1.5 GB) are still untouched.
- Non-level models look dark: their vertex colours average 0.4-0.7 and the original engine presumably brightens (PS2 modulation overbright); a gain control in the viewer is still to do.

## Bike and rider assets (2026-10-02)
- **Bike parts share one space.** Length = Y, up = Z, **1 unit ~ 0.28 m** (a frame is ~6.5 units = 1.8 m, wheels 2.4 units = 0.7 m). Frames (payload 903, e.g. `CANFIELD`, `GIAAC16`), handlebar+fork pieces (payload 904, `MAN100`, `FOX125R`, `FKPAT`...) and wheels (payload 905, `WH*`, `ENTRWH`) have **no transforms in their graph** (`leaf_info` is empty). Wheels are centred at the origin; the handlebar+fork piece is authored **vertical** at the bike centre.
- **Low-quality impostor inside every frame:** a 6-vertex flat quad (texture 0, 64x64, spanning the whole frame bbox) with a side picture of the bike. It is the distance LOD; drop it when assembling the detailed bike. Its alpha mask gives the true layout (u = rear->front along Y, v along Z): wheel hubs at **Y = -1.60 (rear) and +2.43 (front), Z = -1.84** (wheelbase 4.03 u ~ 1.13 m) and a fork tilted ~25.7 deg from vertical, matching the constant 0.436 rad (25 deg) in the shell bike animation `SHBANIM.NGA`.
- **Assembly (verified by eye, `tools/assemble_bike.py`):** frame + wheels at the hubs + the fork piece rotated about X by 0.436 rad (top towards the rear) with its bottom end (centre of the vertices with Z < -1.9, at (Y 0.13, Z -2.28)) moved to the front hub. Result: a complete downhill bike (tilted fork, seat, swingarm). The exact attachment data presumably comes from `BIKESKEL` + `BANIM.NGA`, not yet decoded; the numbers above are measured, not read from the game.
- **`BIKE/BIKESKEL`:** payload 900; its node (type 25, payload 900) points to a block with `u32 17` (joint count?), then identity 4x4 matrices (bind pose = identity) and the string `Banim` (the animation set). Positions therefore come from the animation, not from the skeleton.
- **`.NGA` (animation) so far:** `u32 id/hash`, then `(anim id, offset)` pairs (`SHBANIM`: 4 animations `0xC6..0xC9` at stride `0x120`; `RANIM`: ~hundreds). A common block precedes them: entries `u32 0xCCCC0003, f32 value` = **constant channel CCCC** (27 channels: 9 groups of 3, probably x/y/z). Animation blocks start with `0x03e0000c, f32 0, f32 2.0 (duration?), u32 0x1b (channel count = 27)` followed by per-channel codes (`0x0002_0002` x13 then `0xCCCC0003` constants...). Non-constant channels (`R/RANIM.NGA` has byte-quantised curves) are not decoded.
- **`R/` are first-person arms, not whole riders:** each group is two forearms with gloves (team colours/skins), a small stick-like piece between them, and a large blurry plane (textures 1 and 2: 12 and 21 vertices spanning the whole view, probably a shadow/blur overlay). The 16 `*DI`/`*IO` groups were wrapped in nested `IE` layers (variant `0x0a`); `tools/unpack_ie.py` (nested-container support by Brandon Gil) unwraps them. Each is one large flat quad with rider artwork (jersey/portrait) plus the same stick-like piece; 441 vertices, 147 triangles.
- **`R/KCLOPS.MB`** is a **Maya 4.0 binary** (IFF `FOR4`) left in the data: scene `C:/downhillDB_local/DH_RIDERS/XRider_skel/skel_medium.mb` with the rider rig: joints `xx_hip_RH/LH`, `xx_knee_RH/LH`, `xx_lumbar`, `xx_neck`, `xx_shoulder_RH/LH`, `xx_elbow_RH/LH` (10 joints), IK poles and ragdoll locators (`xx_rag_loc_*`). `R/KCLOPS.NGO` (900 KB) lists that rig's translate/rotate channels by name. Useful as documentation of the rider skeleton; the full rider body mesh has not been located yet (not in `BIKE/` or `R/`).
- **`SKAT/DHSKAT.SKX` (101 MB) is audio:** magic `SKEX`, then `VAGp` (PS2 ADPCM) headers and the `KAudioDLL` tag; `DHSKAT.CTL` is its index (offset table with 0x44-byte stride). Not skeleton/mesh data.
- Viewer: `DH_BIKE=assembled.mdl` draws the assembled bike in ride mode; `DH_GAIN=n` (brightness multiplier; default 2 for models without chunks), `DH_ORBIT="yaw pitch"` (orbit camera around a model), `DH_BG="r g b"` (background colour).

## 6. Where to help next

- Decode the rest of `PTS/` (start/finish/checkpoints in `.PTS`, the sibling `.APT/.HDT/...` files) and build lap/progress logic on the course line.
- Runtime LOD by distance (type-2 nodes carry the max squared draw distance); an optimisation, not a correctness issue.
- Game objects: nodes of type 25/11 and payload kinds > `0x3E8` (props, triggers, start/finish).
- **Collision data first:** find the game's own collision/surface data (see the limit above) before refining the riding physics.
- Rider: find the full-body mesh (not in `BIKE/`/`R/`/`SKAT/`; look at `SHELL`, `REP`, `RST`) and decode `.NGA` curves; bike: read the real attachment transforms from `BIKESKEL` + `BANIM.NGA` instead of the measured ones. Start by reading how the game moves the camera/rider along the course.
- PSMT8H (`0x1b`) textures; which batch uses which material (`--variant`).
- VU1 interpreter / VIF / GIF in the runtime, only if animated geometry needs it.
- Formats still untouched: `.PTS`, `.RST`, `.REP`, `.BNK`, `.SKX`, `.CTL`, `.APT`, `.BHS`.
- Architecture and language questions: see [`architecture.md`](architecture.md).
