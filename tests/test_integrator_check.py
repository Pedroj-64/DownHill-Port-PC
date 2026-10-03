# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
import contextlib, io, os, sys, unittest
import numpy as np
sys.path.insert(0, os.path.dirname(__file__)); sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools'))
import integrator_check as ic, integrator_airfit as af
from test_integrator_airfit import synth

def report(B, B2=None):
    with contextlib.redirect_stdout(io.StringIO()) as out: _, ok = af.report(B, B2 or B, 'x')
    return ok, out.getvalue()

class Levels(unittest.TestCase):
    def test_model_passes(self):
        ok, out = report(synth()); self.assertTrue(ok); self.assertIn('NO validadas', out)       # procedencia pendiente: nunca "validada"
    def test_pos_fails(self):
        B = synth(); B2 = [dict(b) for b in B]; B2[-1]['pos'] = B2[-1]['pos'] + np.array([0, 0, 0.05]); self.assertFalse(report(B, B2)[0])
    def test_rot_fails(self):
        B = synth(); B2 = [dict(b) for b in B]; R = B2[-1]['R'].copy(); R[0, 0] += 1e-5; B2[-1]['R'] = R; self.assertFalse(report(B, B2)[0])
    def test_momentum_fails(self):
        B = synth(); B2 = [dict(b) for b in B]; B2[-1]['P'] = B2[-1]['P'] * (1 + 1e-5); self.assertFalse(report(B, B2)[0])
        B3 = [dict(b) for b in B]; B3[-1]['L'] = B3[-1]['L'] * (1 + 1e-5); self.assertFalse(report(B, B3)[0])
    def test_thresholds_are_3x_floor(self):
        B = synth(); self.assertTrue(np.allclose(af.thresholds(B), 3 * af.floors(B)))
    def test_pair_assert_rejected(self):
        sys.argv = ['x', 'a.p2s', 'b.p2s', '--assert']
        with self.assertRaises(SystemExit) as e: ic.main()
        self.assertIn('--assert necesita', str(e.exception))

if __name__ == '__main__': unittest.main()
