#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Parser de los hermanos del .PTS de nivel (PTS/<NIVEL>.<EXT>). Layout en docs/formats/ptsext.md.
Uso: ptsext.py ARCHIVO [out.pts]   -> resumen (y, con out.pts, los x,y,z como floats sueltos para DH_PTS de dhview)
EN: parser for the per-level sibling files of .PTS (point lists, 32 B records). Layout: docs/formats/ptsext.md."""
import struct, sys, os

MAGIC = 0x10182001
REC = {'RPL': 48}          # RPL usa registros de 48 B; el resto 32 B
POINT_EXTS = ('APT', 'HLI', 'DPT', 'FPT', 'PED', 'PKP', 'BHS', 'BRD', 'RPL')

def parse_points(d, ext):
    """-> lista de dicts {pos:(x,y,z), w, raw:bytes}. Lanza ValueError si el archivo no cuadra byte a byte."""
    if len(d) < 8: raise ValueError('archivo demasiado corto')
    magic, n = struct.unpack_from('<II', d)
    if magic != MAGIC: raise ValueError(f'magic {magic:#x}')
    stride = REC.get(ext.upper(), 32)
    if len(d) != 8 + n * stride: raise ValueError(f'tamaño {len(d)} != 8 + {n}*{stride}')
    out = []
    for i in range(n):
        o = 8 + i * stride; x, y, z, w = struct.unpack_from('<4f', d, o)
        out.append({'pos': (x, y, z), 'w': w, 'raw': d[o + 16:o + stride]})
    return out

def parse_hdt(d):
    """HDT: u32 n; n * (u32 kind, u16 start, u16 count) tabla de grupos; resto = carga sin descifrar (casi todo ceros).
    -> (grupos, carga). Los grupos son contiguos: start_i+1 == start_i+count_i."""
    n, = struct.unpack_from('<I', d)
    if 4 + 8 * n > len(d): raise ValueError('tabla fuera de rango')
    g = [struct.unpack_from('<IHH', d, 4 + 8 * i) for i in range(n)]
    pos = 0
    for _, s, c in g:
        if s != pos: raise ValueError('grupos no contiguos')
        pos += c
    return g, d[4 + 8 * n:]

if __name__ == '__main__':
    f = sys.argv[1]; ext = os.path.splitext(f)[1][1:].upper(); d = open(f, 'rb').read()
    if ext == 'HDT':
        g, rest = parse_hdt(d); print(f'{len(g)} grupos, {sum(c for _, _, c in g)} elementos, carga {len(rest)} B ({sum(1 for b in rest if b)} no nulos)')
    else:
        P = parse_points(d, ext); print(f'{len(P)} puntos')
        if len(sys.argv) > 2: open(sys.argv[2], 'wb').write(b''.join(struct.pack('<3f', *p['pos']) for p in P))
