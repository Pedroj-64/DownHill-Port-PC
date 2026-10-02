#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Exporta los modelos de piloto de un nivel (nodos tipo 25 con kind 4000-4499, docs/formats/rider.md) a <salida>/<kind>.mdl en pose de referencia (T-pose).
Uso: export_riders.py unpacked/LVL/ALP2 out/riders   (sin extensión: .NGP/.PTR/.TEX/.RTX)"""
import sys, os, subprocess
sys.path.insert(0, os.path.dirname(__file__))
from scene import Scene
base, outdir = sys.argv[1], sys.argv[2]
d = open(base + '.NGP', 'rb').read(); S = Scene(d); seen = set(); models = {}   # kind -> (inicio de la cadena, fin = nodo selector que la sigue)
for r in [S.ptr(4 + 4*i) for i in range(S.u32(0))]:
    if S.u32(r) & 0x3f == 7: continue
    for o, _, _ in S.walk(r, seen=seen):
        h = S.u32(o)
        if h & 0x3f != 25 or not 4000 <= h >> 18 <= 4499: continue
        p = S.ptr(o + 0x2c)                     # grupo (tipo 1) con: nodo 35 (huesos), selector (tipo 2) -> cadena VIF, nodo 42
        if p is None: continue
        for sel in (c for c in S.children(p) if c and S.u32(c) & 0x3f == 2):
            lods = [q for q in (S.ptr(sel + 0x28 + 8*k) for k in range(S.u32(sel + 4))) if q]
            if lods: models.setdefault(h >> 18, (min(lods), sel))
os.makedirs(outdir, exist_ok=True)
for kind, (lo, hi) in sorted(models.items()):
    env = dict(os.environ, DH_RANGE=f'{lo:#x},{hi:#x}', DH_NOSKY='1', DH_NODOME='1')
    out = f'{outdir}/{kind}.mdl'
    r = subprocess.run([sys.executable, os.path.join(os.path.dirname(__file__), 'extract_model.py'), base, out], env=env, capture_output=True, text=True)
    print(kind, f'{lo:#x}..{hi:#x}', r.stdout.strip().splitlines()[-1] if r.stdout else r.stderr[-200:])
