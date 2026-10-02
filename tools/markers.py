#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Nodos de juego tipo 11 / 3 / 4 con 'kind' (hdr>>18) del .NGP: planos de control (kinds 8050-8052) y rejillas de salida (8060-8068).
Citas del ELF y hallazgos: docs/formats/markers.md.  Uso: markers.py NIVEL.NGP [LINEA.pts out_overlay.pts out.gates]
EN: gate planes (kinds 8050-8052, FUN_001a33d0) and start grids (kinds 8060-8068, FUN_001a3480) of the .NGP game-object nodes."""
import struct, sys, math
sys.path.insert(0, __import__('os').path.dirname(__file__))
from scene import Scene

# FUN_001a3480: kind -> nº de plazas de la rejilla de salida
START_SLOTS = {8060: 12, 8061: 4, 8062: 6, 8063: 6, 8064: 8, 8065: 6, 8066: 10, 8068: 2}

def walk_kinds(S, lo, hi):
    """Nodos con lo <= hdr>>18 < hi alcanzables desde las raíces (excepto tabla tipo 7) -> (kind, índice (hdr>>7 & 0x7ff), tipo, offset)."""
    seen = set(); out = []
    for i in range(S.u32(0)):
        r = S.ptr(4 + 4 * i)
        if S.u32(r) & 0x3f == 7: continue
        for o, d, tr in S.walk(r, seen=seen):
            h = S.u32(o)
            if lo <= h >> 18 < hi: out.append((h >> 18, (h >> 7) & 0x7ff, h & 0x3f, o))
    return sorted(out)

def gate_planes(S):
    """Kinds 8050/8051/8052 (FUN_00159910 -> FUN_001a33d0): nodo tipo 11, destino = ptr en +4, vec4 en destino+0x10 = (nx, ny, nz, d).
    Plano CONFIRMADO por el consumidor FUN_001a2738: distancia = FUN_002279e8(plano, pos) = n.p - d (positiva = ya cruzado).
    8051 (hdr&0xfffc0000 == 0x7dcc0000) marca flag[idx]=1 en el gestor (puerta con bonus). -> lista (kind, idx, (nx,ny,nz,d))"""
    out = []
    for k, idx, t, o in walk_kinds(S, 8050, 8053):
        if t == 11 and S.ptr(o + 4) is not None: out.append((k, idx, S.f(S.ptr(o + 4) + 0x10, 4)))
    return out

def start_slots(S, o, kind):
    """FUN_001a3480: posiciones de salida de una rejilla (nodo tipo 3: sólo traslación en +0x10; tipo 4: matriz 3x3 en +0x10/+0x20/+0x30 y traslación en +0x40)."""
    n = START_SLOTS.get(kind, 0); t = S.u32(o) & 0x3f
    wide = kind in (8064, 8065, 8066)   # fila a lo ancho: ±6 en x; el resto: ±6 en y
    A, B = ([3.0, 9.7, 4.0], [-3.0, 9.7, 4.0]) if wide else ([5.0, 3.0, 4.0], [5.0, -3.0, 4.0])
    M = S.f(o + 0x10, 16) if t == 4 else None; T = list(M[12:15]) if t == 4 else list(S.f(o + 0x10, 3)); out = []
    for i in range(n):
        first = i < n // 2
        v = A if first else B   # ambas ramas (tipo 3 y 4): 1ª mitad usa A y avanza +6, 2ª usa B y retrocede 6
        out.append(tuple(sum(v[k] * M[4 * k + c] for k in range(3)) + T[c] for c in range(3)) if t == 4 else tuple(v[c] + T[c] for c in range(3)))
        step = 6.0 if first else -6.0
        ax = 0 if wide else 1
        v[ax] += step
    return out

def write_gates(S, path):
    """.gates: u32 n; n * (u32 kind, u32 idx, f32 nx, ny, nz, d) en espacio NGP (Z arriba). Lo leen src/gates.hpp y tests/ride_demo.cpp."""
    P = gate_planes(S)
    open(path, 'wb').write(struct.pack('<I', len(P)) + b''.join(struct.pack('<II4f', k, i, *pl) for k, i, pl in P)); return len(P)

if __name__ == '__main__':
    S = Scene(open(sys.argv[1], 'rb').read()); P = gate_planes(S)
    print(f'{len(P)} planos de control: kinds', sorted({k for k, _, _ in P}), 'índices', sorted(i for _, i, _ in P)[:8], '...')
    grids = [(k, o) for k, _, t, o in walk_kinds(S, 8060, 8069) if t in (3, 4)]
    for k, o in grids: print(f'rejilla de salida kind {k} @ {o:#x}: {START_SLOTS.get(k, 0)} plazas, primera {tuple(round(c) for c in start_slots(S, o, k)[0])}')
    if len(sys.argv) > 4: print(write_gates(S, sys.argv[4]), 'planos ->', sys.argv[4])
    if len(sys.argv) > 3:
        pts = [c for k, o in grids for p in start_slots(S, o, k) for c in p]
        open(sys.argv[3], 'wb').write(struct.pack(f'<{len(pts)}f', *pts)); print(len(pts) // 3, 'puntos ->', sys.argv[3])
