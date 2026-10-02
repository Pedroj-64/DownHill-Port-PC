# Research workflow

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

## 6. Where to help next

- Disassemble the VU1 microcode (`.vutext` @ `0x00269d90`, `0xA140` bytes) using the PCSX2 `DisVUmicro.h` tables as the encoding reference.
- Walk the scene graph: node table at NGP offset `0xB66F00`, 0xC0-byte nodes with a 4x4 matrix.
- Find the link between textures and palettes (CLUTs in `.RTX`).
- Formats still untouched: `.PTS`, `.RST`, `.REP`, `.BNK`, `.SKX`, `.CTL`, `.APT`, `.BHS`.
