#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Malla de colisión del .NGP: nodos tipo 42 (0x2a) -> partes -> triángulos con id de superficie. Layout y citas del ELF: docs/formats/collision.md.
Uso: collision.py NIVEL.NGP [out.col]   -> resumen (y exporta triángulos: u32 n, n * (9 f32 + u16 superficie + u16 relleno))
EN: collision mesh of the .NGP: type-42 nodes -> parts -> triangles with surface id."""
import struct, sys, collections
sys.path.insert(0, __import__('os').path.dirname(__file__))
from scene import Scene, B

def parse_node42(S, o):
    """Nodo 0x2a (FUN_00216780 rama 0x2a): +4 u16 nº de partes, +8 ptr tabla de superficies (u16 por índice de material), +0xC ptr[n] partes.
    Parte (FUN_00219970): +0 ptr vértices (f32x3, índices u8), +4 u16 nº triángulos, +6 u16 nº nodos BVH, +8 f32x3 origen, +0x14 f32 escala,
    +0x18 BVH[nn] de 14 B (6 i16 caja + u16: bit0 hoja, bits1-4 nº tris, bits5-13 primer tri/hijo), luego tris[nt] de 4 B (v0,v1,v2,mat).
    -> lista de (tri(9 floats), surface_id)."""
    n = S.n; cnt = S.u16(o + 4); table = S.ptr(o + 8); out = []
    for i in range(cnt):
        pt = S.ptr(o + 0xc + 4 * i); vp = S.ptr(pt); nt, nn = struct.unpack_from('<2H', n, pt + 4)
        tb = pt + 0x18 + nn * 14
        for k in range(nt):
            a, b, c, m = n[tb + 4 * k: tb + 4 * k + 4]
            tri = struct.unpack_from('<3f', n, vp + 12 * a) + struct.unpack_from('<3f', n, vp + 12 * b) + struct.unpack_from('<3f', n, vp + 12 * c)
            out.append((tri, struct.unpack_from('<H', n, table + 2 * m)[0]))
    return out

def find_node42(S):
    """Todos los nodos 0x2a del archivo. Muchos no cuelgan del árbol que recorre scene.walk (777 en ALP2 frente a 18 alcanzables): los referencian registros
    de 0x4C B (hipótesis: instancias de objeto; cabecera u32 exacta 0x2a, nº de partes 1..64, punteros dentro del archivo, índices de vértice dentro de rango)."""
    n = S.n; out = []
    for o in range(0, len(n) - 0x20, 4):
        if S.u32(o) != 42: continue
        cnt = S.u16(o + 4)
        if not 1 <= cnt <= 64 or S.ptr(o + 8) is None or any(S.ptr(o + 0xc + 4 * i) is None for i in range(cnt)): continue
        out.append(o)
    return out

def collision_tris(S): return [t for o in find_node42(S) for t in parse_node42(S, o)]

if __name__ == '__main__':
    S = Scene(open(sys.argv[1], 'rb').read()); T = collision_tris(S)
    xs = [v for t, _ in T for v in t[0::3]]; ys = [v for t, _ in T for v in t[1::3]]; zs = [v for t, _ in T for v in t[2::3]]
    print(f'{len(T)} triángulos; x[{min(xs):.0f},{max(xs):.0f}] y[{min(ys):.0f},{max(ys):.0f}] z[{min(zs):.0f},{max(zs):.0f}]; superficies {dict(collections.Counter(s for _, s in T))}')
    if len(sys.argv) > 2: open(sys.argv[2], 'wb').write(struct.pack('<I', len(T)) + b''.join(struct.pack('<9fHH', *t, s, 0) for t, s in T))
