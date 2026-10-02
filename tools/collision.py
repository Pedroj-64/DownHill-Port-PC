#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Malla de colisión del .NGP: nodos tipo 42 (0x2a) -> partes -> triángulos con id de superficie. Layout y citas del ELF: docs/formats/collision.md.
Uso: collision.py NIVEL.NGP [out.col]   -> resumen (y exporta triángulos: u32 n, n * (9 f32 + u16 superficie + u16 relleno))
EN: collision mesh of the .NGP: type-42 nodes -> parts -> triangles with surface id."""
import struct, sys, collections, math
import numpy as np
sys.path.insert(0, __import__('os').path.dirname(__file__))
from scene import Scene, B, local, mul, IDENT

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

def parse_node10(S, o):
    """Nodo 0x0a = poliedro convexo en espacio local (FUN_00216780 rama 10 -> FUN_0021a4d8): +4 u16 nº de planos, +6 u16 0xFFFF, +8 u16 id de superficie
    (directo, sin tabla: `param_8[3] = (short)node[2]`), +0x10 esfera envolvente (cx,cy,cz,r) f32 (hipótesis: igual que en 0x2a), planos desde +0x20 de 16 B (nx,ny,nz,w):
    dentro = n.p <= w (+radio al barrer; FUN_0021a4d8: fVar9 = w + r - n.p0, fuera si < 0). -> (superficie, esfera, [(n, w)])."""
    n = S.n; cnt = S.u16(o + 4)
    return S.u16(o + 8), struct.unpack_from('<4f', n, o + 0x10), [(struct.unpack_from('<3f', n, o + 0x20 + 16 * i), S.f(o + 0x2c + 16 * i)[0]) for i in range(cnt)]

def find_node10(S):
    """Nodos 0x0a del archivo: cabecera con los 6 bits bajos = 10, +6 = 0xFFFF, 1..64 planos de normal unitaria. Se barre de 4 en 4 B; los 4 nodos de los 54 niveles están alineados a 16 B (MOAB/MOAB2)."""
    n = S.n; out = []
    for o in range(0, len(n) - 0x40, 4):
        if S.u32(o) & 0x3f != 10 or S.u16(o + 6) != 0xFFFF: continue
        cnt = S.u16(o + 4)
        if not 1 <= cnt <= 64 or o + 0x20 + 16 * cnt > len(n): continue
        if all(abs(math.sqrt(sum(c * c for c in S.f(o + 0x20 + 16 * i, 3))) - 1) < 1e-3 for i in range(cnt)): out.append(o)
    return out

def local_inst(S, p):
    """Matriz local de un nodo de la cadena de un registro de instancia: tipos 3/4 como scene.local y además el tipo 30 (0x1e): traslación f32x3 en +0x10 y escala f32x3 en +0x20,
    v' = v*s + t (hipótesis: nodo 'objeto'; cita FUN_xxxx pendiente. Evidencia: con él 40 de los 42 registros con un nodo 30 que no encajaban en su AABB pasan a encajar; ver docs/formats/collision.md)."""
    if S.u32(p) & 0x3f == 30:
        sx, sy, sz = S.f(p + 0x20, 3); x, y, z = S.f(p + 0x10, 3)
        return [sx,0,0,0, 0,sy,0,0, 0,0,sz,0, x,y,z,1]
    return local(S, p)

