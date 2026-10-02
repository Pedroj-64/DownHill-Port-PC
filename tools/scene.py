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

import os as _os
def _parse_kinds(v):
    out = []
    for part in (v or '0').split(','):
        a, _, b = part.partition('-'); out.append((int(a), int(b or a)))
    return out
KINDS = _parse_kinds(_os.environ.get('DH_KINDS'))   # DH_KINDS="0,4000-4499": 'kinds' (hdr>>18) de las hojas de malla a incluir; por defecto sólo 0 (terreno/escenario). Los 4000-4499 son partes de los pilotos (FUN_00195b80)
def kind_ok(k): return any(a <= k <= b for a, b in KINDS)

def mul(a, b):   # 4x4 fila-por-vector (a luego b), listas de 16 floats
    return [sum(a[4*r + k] * b[4*k + c] for k in range(4)) for r in range(4) for c in range(4)]
IDENT = [1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1]
def local(S, o):
    """Matriz local de un nodo (tipo 3 = traslación, tipo 4 = matriz en +0x10 con traslación en la fila 3), o None."""
    t = S.u32(o) & 0x3f
    if t == 3: x, y, z = S.f(o + 0x10, 3); return [1,0,0,0, 0,1,0,0, 0,0,1,0, x,y,z,1]
    if t == 4: m = list(S.f(o + 0x10, 16)); m[3], m[7], m[11], m[15] = 0, 0, 0, 1; return m
def walk_payloads(S, roots=None):
    """Recorre el grafo SIN deduplicar y rinde una entrada por VISITA de cada nodo de carga útil (tipo 0; también los tipo 1 sin hijos, que son su propia carga).
    Los modelos repetidos (árboles, banderas…) salen una vez por instancia, con su matriz acumulada. La capa, el kind y el radio vienen del tipo 1 ancestro más cercano.
    dict: ptr (offset del nodo, inicio de su cadena VIF), m, sels=[(cx,cy,cz,rad,d2_max)], layer, kind, rad, root."""
    roots = roots or [S.ptr(4 + 4*i) for i in range(S.u32(0))]
    def tp(m, c): return tuple(c[0]*m[0+k] + c[1]*m[4+k] + c[2]*m[8+k] + m[12+k] for k in range(3))
    def rec(o, m, sels, path, rk, anc):
        if o is None or o in path or len(path) > 60: return
        h = S.u32(o); t = h & 0x3f
        if rk is None and path: rk = o
        if t == 1: anc = (S.u32(o + 8) >> 16, h >> 18, S.f(o + 0x1c)[0])
        if t == 0 and kind_ok(anc[1]):
            yield dict(ptr=o, m=m, sels=sels, layer=anc[0], kind=anc[1], rad=anc[2], root=rk)
        l = local(S, o); m2 = mul(l, m) if l else m
        s2 = sels + ((*tp(m, S.f(o + 0x10, 3)), S.f(o + 0x1c)[0], S.f(o + 0x24)[0]),) if t == 2 else sels
        for c in S.children(o): yield from rec(c, m2, s2, path | {o}, rk, anc)
    for r in roots:
        if S.u32(r) & 0x3f != 7: yield from rec(r, IDENT, (), frozenset(), None, (0, 0, 0.0))

def instances(S, roots=None):
    """(inicio de cadena, radio, matriz) por visita de cada carga útil (ver walk_payloads)."""
    for d in walk_payloads(S, roots): yield d['ptr'], d['rad'], d['m']

def fine_roots(S):
    """Hijos de la raíz estática con payload 1 = detalle fino del nivel (payload 2 = LOD lejano, el resto = objetos de juego)."""
    r = S.ptr(4)
    return [c for c in S.children(r) if c is not None and S.u32(c) >> 18 == 1]
class Owners:
    """Asigna cada offset del NGP al nodo de carga útil que lo contiene (mayor inicio <= offset) y da TODAS sus instancias (una por visita del grafo).
    sub=None o 'all' -> todo el grafo (reconstrucción fiel); sub='fine' -> sólo el detalle fino (fine_roots); sub=lista de nodos -> esos subárboles.
    inst[id] = dict(ptr, m, sels, layer, ...); by_ptr[ptr] = [ids]; mats/info/layer se indexan por ID de instancia."""
    def __init__(self, S, sub=None):
        import bisect; self._b = bisect
        self.inst = list(walk_payloads(S)); self.by_ptr = {}
        for k, d in enumerate(self.inst): self.by_ptr.setdefault(d['ptr'], []).append(k)
        self.mats = {k: d['m'] for k, d in enumerate(self.inst)}; self.info = {k: d for k, d in enumerate(self.inst)}; self.layer = {k: d['layer'] for k, d in enumerate(self.inst)}
        self.starts = sorted(self.by_ptr)
        roots = fine_roots(S) if sub == 'fine' else None if sub in (None, 'all') else sub
        self.allowed = None if roots is None else {p for r in roots for p, _, _ in instances(S, [r])}
        import os   # depuración: DH_ONLY / DH_SKIP = inicios de cadena (nodos de carga útil) a conservar / excluir
        if os.environ.get('DH_ONLY'): self.allowed = {int(x, 0) for x in os.environ['DH_ONLY'].split(',')}
        if os.environ.get('DH_SKIP'): self.allowed = (self.allowed if self.allowed is not None else set(self.by_ptr)) - {int(x, 0) for x in os.environ['DH_SKIP'].split(',')}
    BACKDROP = 2
    def owner(self, i):
        k = self._b.bisect_right(self.starts, i) - 1
        return self.starts[k] if k >= 0 else None
    def ok(self, i): return self.allowed is None or self.owner(i) in self.allowed
    def ids(self, i): return self.by_ptr.get(self.owner(i), ())
    def backdrop(self, k): return self.layer.get(k) == self.BACKDROP
    def apply(self, k, pos):
        m = self.mats.get(k)
        if m is None or m == IDENT: return list(pos)
        out = []
        for j in range(0, len(pos), 3):
            x, y, z = pos[j:j+3]
            out += (x*m[0] + y*m[4] + z*m[8] + m[12], x*m[1] + y*m[5] + z*m[9] + m[13], x*m[2] + y*m[6] + z*m[10] + m[14])
        return out

def leaf_info(S, roots=None):
    """Compatibilidad: una entrada por visita de carga útil (ver walk_payloads): ptr, rad, m, sels, layer, root."""
    return walk_payloads(S, roots)
