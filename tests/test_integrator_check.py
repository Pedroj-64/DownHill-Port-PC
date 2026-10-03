# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
import os, sys, unittest
import numpy as np
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools'))
import integrator_check as ic

INVM = 0.01; MG = ic.G / INVM   # sintético: m = 100

def res(pos=0.0, R=0.0, F=(0, 0, -MG), T=(0, 0, 0)):
    return {n.split()[0]: p for n, p, _, _ in ic.evaluate(pos, R, np.array(F, float), np.array(T, float), INVM)}

class Criteria(unittest.TestCase):
    def test_pass(self):
        self.assertTrue(all(res().values()))
    def test_pos(self):
        self.assertFalse(res(pos=2e-3)['|dpos|']); self.assertTrue(res(pos=9e-4)['|dpos|'])
    def test_rot(self):
        self.assertFalse(res(R=2e-5)['max|dR_ij|']); self.assertTrue(res(R=9e-6)['max|dR_ij|'])
    def test_fz(self):
        self.assertFalse(res(F=(0, 0, -MG * 1.01))['F_z']); self.assertTrue(res(F=(0, 0, -MG * 1.0005))['F_z'])
    def test_fxy(self):
        self.assertFalse(res(F=(MG * 2e-3, 0, -MG))['|F_xy|']); self.assertTrue(res(F=(MG * 5e-4, 0, -MG))['|F_xy|'])
    def test_torque(self):
        self.assertFalse(res(T=(0, 0, 5.0))['|T_eff|']); self.assertTrue(res(T=(0, 0, 0.5))['|T_eff|'])
    def test_exit_code(self):
        F = np.array([0, 0, -MG]); good = (0, 0, 1, 0, 0, 0, 0, F, 0, np.zeros(3), INVM); bad = (0, 0, 1, 1.0, 0, 0, 0, F, 0, np.zeros(3), INVM)
        self.assertTrue(ic.verdict([good], 'x')); self.assertFalse(ic.verdict([bad], 'x'))

if __name__ == '__main__': unittest.main()
