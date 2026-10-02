#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Extrae triángulos de un .NGP descomprimido -> .msh (u32 nv, u32 ni, floats xyz[nv], u32 idx[ni]).
Fragmento = UNPACK V3-32 (posiciones) seguido de STCYCL, V4-32 (tag GIF), S-8 (índices en orden de tira), V4-8/V4-5 (color), V2-16 (ST).
Reinicios de tira (ADC): parches STMASK + S-8 x2 sobre el slot UV (+3) del vértice marcan los 2 primeros vértices de cada tira."""
import struct, math, sys, statistics, os
LO, HI = float(os.environ.get('DH_MIN', 0)), float(os.environ.get('DH_MAX', 1e9))   # filtro por escala del fragmento (depuración)
OFF0, OFF1 = int(os.environ.get('DH_OFF0', '0'), 0), int(os.environ.get('DH_OFF1', '0x7fffffff'), 0)   # filtro por rango de offsets (depuración)
sys.path.insert(0, __file__.rsplit('/', 1)[0])
from vif import iter_vif
d = open(sys.argv[1], 'rb').read(); L = len(d)
RR = os.environ.get('DH_RADIUS')   # "rmin,rmax": sólo cadenas de hojas del grafo cuya esfera tiene ese radio (depuración)
ALLOW = None
if RR:
    from scene import Scene
    S = Scene(d); lo_r, hi_r = map(float, RR.split(',')); leaves = []; seen = set()
    for r in [S.ptr(4 + 4*k) for k in range(S.u32(0))]:
        if S.u32(r) & 0x3f == 7: continue
        for o, _, _ in S.walk(r, seen=seen):
            h = S.u32(o)
            if h & 0x3f == 1 and h >> 18 == 0 and S.u16(o + 8) <= 1 and S.ptr(o + 0x20): leaves.append((S.ptr(o + 0x20), S.f(o + 0x10, 4)[3]))
    leaves = sorted(set(leaves)); ALLOW = []
    for k, (ptr, rad) in enumerate(leaves):
        if lo_r <= rad < hi_r: ALLOW.append((ptr, leaves[k+1][0] if k + 1 < len(leaves) else L))

V, I = [], []; stats = dict(chunks=0, with_idx=0, no_idx=0, adc=0)
i = 0x1000
while i < L - 16:
    w = struct.unpack_from('<I', d, i)[0]; cmd, num = w >> 24, (w >> 16) & 0xff
    if cmd in (0x68, 0x78) and num >= 3 and i + 4 + 12*num <= L:
        pos = list(struct.unpack_from(f'<{3*num}f', d, i + 4))
        if all(math.isfinite(f) and abs(f) < 2e5 for f in pos):
            base = len(V) // 3; sc = max(abs(f) for f in pos); used = False
            idx = None; hdr = None; flag = set(); nb = 0
            def emit():
                global I
                start = 0
                for j in range(len(idx)):
                    if j in flag: start = j          # reinicio: la nueva tira empieza en j
                    if j - start < 2: continue
                    a, b, c = idx[j-2], idx[j-1], idx[j]
                    if a == b or b == c or a == c: continue
                    I += (base+a, base+c, base+b) if (j - start) & 1 else (base+a, base+b, base+c)
            end = i + 4 + 12*num
            for off, nm, imm, n2, sz in iter_vif(d, end, min(L, end + 0x4000)):
                if nm.startswith('?') or nm == 'UNPACK V3-32': break
                if nm == 'UNPACK V4-32': hdr = imm & 0x3ff; idx = None; flag = set()
                elif nm == 'UNPACK S-8' and n2 >= 3 and idx is None: idx = list(d[off+4:off+4+n2])
                elif nm == 'UNPACK S-8' and n2 <= 3 and idx is not None and hdr is not None:
                    v = ((imm & 0x3ff) - (hdr + 3)) // 3      # parche en el slot UV (+3) del vértice v: ADC en v..v+n-1
                    flag.update(range(v, v + n2))
                elif nm.startswith('MS') and idx is not None:
                    if max(idx) < num and LO <= sc < HI and OFF0 <= i < OFF1 and (ALLOW is None or any(a <= i < b for a, b in ALLOW)): emit(); used = True; stats['adc'] += len(flag); nb += 1
                    idx = None; hdr = None; flag = set()
                end = off + 4 + sz
            stats['chunks'] += 1
            if used: stats['with_idx'] += 1; V += pos
            else: stats['no_idx'] += 1
            stats['batches'] = stats.get('batches', 0) + nb
            i += 4 + 12*num; continue
    i += 4
open(sys.argv[2], 'wb').write(struct.pack('<II', len(V)//3, len(I)) + struct.pack(f'<{len(V)}f', *V) + struct.pack(f'<{len(I)}I', *I))
print(stats, len(V)//3, 'vértices', len(I)//3, 'triángulos')
