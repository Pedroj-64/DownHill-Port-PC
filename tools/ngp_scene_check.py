#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Paso 2 del loader nativo: el recorrido de build/ngp_scene_check debe dar EXACTAMENTE las raíces, tipos de nodo y «kinds» de tools/scene.py en los niveles. Necesita unpacked/LVL (datos locales, no va a CI).
/ Native-loader step 2: build/ngp_scene_check must give EXACTLY the roots, node types and kinds of tools/scene.py on the levels. Needs unpacked/LVL (local data, not for CI).
Uso / Usage:  python3 tools/ngp_scene_check.py [--assert] [NIVEL ...]   (sin niveles = todos / no levels = all)"""
import collections, glob, os, subprocess, sys
R = os.path.dirname(os.path.dirname(os.path.abspath(__file__))); sys.path.insert(0, os.path.join(R, 'tools'))
from scene import Scene

def python_side(path):
    n = open(path, 'rb').read(); S = Scene(n)
    roots = [S.ptr(4 + 4 * i) for i in range(S.u32(0))]; roots = [r for r in roots if r is not None]
    types, kinds, seen, vis = collections.Counter(), collections.Counter(), set(), 0
    for r in roots:
        if S.u32(r) & 0x3f == 7: continue
        for o, d, tr in S.walk(r, seen=seen):
            h = S.u32(o); types[h & 0x3f] += 1; vis += 1
            if h >> 18: kinds[(h & 0x3f, h >> 18)] += 1
    return roots, vis, types, kinds

def native_side(path):
    o = subprocess.run([os.path.join(R, 'build/ngp_scene_check'), path], capture_output=True, text=True).stdout.splitlines()
    roots = [int(x) for x in o[0].split()[1:]]; vis = int(o[1].split()[1])
    types = collections.Counter({int(a): int(b) for a, b in (x.split(':') for x in o[2].split()[1:])})
    kinds = collections.Counter({tuple(int(y) for y in a.split('/')): int(b) for a, b in (x.split(':') for x in o[3].split()[1:])})
    return roots, vis, types, kinds

def main():
    names = [a for a in sys.argv[1:] if not a.startswith('--')]
    files = [os.path.join(R, 'unpacked/LVL', n + '.NGP') for n in names] if names else sorted(glob.glob(os.path.join(R, 'unpacked/LVL/*.NGP')))
    bad = 0
    for f in files:
        a, b = python_side(f), native_side(f)
        if a != b: bad += 1; print('DISTINTO', os.path.basename(f), 'py:', a[1], 'nativo:', b[1])
    print(f'{len(files) - bad}/{len(files)} niveles idénticos (raíces, tipos, kinds) / identical'); 
    if '--assert' in sys.argv and bad: sys.exit(1)

if __name__ == '__main__':
    main()
