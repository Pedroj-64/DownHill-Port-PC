# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Fija la convención de ejes con datos reales de ALP2 (se SALTA si no hay datos del juego; nunca se versionan).
Hechos: (1) la rejilla de salida se reparte a lo largo de Y y 6 u de paso; (2) la normal media de la plataforma de salida (malla de colisión) es +Z;
(3) la línea de carrera .PTS desciende en Z (y no en Y) de salida a meta  =>  arriba = +Z en el espacio del NGP."""
import os, struct, sys, unittest
ROOT = os.path.join(os.path.dirname(__file__), '..')
sys.path.insert(0, os.path.join(ROOT, 'tools'))
NGP = os.path.join(ROOT, 'unpacked/LVL/ALP2.NGP'); PTS = os.path.join(ROOT, 'unpacked/PTS/ALP2.PTS')

@unittest.skipUnless(os.path.exists(NGP) and os.path.exists(PTS), 'sin datos del juego')
class T(unittest.TestCase):
    def test_up_axis_is_z(self):
        import markers, collision
        from scene import Scene
        S = Scene(open(NGP, 'rb').read())
        grids = [(k, o) for k, _, t, o in markers.walk_kinds(S, 8060, 8069) if t in (3, 4)]
        self.assertEqual(len(grids), 1); k, o = grids[0]
        slots = markers.start_slots(S, o, k); self.assertEqual(len(slots), 10)
        spread = [max(s[a] for s in slots) - min(s[a] for s in slots) for a in range(3)]
        self.assertGreater(spread[1], 50); self.assertLess(spread[0], 5); self.assertLess(spread[2], 5)       # a lo largo de Y, 9 pasos de 6 u
        c = [sum(s[a] for s in slots) / 10 for a in range(3)]
        # normal media de los triángulos de colisión a < 15 m de la salida: +Z (con (v2-v1)x(v0-v1))
        nz = n = 0.0
        for t, _ in collision.collision_tris(S):
            g = [sum(t[3 * i + a] for i in range(3)) / 3 for a in range(3)]
            if sum((g[a] - c[a]) ** 2 for a in range(3)) > 150 ** 2: continue
            e1 = [t[6 + a] - t[3 + a] for a in range(3)]; e2 = [t[a] - t[3 + a] for a in range(3)]
            cr = [e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0]]
            l = sum(x * x for x in cr) ** .5
            if l > 1e-9: nz += cr[2] / l; n += 1
        self.assertGreater(n, 100); self.assertGreater(nz / n, 0.3)
        d = open(PTS, 'rb').read(); N = (len(d) - 8) // 32
        R = [struct.unpack_from('<3f8h4x', d, 8 + 32 * i) for i in range(N)]; i, ch = 0, []
        while 0 <= i < N and i not in ch: ch.append(i); i = R[i][9]
        dz = R[ch[-1]][2] - R[ch[0]][2]; dy = R[ch[-1]][1] - R[ch[0]][1]
        self.assertLess(dz, -5000); self.assertLess(abs(dy), 1000)                                             # baja ~780 m en Z

if __name__ == '__main__': unittest.main()
