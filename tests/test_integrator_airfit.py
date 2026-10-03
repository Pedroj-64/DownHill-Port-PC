# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
import os, sys, unittest
import numpy as np
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools'))
import integrator_check as ic, integrator_airfit as af

def synth(n=20, c=1.25, cL=0.65, G=96.6):
    """Cuerpo sintético que sigue el modelo: amortiguar P y L, integrar (ic.step) con F = (0,0,-m·G)."""
    b = dict(invm=0.01, com=np.array([0, 0, -0.85]), d=np.array([0.0023, 0.0047, 0.0038]), pos=np.array([100.0, 200.0, 5000.0]), P=np.array([-370.0, 350.0, -7000.0]),
             L=np.array([-1400.0, 400.0, -1300.0]), R=np.eye(3))
    b['vel'] = b['P'] * b['invm']; b['omega'] = b['d'] * b['L']; b['node'] = b['pos'] - b['com'] @ b['R']; out = [b]
    for _ in range(n):
        pre = dict(b); pre['P'] = b['P'] * (1 - c * ic.DT); pre['L'] = b['L'] * (1 - cL * ic.DT); pre['vel'] = pre['P'] * b['invm']; pre['omega'] = b['d'] * pre['L']
        b = ic.step(pre, ic.DT, np.array([0, 0, -100 * G]), np.zeros(3)); out.append(b)
    return out

class AirFit(unittest.TestCase):
    def test_recovers_constants(self):
        c, cL, G = af.fit(synth()); self.assertAlmostEqual(c, 1.25, 3); self.assertAlmostEqual(cL, 0.65, 3); self.assertAlmostEqual(G, 96.6, 1)
    def test_errors_small_then_large_if_model_wrong(self):
        B = synth(); self.assertLess(af.errors(B, 1.25, 0.65, 96.6).max(), 1e-3); self.assertGreater(af.errors(B, 0.0, 0.0, 10.0).max(), 1e-2)

if __name__ == '__main__': unittest.main()
