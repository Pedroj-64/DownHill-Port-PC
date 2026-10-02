#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Vista previa 2D de un .msh (proyección superior y lateral). Uso: preview_msh.py in.msh out.png"""
import struct, sys
from PIL import Image, ImageDraw
b = open(sys.argv[1], 'rb').read(); nv, ni = struct.unpack_from('<II', b, 0)
P = struct.unpack_from(f'<{3*nv}f', b, 8); I = struct.unpack_from(f'<{ni}I', b, 8 + 12*nv)
q = lambda a, f: sorted(a)[int(len(a) * f)]
X, Y, Z = P[0::3], P[1::3], P[2::3]
lim = {k: (q(a, .01), q(a, .99)) for k, a in zip('xyz', (X, Y, Z))}
S = 900; im = Image.new('RGB', (S * 2, S), (10, 12, 20)); dr = ImageDraw.Draw(im)
tri = sorted(range(0, ni, 3), key=lambda t: (P[3*I[t]+1] + P[3*I[t+1]+1] + P[3*I[t+2]+1]))
for t in tri:
    ys = (P[3*I[t]+1] + P[3*I[t+1]+1] + P[3*I[t+2]+1]) / 3
    c = int(max(50, min(255, 80 + (ys - lim['y'][0]) / (lim['y'][1] - lim['y'][0]) * 175)))
    top = [((P[3*i]-lim['x'][0])/(lim['x'][1]-lim['x'][0])*S, (P[3*i+2]-lim['z'][0])/(lim['z'][1]-lim['z'][0])*S) for i in I[t:t+3]]
    side = [(S + (P[3*i]-lim['x'][0])/(lim['x'][1]-lim['x'][0])*S, S - (P[3*i+1]-lim['y'][0])/(lim['y'][1]-lim['y'][0])*S) for i in I[t:t+3]]
    dr.polygon(top, fill=(c//2, c, c//2)); dr.polygon(side, fill=(c//2, c, c//2))
im.save(sys.argv[2])
