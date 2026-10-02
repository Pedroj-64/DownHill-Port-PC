# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Pruebas sintéticas del lector de savestates (sin savestates reales: esos nunca entran al repo)."""
import os, struct, sys, tempfile, unittest, zipfile
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools'))
import p2s

def fake_state(path):
    ee = bytearray(0x800000); P = lambda a, fmt, *v: struct.pack_into(fmt, ee, a, *v)
    P(p2s.RIDER_COUNT_ADDR, '<I', 1); r = p2s.RIDER_BASE; node = 0x7A0000
    P(r + 0x792C, '<I', node); P(r + 0x7A5C, '<I', 1); P(node + 0x10, '<3f', 10., 20., 30.)
    for k, row in enumerate(((1, 0, 0), (0, 1, 0), (0, 0, 1))): P(node + 0x40 + 16 * k, '<3f', *row)
    phys = r + p2s.PHYS_OFF; P(phys + 0x12C, '<I', 2)
    P(phys + 0x150, '<3f', 0., 2., -1.); P(phys + 0x15C, '<f', .75); P(phys + 0x160, '<3f', 0., -1., -1.); P(phys + 0x16C, '<f', .75)
    h = 0x2DD780; P(h, '<H', 1); P(h + 6, '<H', 0x1844); P(h + 0x10, '<3f', 1., 2., 3.); P(h + 0x1C, '<f', .25); P(h + 0x20, '<4f', 0., 0., 1., -5.)
    with zipfile.ZipFile(path, 'w', zipfile.ZIP_DEFLATED) as z: z.writestr('eeMemory.bin', bytes(ee)); z.writestr('PCSX2 Savestate Version.id', b'\0' * 36)

class T(unittest.TestCase):
    def test_riders_and_hits(self):
        with tempfile.TemporaryDirectory() as d:
            f = os.path.join(d, 's.p2s'); fake_state(f); st = p2s.State(f); rs = st.riders()
            self.assertEqual(len(rs), 1); r = rs[0]; self.assertEqual((r['type'], r['pos']), (1, (10., 20., 30.)))
            self.assertEqual([tuple(round(c, 3) for c in w) for _, w, _ in r['contacts']], [(10., 22., 29.), (10., 19., 29.)])    # mundo = pos + local (matriz identidad)
            hs = p2s.hit_records(st); self.assertEqual(len(hs), 1); self.assertEqual((hs[0]['addr'], hs[0]['surface'], hs[0]['d']), (0x2DD780, 0x1844, -5.))

if __name__ == '__main__': unittest.main()
