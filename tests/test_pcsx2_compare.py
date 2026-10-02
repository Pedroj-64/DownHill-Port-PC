# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
import os, sys, tempfile, unittest
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools', 'pcsx2'))
import compare_trace as ct

LOG = """# ejemplo
SEG 1 0 0 10  0 0 -10  1
HIT 1 0.45 0 0 1  0 0 1  7
SEG 2 5 5 5  6 6 6  0
NOHIT 2
SEG 3 1 1 10  1 1 -10  1
HIT 3 -3.4028235e38 1 1 0  0 0 1  7
"""

class T(unittest.TestCase):
    def setUp(self):
        self.f = tempfile.NamedTemporaryFile('w', suffix='.log', delete=False); self.f.write(LOG); self.f.close()
    def tearDown(self): os.unlink(self.f.name)
    def test_parse(self):
        segs, res = ct.parse(self.f.name); self.assertEqual(len(segs), 3); self.assertEqual(res[2][0], 'NOHIT'); self.assertEqual(res[1][1][7], 7)
    def test_match(self):
        segs, res = ct.parse(self.f.name)
        ours = [('HIT', [0.4505, 0, 0, 1, 0, 0, 1, 7]), ('NOHIT', None), ('HIT', [-3.4028235e38, 1, 1, 0, 0, 0, 1, 7])]
        self.assertEqual(ct.compare(segs, res, ours), [])
    def test_detects(self):
        segs, res = ct.parse(self.f.name)
        ours = [('HIT', [0.6, 0, 0, 1, 1, 0, 0, 9]), ('HIT', [0.5, 0, 0, 0, 0, 0, 1, 0]), ('NOHIT', None)]
        why = [w for _, w in ct.compare(segs, res, ours)]
        self.assertTrue(any('frac' in w for w in why)); self.assertTrue(any('normal' in w for w in why)); self.assertTrue(any('surface' in w for w in why))
        self.assertTrue(any('juego=NOHIT' in w for w in why)); self.assertTrue(any('solape' in w or 'juego=HIT' in w for w in why))
    def test_group_takes_best_of_call(self):
        segs = [(1, [0, 0, 0, 0, 0, 1, 1]), (1, [0, 0, 0, 0, 0, 2, 1])]; game = {1: ('HIT', [0.2, 0, 0, 0, 0, 0, 1, 5])}
        ours = [('HIT', [0.7, 0, 0, 0, 0, 0, 1, 5, 3]), ('HIT', [0.2, 0, 0, 0, 0, 0, 1, 5, 9])]    # el motor devuelve el de menor fracción
        self.assertEqual(ct.compare(segs, game, ours), [])
        self.assertEqual(ct.best(ours)[1][8], 9)

if __name__ == '__main__': unittest.main()
