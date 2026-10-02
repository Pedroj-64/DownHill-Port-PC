#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Vuelca el grafo de escena de un .NGP a un HTML con árbol plegable. Uso: scene_html.py in.NGP out.html
Sólo estructura (tipos, kinds, capas, nº de hijos); no contiene datos del juego. / Structure only, no game data."""
import sys, json, os
sys.path.insert(0, os.path.dirname(__file__))
from scene import Scene
S = Scene(open(sys.argv[1], 'rb').read()); seen = {}
def node(o, depth=0):
    h = S.u32(o); t = h & 0x3f; kind = h >> 18
    d = dict(o=hex(o), t=t, k=kind)
    if t == 1: d['l'] = S.u32(o + 8) >> 16; d['rad'] = round(S.f(o + 0x1c)[0], 1)
    if t == 2: d['rad'] = round(S.f(o + 0x1c)[0], 1)
    if o in seen: seen[o] += 1; d['ref'] = 1; return d
    seen[o] = 1; ch = [c for c in S.children(o) if c is not None] if depth < 60 else []
    if ch: d['c'] = [node(c, depth + 1) for c in ch]
    return d
roots = [S.ptr(4 + 4*i) for i in range(S.u32(0))]
tree = dict(o='NGP', t=-1, k=0, c=[node(r) for r in roots])
vis = {hex(o): v for o, v in seen.items() if v > 1}
html = open(os.path.join(os.path.dirname(__file__), 'scene_html.tpl')).read()
open(sys.argv[2], 'w').write(html.replace('__TREE__', json.dumps(tree, separators=(',', ':'))).replace('__VIS__', json.dumps(vis)).replace('__NAME__', os.path.basename(sys.argv[1])))
