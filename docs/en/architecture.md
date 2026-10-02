<!-- SPDX-FileCopyrightText: 2026 Pedro Soto -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Architecture and language decision

🇪🇸 [Versión en español](../es/architecture.md)

## 1. How the pieces fit today

```
your ISO ──7z──▶ iso_extract/ ──unpack_ie.py──▶ unpacked/ (.NGP .PTR .TEX .RTX ...)
                                                    │
                       offline tools (Python, tools/): decode formats, walk the scene graph,
                       build .msh / .mdl
                                                    │
                                                    ▼
                                      dhview (C++20, SDL3 + OpenGL)
                              viewer, free camera, walk mode with terrain collision
```

- **Python is research and conversion tooling only.** It exists because format discovery needs fast iteration (change a rule, re-render, compare). It is not, and will not be, the game runtime.
- **The only runtime code is C++** (`src/main.cpp`, ~200 lines today). It reads the `.mdl`/`.msh` the tools produce.
- **The documented formats (`docs/*/formats.md`) are the real product of the research.** They are language-independent: any implementation, in any language, can be written from them.

## 2. Target architecture (what "the port" means)

A single native program that, on first run, reads the user's own disc image, converts what it needs into its own cache (the equivalent of today's `.mdl`) and then plays from that. No game data ever ships with the program. That means the conversion code (today: Python) has to be **rewritten in the runtime language** before the port is usable by someone who is not a developer. Planned layers:

| Layer | Responsibility |
|-------|----------------|
| `platform` | window, input, audio device, timing (SDL3 or equivalent) |
| `render` | GPU abstraction, level/model drawing, materials, alpha layers |
| `assets` | disc image reader, `IE` container, `.NGP/.PTR/.TEX/.RTX` parsing, scene graph, cache |
| `world` | level graph, LOD by distance, collision grid, game objects |
| `game` | bike, rider, physics, AI, modes, menus, save data |

## 3. Language decision (status: **under evaluation, not decided**)

**Idea recorded from the maintainer (2026-10-01):** the runtime might be better written in Rust (or another language with strong memory management) instead of C++, and it should behave sensibly on any PC. It is early, so this is recorded to be kept in mind, not acted on yet.

**What the choice does and does not buy**

| Goal | Rust helps? | Notes |
|------|-------------|-------|
| Memory safety when parsing untrusted binary data (asset files from a disc image) | **Yes, substantially.** | This is the strongest argument: the project is mostly a parser of binary formats with offsets, pointers and relocation tables. Out-of-bounds reads are the main bug class. |
| Cross-platform builds (Linux `.deb`, Windows `.exe`) | Yes, mildly | `cargo` is simpler than CMake + system SDL3 + OpenGL headers. C++ can be fine with vcpkg/Conan. |
| "Knows what it is doing on any PC" (GPUs, drivers, memory) | **Not by itself** | That is a design property, not a language feature: capability detection, a graphics API abstraction with fallback (Vulkan/Metal/DX12/GL), a memory budget for textures and a settings auto-tuner. These must be designed in either language. |
| Ecosystem for this job | Mixed | Rust: `sdl3` bindings or `winit` + `wgpu` (portable GPU API), `glam`, `zerocopy`/`bytemuck`. C++: SDL3, OpenGL, many examples of PS2 formats. |
| Cost of switching | **Lowest right now** | The runtime is one small file. The expensive thing is the *parsers* (still in Python), and they have to be rewritten anyway. |

**Options**
1. **Stay on C++20.** Keep SDL3/OpenGL; harden parsers with bounds-checked spans, sanitizers (ASan/UBSan) and fuzzing.
2. **Rust runtime.** Rewrite `dhview` and write the new parsers directly in Rust. Prefer `wgpu` for portability and automatic backend selection.
3. **Hybrid.** Rust (or C++) core library + thin front-end. Not recommended this early: two build systems for one tiny program.

**Recommendation (maintainer assistant's view, to be confirmed by the maintainer):** decide *before* the first native parser is written, because that is the moment the language gets locked in by the amount of code. Do a time-boxed **spike**: port the current `.mdl` loader + walk mode (~200 lines) to Rust with `wgpu`, and compare (a) lines and clarity, (b) build/packaging on Linux and Windows, (c) frame time on a low-end GPU, (d) how pleasant it is to write a bounds-checked reader for the `.PTR` relocation + `.NGP` scene graph. If the spike is not clearly better, stay on C++20 with sanitizers and fuzzing. Either way the Python tools stay as the reference implementation and test oracle until the native parsers match their output byte for byte.

**Decision criteria** (so the choice is made on evidence): memory-safety of the parsers, time to a playable level, packaging effort on Windows and Linux, performance headroom on weak hardware, how easy it is for community contributors to build and contribute.

## 4. Requirements for "works well on any PC" (language-independent)

- Detect GPU/driver capabilities at start; choose a backend and texture size from them; fall back instead of crashing.
- Keep a configurable memory budget; textures are small (≤256², palettized), so keep them palettized or compressed on the GPU when possible.
- Never rely on undefined behaviour in file parsing: every offset and length read from a game file is validated against the file size before use.
- Fixed-timestep simulation separated from rendering, so frame rate does not change gameplay.
- A single `--check` mode that prints the detected hardware and the chosen settings, to make bug reports useful.

## 5. Testing strategy during any migration

Treat the Python tools as the oracle: for each level, the native parser must produce the same vertex/index/texture data (hash of the `.mdl`). Add that comparison as a CI check as soon as there is a native parser. No game data goes into the repository; the check runs locally on the contributor's own image.
