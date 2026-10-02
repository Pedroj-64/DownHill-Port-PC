#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Esqueleto de un modelo con nodos tipo 35 (0x23, FUN_0020effc caso 0x23) + tablas de articulación tipo 17 (0x11): pilotos (docs/formats/rider.md) y BIKESKEL.
Uso: rider_skel.py archivo.NGP [offset_del_nodo_raíz_35 ...]   -> árbol. Sin offsets: busca nodos 0x23 con u32 1 y toma como raíces los que nadie referencia."""
import struct, sys
B = 0xA00000   # base de carga del NGP en RAM (los punteros guardados valen offset + B); ver scene.py
class Joint:
    def __init__(s, d, node):
        u = lambda o: struct.unpack_from('<I', d, o)[0]
        s.node, s.table = node, u(node + 0xc) - B
        t = s.table
        s.index = struct.unpack_from('<H', d, node + 8)[0]                       # número de hueso n (orden del archivo)
        s.inv_bind = struct.unpack_from('<16f', d, node + 0x10)                  # matriz 4x4 por filas (vector fila): inversa de la pose de referencia
        s.nchild = u(t + 4)
        s.chan = [None if c == 0xffff else c for c in struct.unpack_from('<8H', d, t + 0x10)]   # [1..3] = canales de Euler X,Y,Z; raíz: [4..6] = otro triple (traslación, hipótesis)
        s.loc = struct.unpack_from('<3f', d, t + 0x30)                           # traslación local (en el marco del padre) de la pose de referencia
        s.bind_rot = struct.unpack_from('<9f', d, t + 0x80) if False else tuple(struct.unpack_from('<4f', d, t + 0x80 + 16*r)[:3] for r in range(3))   # rotación de referencia (marco espejado de la cadena izquierda: diag(-1,-1,1))
        s.flags = u(t + 0xc0); s.index2 = u(t + 0xc4)
        s.children_nodes = [u(t + 0xcc + 4*i) - B for i in range(s.nchild)]
def load(d, root):
    J = {}
    def rec(n):
        j = Joint(d, n); J[n] = j
        for c in j.children_nodes: rec(c)
        return j
    rec(root); return J
def preorder(J, root):
    out = []
    def rec(n):
        out.append(n)
        for c in J[n].children_nodes: rec(c)
    rec(root); return out
if __name__ == '__main__':
    d = open(sys.argv[1], 'rb').read()
    u = lambda o: struct.unpack_from('<I', d, o)[0]
    roots = [int(x, 0) for x in sys.argv[2:]]
    if not roots:
        nodes = [o for o in range(0, len(d) - 0x50, 16) if u(o) == 0x23 and u(o + 4) == 1]
        ref = set()
        for o in nodes:
            t = u(o + 0xc) - B
            if 0 <= t < len(d) - 0xe0: ref |= {u(t + 0xcc + 4*i) - B for i in range(min(u(t + 4), 16))}
        roots = [o for o in nodes if o not in ref]
    for r in roots:
        J = load(d, r); order = preorder(J, r)
        print(f'raíz {r:#x}: {len(J)} huesos; paleta (preorden) -> nodo n:')
        for p, n in enumerate(order):
            j = J[n]; print(f'  paleta {p:2d} (id VU {4*p:3d})  n={j.index:2d} node={n:#x} canales={j.chan[1:7]} loc={tuple(round(x, 3) for x in j.loc)} flags={j.flags:#x} hijos={[J[c].index for c in j.children_nodes]}')
