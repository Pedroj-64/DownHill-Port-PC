# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Instanciación del grafo: un modelo (nodo de carga útil tipo 0) colocado por varios nodos tipo 3/4 debe salir UNA VEZ POR VISITA con su matriz (árboles, banderas)."""
import os, struct, sys, unittest
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools'))
import scene
from scene import Scene, B

def build():
    n = bytearray(0x400); P = lambda a, fmt, *v: struct.pack_into(fmt, n, a, *v)
    P(0, '<2I', 1, 0x100 + B)                                              # una raíz
    P(0x100, '<III', 1, 0, 3)                                              # grupo tipo 1 con 3 hijos (u16 @+8 = 3, capa 0)
    P(0x120, '<3I', 0x200 + B, 0x240 + B, 0x280 + B)
    P(0x200, '<IIII', 3, 0, 1, 0); P(0x210, '<3f', 10., 0., 0.); P(0x21c, '<I', 0x300 + B)     # tipo 3: traslación (10,0,0) -> modelo 0x300
    P(0x240, '<IIII', 3, 0, 1, 0); P(0x250, '<3f', 0., 20., 0.); P(0x25c, '<I', 0x300 + B)     # tipo 3: traslación (0,20,0) -> el MISMO modelo
    P(0x280, '<IIII', 4, 0, 1, 0); P(0x290, '<16f', 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 30., 1.); P(0x2d0, '<I', 0x300 + B)  # tipo 4: traslación (0,0,30)
    P(0x300, '<I', 0)                                                      # el modelo: nodo tipo 0
    return Scene(bytes(n))

class T(unittest.TestCase):
    def test_one_visit_per_placement(self):
        S = build(); V = list(scene.walk_payloads(S))
        self.assertEqual(len(V), 3); self.assertTrue(all(v['ptr'] == 0x300 for v in V))
        self.assertEqual(sorted(tuple(v['m'][12:15]) for v in V), [(0., 0., 30.), (0., 20., 0.), (10., 0., 0.)])
    def test_owners_multi_instance(self):
        S = build(); O = scene.Owners(S)
        ids = O.ids(0x320)                                                # un offset posterior a 0x300 pertenece a ese modelo
        self.assertEqual(len(ids), 3); self.assertEqual(O.apply(ids[0], [1., 2., 3.]) != [1., 2., 3.], True)
        self.assertEqual(sorted(O.apply(k, [0., 0., 0.]) for k in ids)[0], [0., 0., 30.])

if __name__ == '__main__': unittest.main()
