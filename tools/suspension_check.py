#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""T7: oráculo de la suspensión de rueda (src/suspension.hpp, FUN_00133810) contra las grabaciones ~/dh-states/awiiwa (ctrl+0x2E0/0x360).
Para cada par de ticks consecutivos con la rueda en estado 1 (sonda en contacto) antes y después y sin impacto (ctrl+0x430 == 0):
  e' = e + s·dt (e = -1.8 - z_local);  s' = s + dt·(80·(0.5 - (e + off)) - (s<0 ? 10 : 7)·s).
Criterio (PROPUESTA): |e'-e_obs| p90 <= 1e-6 y |s'-s_obs| p90 <= 1e-3 u/s, sin los ticks con e en los topes (0 / 0.5).
/ T7 wheel-suspension oracle on the recorded ctrl sphere blocks."""
import argparse, glob, os, struct, sys
import numpy as np
sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'telemetry'))
sys.path.insert(0, os.path.dirname(__file__))
import contact_impulse_check as C
F = C.F

def main():
    ap = argparse.ArgumentParser(); ap.add_argument('--assert', dest='chk', action='store_true'); a = ap.parse_args()
    ee, es, n = [], [], 0
    for d in sorted(glob.glob(os.path.expanduser('~/dh-states/awiiwa/dhtel_*_TNOS'))):
        rows = C.ticks(d, True)
        for (t, c, _), (t1, c1, _) in zip(rows, rows[1:]):
            if not .019 < t1 - t < .021 or F(c1, 0x430, 1)[0] != 0: continue
            for k, o in ((0, 0x354), (1, 0x3D4)):
                if struct.unpack_from('<I', c1, o)[0] & 0xff != 1 or struct.unpack_from('<I', c, o)[0] & 0xff != 1: continue
                b = 0x2E0 + 0x80 * k
                e = -1.8 - F(c, b, 4)[2]; off = F(c, b + 0x70, 1)[0]; s = F(c, b + 0x60, 1)[0]
                if not 0.01 < e < 0.49: continue
                e1 = e + s * .02; s1 = s + .02 * (80 * (0.5 - (e + off)) - (10 if s < 0 else 7) * s)
                ee.append(abs(e1 - (-1.8 - F(c1, b, 4)[2]))); es.append(abs(s1 - F(c1, b + 0x60, 1)[0])); n += 1
    ee, es = np.array(ee), np.array(es)
    pe, ps = np.percentile(ee, 90), np.percentile(es, 90)
    print(f"ruedas-tick comparados: {n}; error de e p50/p90 = {np.percentile(ee,50):.2e}/{pe:.2e}; error de s p50/p90 = {np.percentile(es,50):.2e}/{ps:.2e}")
    ok = n > 1000 and pe <= 1e-6 and ps <= 1e-3
    print(('PASA ' if ok else 'FALLA') + ' criterio propuesto: p90(e) <= 1e-6 y p90(s) <= 1e-3')
    if a.chk and not ok: sys.exit(1)

if __name__ == '__main__': main()
