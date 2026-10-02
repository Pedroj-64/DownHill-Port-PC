# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
import math, os, struct, sys, unittest
import numpy as np
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools'))
import rider_pose, rider_skel
B = rider_skel.B

def joint(blob, node, table, index, loc, world, chans, children=()):
    """Escribe un nodo 0x23 en `node` y su tabla 0x11 en `table` (layout de docs/formats/rider.md); `world` = posición de referencia (sólo traslaciones: inv_bind = traslación(-world))."""
    inv = np.eye(4); inv[3, :3] = -np.asarray(world, float)
    blob[node:node + 0x50] = struct.pack('<IIHHI', 0x23, 1, index, 0, B + table) + inv.astype('<f4').tobytes()
    t = bytearray(0xe0); struct.pack_into('<IIIf', t, 0, 0x11, len(children), 0, 1.0)
    struct.pack_into('<8H', t, 0x10, 0xffff, *[0xffff if c is None else c for c in chans], 0xffff)
    struct.pack_into('<3f', t, 0x30, *loc); struct.pack_into('<II', t, 0xc0, 0x300, index)
    for k in range(3): struct.pack_into('<4f', t, 0x80 + 16*k, *np.eye(4)[k])
    for i, c in enumerate(children): struct.pack_into('<I', t, 0xcc + 4*i, B + c)
    blob[table:table + 0xe0] = t

class T(unittest.TestCase):
    def test_euler_is_rotation_and_pure_axes(self):                    # FUN_00227da8: filas = Rx(a)Ry(b)Rz(c)
        M = rider_pose.euler_matrix(0.3, -0.7, 1.1)[:3, :3]
        self.assertTrue(np.allclose(M @ M.T, np.eye(3), atol=1e-12) and abs(np.linalg.det(M) - 1) < 1e-12)
        c, s = math.cos(0.5), math.sin(0.5)
        self.assertTrue(np.allclose(rider_pose.euler_matrix(0, 0, 0.5)[:3, :3], [[c, -s, 0], [s, c, 0], [0, 0, 1]]))
        self.assertTrue(np.allclose(rider_pose.euler_matrix(0.5, 0, 0)[:3, :3], [[1, 0, 0], [0, c, -s], [0, s, c]]))
    def test_two_joint_chain_bind_is_identity_and_bends(self):
        blob = bytearray(0x500)
        joint(blob, 0x100, 0x1a0, 0, (0, 0, 2.0), (0, 0, 2.0), [0, 1, 2, None, None, None], children=[0x300])
        joint(blob, 0x300, 0x360, 1, (1.0, 0, 0), (1.0, 0, 2.0), [None] * 6)
        J = rider_skel.load(bytes(blob), 0x100); order = rider_skel.preorder(J, 0x100)
        self.assertTrue(all(np.allclose(m, np.eye(4), atol=1e-6) for m in rider_pose.skin_matrices(J, order, {})))   # pose de referencia = identidad
        S = rider_pose.skin_matrices(J, order, {2: math.pi / 2})                  # canal 2 = Z de la raíz: gira 90 grados toda la rama
        p = np.array([1, 0, 2.0, 1]) @ S[1]                                       # vértice del hijo en su pivote
        self.assertAlmostEqual(float(np.linalg.norm(p[:3] - [0, 0, 2.0])), 1.0, places=6)   # sigue a 1 del pivote de la raíz
        self.assertGreater(float(np.linalg.norm(p[:3] - [1, 0, 2.0])), 1.0)                # y se ha movido
if __name__ == '__main__': unittest.main()
