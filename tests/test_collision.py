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

def build_instance():
    """build() + nodo de traslación (100,0,0) en 0x400 + registro de rejilla en 0x500 (AABB mundo, n=2, cadena [traslación, nodo 0x2a]) + nodo 0x0a (caja de 4 planos) en 0x600."""
    n = bytearray(build()[0].ljust(0x800, b'\0'))
    struct.pack_into('<I', n, 0x400, 3); struct.pack_into('<I', n, 0x408, 1); struct.pack_into('<3f', n, 0x410, 100., 0., 0.); struct.pack_into('<I', n, 0x41c, 0x300 + B)
    struct.pack_into('<6f', n, 0x500, 100., 1., 0., 110., 3., 10.); n[0x51e] = 2; struct.pack_into('<2I', n, 0x520, 0x400 + B, 0x300 + B)
    struct.pack_into('<IHHH', n, 0x600, 10, 4, 0xFFFF, 7); struct.pack_into('<4f', n, 0x610, 0., 0., 0., 2.)
    for i, pl in enumerate([(1, 0, 0, 1), (-1, 0, 0, 1), (0, 1, 0, 1), (0, -1, 0, 1)]): struct.pack_into('<4f', n, 0x620 + 16 * i, *pl)
    return bytes(n)

class T(unittest.TestCase):
    def test_instances(self):
        S = Scene(build_instance()); inst = collision.find_instances(S, (42,))
        self.assertEqual([(l, o) for l, _, o in inst], [(0x300, 0x500)]); self.assertEqual(inst[0][1][12:15], [100., 0., 0.])
        tris = collision.instance_tris(S)
        self.assertEqual(tris[0], ((100, 1, 0, 110, 1, 0, 100, 2, 10), 5))   # primer triángulo de build() trasladado +100 en x
    def test_node10(self):
        S = Scene(build_instance()); self.assertEqual(collision.find_node10(S), [0x600])
        surf, sph, planes = collision.parse_node10(S, 0x600)
        self.assertEqual((surf, sph[3], len(planes), planes[0]), (7, 2., 4, ((1., 0., 0.), 1.)))
    def test_parse(self):
        n, N = build(); tris = collision.parse_node42(Scene(n), N)
        self.assertEqual(len(tris), 2)
        self.assertEqual(tris[0], ((0, 1, 0, 10, 1, 0, 0, 2, 10), 5)); self.assertEqual(tris[1][1], 9)
    def test_find(self):
        n, N = build(); self.assertEqual(collision.find_node42(Scene(n)), [N])

    @unittest.skipUnless(os.path.exists(os.path.join(os.path.dirname(__file__), '..', 'unpacked/LVL/MOAB.NGP')), 'sin datos descomprimidos')
    def test_real_moab(self):
        """MOAB: 1629 instancias 0x2a, 2 nodos 0x0a con registro; los triángulos transformados caben en el AABB del registro salvo 1 (docs/formats/collision.md)."""
        S = Scene(open(os.path.join(os.path.dirname(__file__), '..', 'unpacked/LVL/MOAB.NGP'), 'rb').read())
        inst = collision.find_instances(S, (42,)); self.assertEqual(len(inst), 1629); self.assertEqual(len(collision.find_node10(S)), 2)
        self.assertEqual(len(collision.find_instances(S, (10,))), 2)
        bad = 0
        for leaf, m, o in inst:
            P = [v for t, _ in collision.transform_tris(collision.parse_node42(S, leaf), m) for v in t]; bb = S.f(o, 6)
            xs, ys, zs = P[0::3], P[1::3], P[2::3]
            bad += not (min(xs) >= bb[0] - 2 and min(ys) >= bb[1] - 2 and min(zs) >= bb[2] - 2 and max(xs) <= bb[3] + 2 and max(ys) <= bb[4] + 2 and max(zs) <= bb[5] + 2)
        self.assertLessEqual(bad, 1)

    def test_node30_local(self):
        """Nodo 30 de una cadena de instancia: v' = v*s + t (t en +0x10, s en +0x20); un nodo de otro tipo no es transformación."""
        n = bytearray(0x80); struct.pack_into('<I', n, 0, 30); struct.pack_into('<3f', n, 0x10, 5., 6., 7.); struct.pack_into('<3f', n, 0x20, 2., 3., 4.); struct.pack_into('<I', n, 0x40, 1)
        S = Scene(bytes(n)); m = collision.local_inst(S, 0)
        self.assertEqual(m, [2, 0, 0, 0, 0, 3, 0, 0, 0, 0, 4, 0, 5, 6, 7, 1]); self.assertIsNone(collision.local_inst(S, 0x40))

    @unittest.skipUnless(os.path.exists(os.path.join(os.path.dirname(__file__), '..', 'unpacked/LVL/BC.NGP')), 'sin datos descomprimidos')
    def test_real_bc_node30(self):
        """BC: la instancia 0xbc14ac (cadena con un nodo 30: escala 1.1 + traslación) sólo cae en su AABB con el nodo 30 aplicado."""
        S = Scene(open(os.path.join(os.path.dirname(__file__), '..', 'unpacked/LVL/BC.NGP'), 'rb').read())
        (leaf, m, o), = [i for i in collision.find_instances(S, (42,)) if i[2] == 0xbc14ac]
        P = [v for t, _ in collision.transform_tris(collision.parse_node42(S, leaf), m) for v in t]; bb = S.f(o, 6)
        self.assertTrue(all(bb[k] - 2 <= min(P[k::3]) and max(P[k::3]) <= bb[k + 3] + 2 for k in range(3)))

if __name__ == '__main__': unittest.main()
