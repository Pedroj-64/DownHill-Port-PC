#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Extrae vértices (UNPACK V3-32 de VIF) de un .NGP descomprimido a floats crudos x,y,z. Uso: extract_points.py in.NGP out.pts
ponytail: escaneo heurístico, sin topología; sustituir por el recorrido real del grafo de escena cuando se entienda."""
import struct, math, sys
n = open(sys.argv[1], 'rb').read(); L = len(n); out = bytearray(); i = 0x1000
while i < L - 16:
    v = struct.unpack_from('<I', n, i)[0]; cmd, num = v >> 24, (v >> 16) & 0xff
    if cmd in (0x68, 0x78) and num and i + 4 + 12*num <= L:
        fl = struct.unpack_from(f'<{3*num}f', n, i + 4)
        if all(math.isfinite(f) and abs(f) < 2e5 for f in fl) and any(abs(f) > 1 for f in fl):
            out += n[i+4:i+4+12*num]; i += 4 + 12*num; continue
    i += 4
open(sys.argv[2], 'wb').write(out); print(len(out)//12, 'vértices')
