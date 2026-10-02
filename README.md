# DownHill-Port-PC

🇬🇧 English · 🇪🇸 [Español](README.es.md)

A community-driven effort to bring **Downhill Domination** (Incognito Entertainment, PS2, 2003) to modern PCs, natively on **Linux and Windows**.

This is a project **by the community, for the community**. It is not affiliated with, endorsed by, or connected to the original developers or publishers.

> **Status: early reverse-engineering.** There is no playable game yet. What exists today is a set of format notes, extraction tools and a small 3D viewer. See [Roadmap](#roadmap).

## Goal

A native engine reimplementation (not an emulator wrapper) that loads the game's original data from **your own legally obtained disc image**, and runs on current systems with modern resolutions, input and frame pacing.

## Legal notice

- **This repository contains no game data, no game code and no disassembly or decompilation output.** No ISO, executable, textures, models, audio or video are, or will ever be, included.
- You must supply **your own copy** of the game. The tools read your image locally and nothing is uploaded anywhere.
- `.gitignore` is set up to keep disc images, extracted assets, the game executable, Ghidra projects and decompiler output out of version control. **Please keep it that way in pull requests.**
- Documentation here describes *file formats and structures* discovered through interoperability research. If you are a rights holder and have concerns, please open an issue.

## What works so far

| Area | State |
|------|-------|
| ISO extraction | Done (any ISO tool; `7z x` works) |
| `IE` asset container (`.NGP/.PTR/.RTX/.TEX`) | Unpacker working (`tools/unpack_ie.py`); a few nested variants pending |
| `.TEX` textures | Decoded to 8-bit indexed images (GS swizzle undone), exported as grayscale |
| `.RTX` palettes | Decoded; texture-to-palette link solved through the `.PTR` material table |
| `.PTR` relocation table | Understood (base `0xA00000`) |
| `.NGP` geometry and scene graph | Solved for static level geometry: node walk, transforms, fine-detail subtree, strips with ADC rule, UVs, vertex colour and alpha |
| VU1 microcode | Located and disassembled (`tools/vudis.py`); interpreter not needed so far for static geometry |
| Textured models and levels | All 54 levels extract; ALP2 is exported as the **whole scene graph** with per-leaf visibility ranges (`DHM3`). All texture upload formats decoded (swizzled CT32, linear T8/T4, T8H). 78 loading screens render with their original art. Bike parts (`BIKE/`) and rider segments (`R/`) render; assembling them needs the skeleton + animations |
| Viewer (`dhview`) | SDL3 + OpenGL viewer for points, triangles and textured models (`.mdl`): free camera, alpha layers, walk mode with terrain collision |
| Course line | `.PTS` decoded as a point graph (racing line of ~55,000 u in ALP2); start/finish/checkpoints and the sibling files pending |
| Bike / physics | Point-mass prototype (`R` in the viewer); real collision data not found yet, so it cannot ride a full course |
| AI, audio, menus | Not started |

Format notes live in [`docs/en/formats.md`](docs/en/formats.md); the Ghidra setup and research workflow are in [`docs/en/research.md`](docs/en/research.md); architecture and the language decision are in [`docs/en/architecture.md`](docs/en/architecture.md). All are also available in Spanish under [`docs/es/`](docs/es/).

## Roadmap

1. ✅ Walk the scene graph and render clean, textured level geometry (static terrain done).
2. ✅ Backdrop/sky split. Next: decode `PTS/` (course line found, start/finish pending), props and game objects, optional runtime LOD.
3. Decide the runtime language and architecture, then write the native asset parsers ([`docs/en/architecture.md`](docs/en/architecture.md)); the Python tools become the reference oracle.
4. Collision data, bike model, animation, camera and riding physics (point-mass prototype exists).
   (A VU1 interpreter is only planned if animated geometry turns out to need it.)
5. Audio (VAG/BNK), video (PSS), menus and game modes, save data.
6. Packaging: `.deb` and Windows builds. The package will **not** ship game data; the app will extract assets from your image on first run.

## Building

Requirements: a C++20 compiler, CMake ≥ 3.20, Ninja, SDL3 and OpenGL development files.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/dhview path/to/mesh.msh      # WASD + mouse, Shift = fast, Esc = quit
./build/dhview level.mdl             # same, plus F = fly/walk (gravity + terrain collision), R = bike prototype, Space = jump
```

Packaging (no game data included): `cd build && cpack -G DEB`.

## Tools

Python 3 with Pillow is enough for everything in `tools/`. Python is used only for research and offline conversion; the runtime is C++ (see [`docs/en/architecture.md`](docs/en/architecture.md) for the language discussion, including a possible move to Rust).

| Tool | Purpose |
|------|---------|
| `tools/unpack_ie.py` | Inflate the `IE` containers from an extracted disc folder |
| `tools/tex_dump.py`, `tools/tex_export.py` | Inspect and export textures (`gs.py` has the GS swizzle tables) |
| `tools/vif.py` | Minimal VIF packet decoder |
| `tools/scene.py` | Scene-graph walker for `.NGP` files; `instances()` / `Owners` give per-leaf transforms and the fine-detail filter |
| `tools/extract_model.py` | Build a textured `.mdl` (mesh + materials + palettes + vertex colour/alpha) from a model or level group |
| `tools/batch_models.py` | Run `extract_model.py` over every complete group in a folder (e.g. all levels) |
| `tools/pts_path.py` | Read a level `.PTS` (course point graph) and export its points / main racing line |
| `tools/contact_sheet.py` | Render every `.mdl` of a folder with `dhview` into one labelled contact-sheet image |
| `tools/assemble_bike.py` | Assemble a bike from frame + handlebar/fork + wheel `.mdl` parts (measured hub positions, tilted fork) |
| `tools/vudis.py` | VU1 microcode disassembler |
| `tools/extract_mesh.py`, `tools/extract_points.py` | Pull vertices/triangles out of `.NGP` files |
| `tools/preview_msh.py` | Quick 2D preview of an extracted mesh |
| `tools/ghidra/` | Headless Ghidra scripts (string xrefs, function decompilation) |

Reverse engineering uses [Ghidra](https://ghidra-sre.org/) with the community [ghidra-emotionengine-reloaded](https://github.com/chaoticgd/ghidra-emotionengine-reloaded) extension (language `r5900:LE:32:default`).

Typical workflow:

```sh
7z x "Downhill Domination.iso" -oiso_extract
python3 tools/unpack_ie.py iso_extract unpacked
python3 tools/tex_export.py unpacked/SHELL/BIKESHOP.TEX out_textures
```

## Contributing

See [`CONTRIBUTING.md`](CONTRIBUTING.md) and [`SECURITY.md`](SECURITY.md) first.

Contributions are very welcome, especially:

- Reverse engineering: formats (`.PTS`, `.RST`, `.REP`, `.BNK`, `.SKX`, ...), VU1 microcode, game logic.
- Rendering and engine work in C++.
- Testing on different PAL/NTSC releases of the game (so far only the PAL `SLES_522.02` build has been examined).
- Documentation of anything you figure out.

Ground rules:

1. Never commit game data, executables, extracted assets or decompiler output.
2. Document findings in `docs/en/formats.md` and `docs/es/formats.md` (both languages, if you can) in your own words (structures and field meanings, not copied code).
3. Keep tools small and runnable; a short check beats a long explanation.

## Acknowledgements

Thanks to the PCSX2 and Ghidra communities, and to everyone who has documented the PS2 hardware (GS, VIF, VU) over the years.

## Authors

Created and maintained by **Pedro Soto**, with contributions from the community (see [`AUTHORS`](AUTHORS)).

## License

Copyright (C) 2026 Pedro Soto and the DownHill-Port-PC contributors.

The code in this repository is licensed under the **GNU General Public License v3.0 or later** (see [`LICENSE`](LICENSE)). Contributions are accepted under the same license.

This license covers only the code and documentation in this repository. Game assets, code and trademarks belong to their respective owners and are not included.
