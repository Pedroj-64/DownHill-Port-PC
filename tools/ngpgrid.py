#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Nodos raíz tipo 15 (rejilla de objetos) y 45 (sondas de luz) del .NGP. Layout: docs/formats/ngp-grid-probes.md (de FUN_00216ca8 / FUN_00226b08).
Uso: ngpgrid.py NIVEL.NGP   -> resumen. EN: root node types 15 (object grid) and 45 (light probes) of the .NGP."""
import struct, sys
BASE = 0xA00000   # las direcciones del NGP son de carga (base de .PTR)

def parse_grid(n, o):
    """-> dict(w,h,cell,x0,z0,x1,z1,cells=[(start,count)]). start = u32>>10 (índice acumulado), count = u32&1023."""
    t, w, h = struct.unpack_from('<IHH', n, o)
    if t & 0x3f != 15: raise ValueError('no es un nodo tipo 15')
    cell, x0, z0, x1, z1 = struct.unpack_from('<5f', n, o + 8)
    raw = struct.unpack_from(f'<{w * h}I', n, o + 0x30)
    return dict(w=w, h=h, cell=cell, x0=x0, z0=z0, x1=x1, z1=z1, cells=[(v >> 10, v & 1023) for v in raw], size=0x30 + 4 * w * h)

def parse_probes(n, o):
    """-> dict(origin, scale, probes=[(x,y,z,r,g,b)]) con r,g,b en 0..1.94 (1.0 = 16/16)."""
    t, cnt = struct.unpack_from('<II', n, o)
    if t & 0x3f != 45: raise ValueError('no es un nodo tipo 45')
    origin = struct.unpack_from('<3f', n, o + 8); scale, = struct.unpack_from('<f', n, o + 0x14)
    P = []
    for i in range(cnt):
        x, y, z, c = struct.unpack_from('<3hH', n, o + 0x18 + 8 * i)
        P.append((x, y, z, (c >> 1 & 31) / 16, (c >> 6 & 31) / 16, (c >> 11) / 16))
    return dict(origin=origin, scale=scale, probes=P, size=0x18 + 8 * cnt)

if __name__ == '__main__':
    n = open(sys.argv[1], 'rb').read()
    for i in range(struct.unpack_from('<I', n)[0]):
        r = struct.unpack_from('<I', n, 4 + 4 * i)[0] - BASE; t = struct.unpack_from('<I', n, r)[0] & 0x3f
        if t == 15: g = parse_grid(n, r); print(f"rejilla {g['w']}x{g['h']} celda {g['cell']}: {sum(c for _, c in g['cells'])} entradas, {g['size']} B")
        if t == 45: p = parse_probes(n, r); print(f"sondas: {len(p['probes'])}, {p['size']} B")
