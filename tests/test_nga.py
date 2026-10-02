# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
import os, struct, sys, unittest
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools'))
import nga

def clip(cid, dur, counts): return struct.pack('<2HIfI', 12, cid, 0, dur, len(counts)) + b'\0\0' + struct.pack(f'<{len(counts)}H', *counts) + bytes(8)

class T(unittest.TestCase):
    def test_two_clips(self):
        a, b = clip(12, 120., [5, 2]), clip(24, 40., [9])
        d = struct.pack('<IHHIHHI', (2 << 16) | 6040, 1, 0, 20, 7, 0, 20 + len(a)) + a + b     # ids 1 y 7 (no consecutivos)
        m, C = nga.parse_nga(d); self.assertEqual(len(C), 2)
        self.assertEqual((C[0]['dur'], C[0]['counts'], C[1]['id'], C[1]['cid'], C[1]['size']), (120., (5, 2), 7, 24, len(b)))
    def test_gap_ids(self):
        a = clip(1, 1., [1]); d = struct.pack('<IHHI', (1 << 16), 202, 0, 12) + a
        self.assertEqual(nga.parse_nga(d)[1][0]['id'], 202)

HDR = lambda t0, dt, v0, dv: struct.pack('<4f', t0, dt, v0, dv)

class Tracks(unittest.TestCase):
    def test_constant(self):
        d = b'\0' * 16 + struct.pack('<HHf', 3, 5, 2.5); tr = nga.parse_track(d, 16, None)
        self.assertEqual((tr['type'], tr['channel'], nga.sample_track(tr, 123.)), (3, 5, 2.5))
    def test_uniform_bytes_lerp(self):                       # (2,2): u8 muestras con paso dt, interpolación lineal (FUN_00268968)
        d = HDR(10., 2., 1., 0.5) + struct.pack('<3H3B', 2 | 2 << 3, 0, 3, 0, 2, 4) + b'\0'
        tr = nga.parse_track(d, 16, None)
        self.assertAlmostEqual(nga.sample_track(tr, 10.), 1.0); self.assertAlmostEqual(nga.sample_track(tr, 11.), 1.5)    # 1 + 0.5*(0..2) a mitad
        self.assertAlmostEqual(nga.sample_track(tr, 14.), 3.0); self.assertAlmostEqual(nga.sample_track(tr, 99.), 3.0); self.assertAlmostEqual(nga.sample_track(tr, -5.), 1.0)
    def test_hermite_bytes(self):                            # (1,2): claves [t8, v8, tanA, tanB]; tangentes 0 -> suavizado cúbico 3u^2-2u^3
        d = HDR(0., 1., 0., 1.) + struct.pack('<3H', 1 | 2 << 3, 0, 2) + struct.pack('<2B2H', 0, 0, 0, 0) + struct.pack('<2B2H', 2, 10, 0, 0)
        tr = nga.parse_track(d, 16, None)
        self.assertAlmostEqual(nga.sample_track(tr, 1.0), 10 * (3 * .25 - 2 * .125))                                   # u = 0.5 -> 0.5 * d
    def test_tangent_codes(self):
        self.assertAlmostEqual(nga._tan(8192), 0.5); self.assertAlmostEqual(nga._tan(16384 + 8192), 16384 / (32768 - 24576))   # 01xx...
        self.assertAlmostEqual(nga._tan(65536 - 16385), 16384 / (-32768.0 + 16385), places=3)                                  # 10xx...: pendiente negativa de módulo > 1
    def test_cubic_float(self):                              # (0,0): [t, v, c3, c2, c1] en t - t_k
        d = b'\0' * 16 + struct.pack('<3H', 0, 0, 2) + b'\0' * 0
        d = b'\0' * 16 + struct.pack('<HHH', 0, 0, 2) + b'\0\0' + struct.pack('<5f', 0, 1, 0, 0, 2) + struct.pack('<5f', 4, 9, 0, 0, 0)
        tr = nga.parse_track(d, 16, None); self.assertAlmostEqual(nga.sample_track(tr, 2.), 5.)                        # 1 + 2*2
    def test_unknown_combo(self):
        with self.assertRaises(ValueError): nga.parse_track(b'\0' * 16 + struct.pack('<HHH', 7 | 7 << 3, 0, 1), 16, None)

@unittest.skipUnless(os.path.exists(os.path.join(os.path.dirname(__file__), '..', 'unpacked/BIKE/BANIM.NGA')), 'sin datos del juego')
class RealData(unittest.TestCase):
    def test_all_files(self):
        import glob, math
        base = os.path.join(os.path.dirname(__file__), '..', 'unpacked'); n = 0
        for f in glob.glob(base + '/[BRS]*/*.NGA'):
            d = open(f, 'rb').read(); m, C = nga.parse_nga(d); self.assertEqual(len(C), m >> 16)
            for c in C:
                prev = None
                for i, (tr, (pos, sz)) in enumerate(nga.clip_tracks(d, c)):
                    need = (tr['size'] + 3) // 4 * 4 + (16 if prev and (prev['type'], prev['mode']) in nga.HAS_HDR else 0)
                    if i: self.assertEqual(sz, need)               # pistas > 0: tamaño exacto; la 0 puede llevar relleno hasta la cabecera (<= 12 B)
                    else: self.assertTrue(0 <= sz - need <= 12)
                    ts = [k[0] for k in tr['keys']]; self.assertEqual(ts, sorted(ts)); self.assertTrue(all(math.isfinite(x) for k in tr['keys'] for x in k))
                    prev = tr; n += 1
        self.assertGreater(n, 25000)

if __name__ == '__main__': unittest.main()
