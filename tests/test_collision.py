# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
import os, struct, sys, unittest
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools'))
import collision
from scene import Scene, B

def build():
    """Archivo sintético: 4 vértices, 1 parte (2 triángulos, 1 nodo BVH), tabla de superficies [5, 9], un nodo 0x2a."""
    n = bytearray(0x400); V = 0x100; P = 0x140; T = 0x200; N = 0x300
    for i, v in enumerate([(0, 1, 0), (10, 1, 0), (0, 2, 10), (10, 3, 10)]): struct.pack_into('<3f', n, V + 12 * i, *v)
    struct.pack_into('<I2H3ffI', n, P, V + B, 2, 1, -1., 0., 0., 100., 0)     # +0 vtx, +4 nt, +6 nn, +8 origen, +0x14 escala (sin usar aquí)
    n[P + 0x18: P + 0x18 + 14] = bytes(14)                                       # 1 nodo BVH de 14 B
    n[P + 0x18 + 14: P + 0x18 + 14 + 8] = bytes([0, 1, 2, 0, 1, 3, 2, 1])        # tris (v0 v1 v2 mat)
    struct.pack_into('<2H', n, T, 5, 9)
    struct.pack_into('<IHH', n, N, 42, 1, 0); struct.pack_into('<2I', n, N + 8, T + B, P + B)
    return bytes(n), N

class T(unittest.TestCase):
    def test_parse(self):
        n, N = build(); tris = collision.parse_node42(Scene(n), N)
        self.assertEqual(len(tris), 2)
        self.assertEqual(tris[0], ((0, 1, 0, 10, 1, 0, 0, 2, 10), 5)); self.assertEqual(tris[1][1], 9)
    def test_find(self):
        n, N = build(); self.assertEqual(collision.find_node42(Scene(n)), [N])

if __name__ == '__main__': unittest.main()
