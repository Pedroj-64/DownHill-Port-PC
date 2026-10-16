#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Paso 4 del loader nativo: cada textura RGBA que tools/extract_model.py escribió en el .mdl de un nivel debe poder reproducirse EXACTAMENTE (mismas dimensiones y bytes) por el decodificador nativo build/tex_check
(src/tex.hpp). El .mdl solo contiene las texturas usadas; el nativo decodifica todos los materiales, así que se exige  textures(.mdl)  ⊆  textures(nativo).  Necesita unpacked/LVL y out/maps (datos locales, no va a CI).
/ Native-loader step 4: every RGBA texture that tools/extract_model.py wrote into a level's .mdl must be reproduced EXACTLY (same size and bytes) by the native decoder build/tex_check (src/tex.hpp). The .mdl holds only the
used textures; the native one decodes every material, so we require  textures(.mdl) ⊆ textures(native). Needs unpacked/LVL and out/maps (local data, not for CI).
Uso / Usage:  python3 tools/tex_check.py [--assert] [NIVEL ...]"""
import glob, os, struct, subprocess, sys, tempfile, zlib
R = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

def mdl_textures(path):
    d = open(path, 'rb').read(); assert d[:4] in (b'DHM2', b'DHM3'), d[:4]
    n = struct.unpack_from('<I', d, 4)[0]; o = 16; out = []
    for _ in range(n):
        w, h = struct.unpack_from('<II', d, o); o += 8; out.append((w, h, zlib.crc32(d[o:o + w * h * 4]) & 0xffffffff)); o += w * h * 4
    return out

def main():
    names = [a for a in sys.argv[1:] if not a.startswith('--')] or [os.path.basename(f)[:-4] for f in sorted(glob.glob(os.path.join(R, 'unpacked/LVL/*.NGP')))]
    bad = tot = lvls = 0
    with tempfile.TemporaryDirectory() as t:
        for n in names:
            mdl = os.path.join(R, 'out/maps', n + '.mdl')
            if not os.path.exists(mdl): continue
            out = os.path.join(t, n + '.txt'); r = subprocess.run([os.path.join(R, 'build/tex_check'), os.path.join(R, 'unpacked/LVL', n), out], capture_output=True, text=True)
            if r.returncode: print('ERROR', n, r.stderr.strip()); bad += 1; continue
            nat = {tuple(int(x, 16) if i == 2 else int(x) for i, x in enumerate(l.split())) for l in open(out)}
            py = mdl_textures(mdl); miss = [x for x in py if x not in nat]; lvls += 1; tot += len(py)
            if miss: bad += 1; print('FALTAN', n, f'{len(miss)}/{len(py)} texturas del .mdl no coinciden', miss[:2])
    print(f'{lvls} niveles, {tot} texturas del .mdl comparadas, {bad} niveles con diferencias / levels, mdl textures compared, levels differing')
    if '--assert' in sys.argv and (bad or not lvls): sys.exit(1)

if __name__ == '__main__':
    main()
