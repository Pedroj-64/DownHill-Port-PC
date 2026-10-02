# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
import os, struct, sys, unittest
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools'))
import markers
from scene import Scene, B

def build():
    n = bytearray(0x600); struct.pack_into('<2I', n, 0, 1, 0x100 + B)
    struct.pack_into('<I', n, 0x100, 3); struct.pack_into('<I', n, 0x108, 1); struct.pack_into('<I', n, 0x11c, 0x200 + B)   # raíz tipo 3 con 1 hijo
    struct.pack_into('<2I', n, 0x200, 11 | (8050 << 18) | (7 << 7), 0x300 + B)                                              # nodo 11, kind 8050, idx 7
    struct.pack_into('<4f', n, 0x310, 0, 0, 1, -5)
    struct.pack_into('<I', n, 0x400, 4 | (8066 << 18)); struct.pack_into('<16f', n, 0x410, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 100, 200, 300, 1)
    return Scene(bytes(n))

class T(unittest.TestCase):
    def test_plane(self):
        self.assertEqual(markers.gate_planes(build()), [(8050, 7, (0, 0, 1, -5))])
    def test_start_slots_wide(self):
        s = markers.start_slots(build(), 0x400, 8066)     # identidad + traslación: 10 plazas, 1ª mitad x = 3, 9, 15…; 2ª x = -3, -9…
        self.assertEqual(len(s), 10); self.assertEqual(s[0], (103, 209.7, 304)); self.assertEqual(s[1][0], 109); self.assertEqual(s[5][0], 97); self.assertEqual(s[6][0], 91)

if __name__ == '__main__': unittest.main()
