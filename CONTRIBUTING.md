<!-- SPDX-FileCopyrightText: 2026 Pedro Soto -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Contributing to DownHill-Port-PC

Thanks for helping! This is a community project; every contribution is welcome.

## Hard rules (non-negotiable)
1. **Never commit game data**: no ISO, executable, textures, models, audio, video, save files, Ghidra projects or decompiler output. Not even small samples.
2. **No copied code.** Describe formats and behaviour in your own words in `docs/`. Do not paste decompiled functions.
3. **No personal data or secrets** in commits (paths, tokens, emails in files).

## Setup
```sh
git config core.hooksPath .githooks   # local guard: blocks the mistakes above before they reach history
tools/guard.sh                        # same checks over the whole tree (CI runs this too)
```

## Tips for a clean history
- Use your GitHub `noreply` address for commits if you don't want your email public: `git config user.email "ID+user@users.noreply.github.com"`.
- Keep pull requests small and focused; mention which game release (PAL/NTSC) you tested.

## License
By contributing you agree your work is licensed under GPL-3.0-or-later.
