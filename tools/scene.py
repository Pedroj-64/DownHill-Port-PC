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
