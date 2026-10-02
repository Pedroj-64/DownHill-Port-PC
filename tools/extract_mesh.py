#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Extrae triángulos de un .NGP descomprimido -> .msh (u32 nv, u32 ni, floats xyz[nv], u32 idx[ni]).
Fragmento = UNPACK V3-32 (posiciones) seguido de STCYCL, V4-32 (tag GIF), S-8 (índices en orden de tira), V4-8/V4-5 (color), V2-16 (ST).
ponytail: la tira se corta por heurística de longitud de arista (el ADC real vive en el microcódigo VU1); afinar al leer el VU1."""
import struct, math, sys, statistics, os
LO, HI = float(os.environ.get('DH_MIN', 0)), float(os.environ.get('DH_MAX', 1e9))   # filtro por escala del fragmento (depuración)
sys.path.insert(0, __file__.rsplit('/', 1)[0])
from vif import iter_vif
d = open(sys.argv[1], 'rb').read(); L = len(d)
V, I = [], []; stats = dict(chunks=0, with_idx=0, no_idx=0)
i = 0x1000
while i < L - 16:
    w = struct.unpack_from('<I', d, i)[0]; cmd, num = w >> 24, (w >> 16) & 0xff
    if cmd in (0x68, 0x78) and num >= 3 and i + 4 + 12*num <= L:
        pos = list(struct.unpack_from(f'<{3*num}f', d, i + 4))
        if all(math.isfinite(f) and abs(f) < 2e5 for f in pos):
            idx = None; k = 0
            for off, nm, imm, n2, sz in iter_vif(d, i + 4 + 12*num, min(L, i + 4 + 12*num + 0x400)):
                if nm.startswith('?') or nm.startswith('MS'): break
                if nm == 'UNPACK S-8' and n2 >= 3 and idx is None: idx = list(d[off+4:off+4+n2])
                k += 1
                if k > 8: break
            stats['chunks'] += 1
            sc = max(abs(f) for f in pos)
            if idx and max(idx) < num and LO <= sc < HI:
                stats['with_idx'] += 1
                P = [tuple(pos[3*j:3*j+3]) for j in range(num)]
                e = [math.dist(P[idx[j]], P[idx[j+1]]) for j in range(len(idx)-1)]
                med = lambda j: statistics.median(e[max(0, j-4):j+6]) * 2.5 + 1e-6   # mediana local: sobrevive a reinicios de tira
                base = len(V) // 3; V += pos
                for j in range(len(idx) - 2):
                    a, b, c = idx[j], idx[j+1], idx[j+2]
                    if a == b or b == c or a == c: continue
                    if max(e[j], e[j+1], math.dist(P[a], P[c])) > med(j): continue
                    I += (base+a, base+c, base+b) if j & 1 else (base+a, base+b, base+c)
            else: stats['no_idx'] += 1
            i += 4 + 12*num; continue
    i += 4
open(sys.argv[2], 'wb').write(struct.pack('<II', len(V)//3, len(I)) + struct.pack(f'<{len(V)}f', *V) + struct.pack(f'<{len(I)}I', *I))
print(stats, len(V)//3, 'vértices', len(I)//3, 'triángulos')
