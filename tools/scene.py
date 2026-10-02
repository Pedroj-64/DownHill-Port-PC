#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Recorre el grafo de escena de un .NGP descomprimido. Uso: scene.py in.NGP
Tipos de nodo (bits 0-5 de la primera palabra): 1 grupo, 2 selector/LOD, 3 traslación, 4 matriz, 6 lista, 8 enlace; el resto se omite.
Payload (bits 18+): 1/2 = malla, 4 = ..., >0x3e8 = objetos de juego. Layout inferido del recorrido del juego (ver docs)."""
import struct, sys, collections
B = 0xA00000
class Scene:
    def __init__(self, n): self.n = n
    def u32(self, o): return struct.unpack_from('<I', self.n, o)[0]
    def u16(self, o): return struct.unpack_from('<H', self.n, o)[0]
    def f(self, o, k=1): return struct.unpack_from(f'<{k}f', self.n, o)
    def ptr(self, o):
        v = self.u32(o); return v - B if B <= v < B + len(self.n) else None
    def children(self, o):
        t = self.u32(o) & 0x3f
        if t == 1: return [self.ptr(o + 0x20 + 4*i) for i in range(self.u16(o + 8))]
        if t == 2: return [self.ptr(o + 0x28 + 8*i) for i in range(self.u32(o + 4))]
        if t == 3: return [self.ptr(o + 0x1c + 4*i) for i in range(self.u32(o + 8))]
        if t == 4: return [self.ptr(o + 0x50 + 4*i) for i in range(self.u32(o + 8))]
        if t == 6: return [self.ptr(o + 0xc + 4*i) for i in range(self.n[o + 0xb])]
        if t == 8: return [self.ptr(o + 4)]
        return []
    def walk(self, o, depth=0, seen=None, trail=()):
        seen = set() if seen is None else seen
        if o is None or o in seen or depth > 40: return
        seen.add(o)
        yield o, depth, trail
        for c in self.children(o): yield from self.walk(c, depth + 1, seen, trail + (o,))
if __name__ == '__main__':
    n = open(sys.argv[1], 'rb').read(); S = Scene(n)
    roots = [S.ptr(4 + 4*i) for i in range(S.u32(0))]
    types, kinds = collections.Counter(), collections.Counter(); seen = set()
    for r in roots:
        t = S.u32(r) & 0x3f
        print(f'root {r:#x} type={t} kind={S.u32(r) >> 18:#x}')
        if t == 7: continue
        for o, d, tr in S.walk(r, seen=seen):
            h = S.u32(o); types[h & 0x3f] += 1
            if h >> 18: kinds[(h & 0x3f, h >> 18)] += 1
    print('node types', dict(types)); print('payload kinds (type,kind):count', dict(kinds.most_common(25)))

def mul(a, b):   # 4x4 fila-por-vector (a luego b), listas de 16 floats
    return [sum(a[4*r + k] * b[4*k + c] for k in range(4)) for r in range(4) for c in range(4)]
IDENT = [1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1]
def local(S, o):
    """Matriz local de un nodo (tipo 3 = traslación, tipo 4 = matriz en +0x10 con traslación en la fila 3), o None."""
    t = S.u32(o) & 0x3f
    if t == 3: x, y, z = S.f(o + 0x10, 3); return [1,0,0,0, 0,1,0,0, 0,0,1,0, x,y,z,1]
    if t == 4: m = list(S.f(o + 0x10, 16)); m[3], m[7], m[11], m[15] = 0, 0, 0, 1; return m
def instances(S, roots=None):
    """Hojas de malla (grupo con payload 0, <=1 hijo, cadena en +0x20) con su matriz acumulada. Sin dedupe global: una hoja compartida sale una vez por instancia."""
    roots = roots or [S.ptr(4 + 4*i) for i in range(S.u32(0))]
    def rec(o, m, path):
        if o is None or o in path or len(path) > 40: return
        h = S.u32(o); t = h & 0x3f
        if t == 1 and h >> 18 == 0 and S.u16(o + 8) <= 1 and S.ptr(o + 0x20): yield S.ptr(o + 0x20), S.f(o + 0x10, 4)[3], m
        l = local(S, o); m2 = mul(l, m) if l else m
        for c in S.children(o): yield from rec(c, m2, path | {o})
    for r in roots:
        if S.u32(r) & 0x3f != 7: yield from rec(r, IDENT, frozenset())

def fine_roots(S):
    """Hijos de la raíz estática con payload 1 = detalle fino del nivel (payload 2 = LOD lejano, el resto = objetos de juego)."""
    r = S.ptr(4)
    return [c for c in S.children(r) if c is not None and S.u32(c) >> 18 == 1]
class Owners:
    """Asigna cada offset del NGP a la hoja de malla que lo contiene (mayor inicio de cadena <= offset) y da su matriz.
    sub=None o 'all' -> todo el grafo (reconstrucción fiel); sub='fine' -> sólo el detalle fino (fine_roots); sub=lista de nodos -> esos subárboles."""
    def __init__(self, S, sub=None):
        import bisect; self._b = bisect
        self.mats = {}; self.info = {}
        for li in leaf_info(S): self.mats[li['ptr']] = li['m']; self.info[li['ptr']] = li
        self.starts = sorted(self.mats)
        roots = fine_roots(S) if sub == 'fine' else None if sub in (None, 'all') else sub
        self.allowed = None if roots is None else {p for r in roots for p, _, _ in instances(S, [r])}
        import os   # depuración: DH_ONLY / DH_SKIP = inicios de cadena (hojas) a conservar / excluir
        if os.environ.get('DH_ONLY'): self.allowed = {int(x, 0) for x in os.environ['DH_ONLY'].split(',')}
        if os.environ.get('DH_SKIP'): self.allowed = (self.allowed if self.allowed is not None else set(self.mats)) - {int(x, 0) for x in os.environ['DH_SKIP'].split(',')}
        self.layer = {}   # inicio de cadena -> capa de dibujo (u16 alto de la palabra +8 de la hoja): 3 = celdas de terreno, 5 = otro, 2 = telón de fondo/cielo (una por nivel)
        def rec(o, path):
            if o is None or o in path or len(path) > 40: return
            h = S.u32(o)
            if h & 0x3f == 1 and h >> 18 == 0 and S.u16(o + 8) <= 1 and S.ptr(o + 0x20): self.layer[S.ptr(o + 0x20)] = S.u32(o + 8) >> 16
            for c in S.children(o): rec(c, path | {o})
        for r in roots if roots is not None else fine_roots(S): rec(r, frozenset())
    BACKDROP = 2
    def backdrop(self, i): return self.layer.get(self.owner(i)) == self.BACKDROP
    def owner(self, i):
        k = self._b.bisect_right(self.starts, i) - 1
        return self.starts[k] if k >= 0 else None
    def ok(self, i): return self.allowed is None or self.owner(i) in self.allowed
    def apply(self, i, pos):
        o = self.owner(i); m = self.mats.get(o)
        if m is None or m == IDENT: return list(pos)
        out = []
        for j in range(0, len(pos), 3):
            x, y, z = pos[j:j+3]
            out += (x*m[0] + y*m[4] + z*m[8] + m[12], x*m[1] + y*m[5] + z*m[9] + m[13], x*m[2] + y*m[6] + z*m[10] + m[14])
        return out

def leaf_info(S, roots=None):
    """Hojas de malla con su matriz acumulada y los selectores de distancia (nodos tipo 2) de sus ancestros.
    Rinde dicts: ptr, rad, m, sels=[(cx,cy,cz,rad,d2_max)] en espacio de mundo, layer (u16 alto de +8), root (hijo de la raíz que la contiene).
    Una hoja visitada por varios padres sale una vez por camino."""
    roots = roots or [S.ptr(4 + 4*i) for i in range(S.u32(0))]
    def tp(m, c): return tuple(c[0]*m[0+k] + c[1]*m[4+k] + c[2]*m[8+k] + m[12+k] for k in range(3))
    def rec(o, m, sels, path, rk):
        if o is None or o in path or len(path) > 40: return
        h = S.u32(o); t = h & 0x3f
        if rk is None and path: rk = o
        if t == 1 and h >> 18 == 0 and S.u16(o + 8) <= 1 and S.ptr(o + 0x20):
            yield dict(ptr=S.ptr(o + 0x20), rad=S.f(o + 0x1c)[0], m=m, sels=sels, layer=S.u32(o + 8) >> 16, root=rk)
        l = local(S, o); m2 = mul(l, m) if l else m
        s2 = sels + ((*tp(m, S.f(o + 0x10, 3)), S.f(o + 0x1c)[0], S.f(o + 0x24)[0]),) if t == 2 else sels
        for c in S.children(o): yield from rec(c, m2, s2, path | {o}, rk)
    for r in roots:
        if S.u32(r) & 0x3f != 7: yield from rec(r, IDENT, (), frozenset(), None)
