#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Exporta cada textura de un .TEX como PNG en gris (T8 deshecho del swizzle GS). Uso: tex_export.py in.TEX outdir
Las dims del registro son las de la subida CT32; la textura real es T8 con el doble en cada eje."""
import struct, sys, os
sys.path.insert(0, os.path.dirname(__file__))
from gs import upload32, read8
from PIL import Image
d = open(sys.argv[1], 'rb').read(); out = sys.argv[2]; os.makedirs(out, exist_ok=True)
off = struct.unpack_from('<I', d, 4)[0] * 16; n = 0
while True:
    nxt, h, lo, hi = struct.unpack_from('<IIII', d, off)
    w, ht = 1 << (hi >> 8 & 15), 1 << (hi >> 12 & 15)
    if hi & 0x3f == 0 and w >= 8 and ht >= 8:
        raw = d[off + 0x80: off + 0x80 + w * ht * 4]
        try: Image.frombytes('L', (w*2, ht*2), read8(upload32(raw, w, ht), w*2, ht*2)).save(f'{out}/{lo & 0xffff:04d}.png')
        except Exception as e: print('skip', lo & 0xffff, e)
    n += 1
    if nxt == 0: break
    off += (nxt & ~3) * 4
print(n, 'texturas')