def find_instances(S, kinds=(42, 10)):
    """Registros de objeto de la rejilla (cadena raíz->hoja): AABB mundo (6 f32), u32 0, u16 flags, u8 n (+0x1e), u8 extra, y n punteros desde +0x20; el último es la hoja.
    La transformación de la instancia es el producto de las matrices (tipos 3/4, como scene.local) de los nodos de la cadena; la hoja 0x2a/0x0a está en espacio local.
    Evidencia (docs/formats/collision.md): el AABB del registro contiene los triángulos transformados en 1788/1789 (ALP2), 859/859 (ALPINEMX) y 1628/1629 (MOAB); sin transformar, 1.
    -> lista de (offset hoja, matriz 4x4 como 16 floats, offset registro), sin duplicados (hoja, matriz)."""
    n = S.n; L = len(n); w = np.frombuffer(n, '<u4', count=L // 4); seen = set(); out = []
    for k in np.nonzero(w[6:] == 0)[0]:
        o = int(k) * 4
        if o + 0x24 > L: continue
        cnt = n[o + 0x1e]
        if not 1 <= cnt <= 255 or o + 0x20 + 4 * cnt > L: continue
        ps = [S.ptr(o + 0x20 + 4 * i) for i in range(cnt)]
        if any(p is None for p in ps) or S.u32(ps[-1]) & 0x3f not in kinds: continue
        bb = S.f(o, 6)
        if not (all(math.isfinite(v) and abs(v) < 1e6 for v in bb) and bb[0] <= bb[3] and bb[1] <= bb[4] and bb[2] <= bb[5]): continue
        m = IDENT
        for p in ps[:-1]:
            l = local_inst(S, p)
            if l: m = mul(l, m)
        key = (ps[-1], tuple(round(x, 4) for x in m))
        if key in seen: continue
        seen.add(key); out.append((ps[-1], m, o))
    return out

def transform_tris(tris, m):
    """Triángulos (9 floats, surf) -> mundo con m (fila-por-vector, traslación en m[12:15]); las matrices del juego son rotaciones puras (det 1, filas unitarias en los 3 niveles medidos), así que el sentido se conserva."""
    R = np.array(m).reshape(4, 4); t = R[3, :3]; R = R[:3, :3]
    return [(tuple((np.asarray(tr, dtype=np.float64).reshape(3, 3) @ R + t).ravel()), sf) for tr, sf in tris]

ORPHAN_MIN = 50.0   # un nodo 0x2a sin registro de rejilla se conserva sin transformar si su centroide está a >= 50 u del origen (geometría de mundo); si no, es geometría local que nada coloca

def instance_tris(S):
    """Colisión estática con todas las instancias de nodos 0x2a transformadas a mundo (un nodo compartido sale una vez por instancia). Los nodos 0x2a SIN registro de rejilla: los que tienen el centroide
    en el origen (<50 u) son geometría local que nada coloca y se descartan; el resto (p. ej. 81 triángulos en ALP2 cerca de (-1680,-145,4758)) ya están en coordenadas de mundo y se conservan tal cual
    (hipótesis: los coloca el árbol de escena o el cargador; sin ellos la esfera de la demo atraviesa la malla visual). El 0x0a no genera triángulos (planos: parse_node10)."""
    out = []; leaves = set()
    for leaf, m, _ in find_instances(S, (42,)): out += transform_tris(parse_node42(S, leaf), m); leaves.add(leaf)
    for o in find_node42(S):
        if o in leaves: continue
        T = parse_node42(S, o)
        if T and np.linalg.norm(np.mean([np.asarray(t, dtype=np.float64).reshape(3, 3).mean(0) for t, _ in T], axis=0)) >= ORPHAN_MIN: out += T
    return out

def collision_tris(S): return [t for o in find_node42(S) for t in parse_node42(S, o)]

if __name__ == '__main__':
    inst = '--instances' in sys.argv; sys.argv = [a for a in sys.argv if a != '--instances']   # --instances: todas las instancias transformadas (instance_tris) en vez de un triángulo-juego por nodo
    S = Scene(open(sys.argv[1], 'rb').read()); T = instance_tris(S) if inst else collision_tris(S)
    xs = [v for t, _ in T for v in t[0::3]]; ys = [v for t, _ in T for v in t[1::3]]; zs = [v for t, _ in T for v in t[2::3]]
    print(f'{len(T)} triángulos; x[{min(xs):.0f},{max(xs):.0f}] y[{min(ys):.0f},{max(ys):.0f}] z[{min(zs):.0f},{max(zs):.0f}]; superficies {dict(collections.Counter(s for _, s in T))}')
    if len(sys.argv) > 2: open(sys.argv[2], 'wb').write(struct.pack('<I', len(T)) + b''.join(struct.pack('<9fHH', *t, s, 0) for t, s in T))
