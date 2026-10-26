#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""T7: exporta vectores de prueba (entradas + salida de la referencia Python engine_tick_ref4.tick) para validar el port C++ src/enginetick.hpp bit a bit
(tests/enginetick_check.cpp). Los vectores salen de las grabaciones del usuario: van FUERA del repo (regla 4), por defecto ~/dh-states/h3/etick_<NIVEL>.bin.
Formato (little-endian): u32 n; n registros de f64: ENTRADA im, cml[3], cm[3], P[3], L[3], w[3], M[9], loc[3][3], susp[2][3], E[3], Fq[3], ramp, coefs[32], state[2], rad[3], scl[3][3]
(= 1+3+3+3+3+3+9+9+6+3+3+1+32+2+3+9 = 93 f64) y SALIDA cm[3], P[3], L[3], M[9], w[3], susp[2][3], j, nimp, npen, err (= 3+3+3+9+3+6+4 = 31 f64).
Uso: enginetick_export.py [--level ALPINE_] [--max N] [--stride K] [--out ruta]   / Test-vector exporter for the C++ port of the T7 collision tick."""
import argparse, glob, os, struct, sys
import numpy as np
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE); sys.path.insert(0, os.path.join(HERE, 'telemetry'))
import colgeo, engine_tick_ref as T, engine_tick_ref4 as R
F = T.F; u8 = T.u8

def main():
    ap = argparse.ArgumentParser(); ap.add_argument('--level', default='ALPINE_'); ap.add_argument('--max', type=int, default=400); ap.add_argument('--stride', type=int, default=1); ap.add_argument('--out', default='')
    a = ap.parse_args(); recs = []; name = None
    for dd in sorted(glob.glob(os.path.expanduser('~/dh-states/awiiwa/dhtel_*%s*TNOS' % a.level)))[:1]:
        name = os.path.basename(dd).split('_')[2]; col = colgeo.Col(os.path.join(HERE, '..', 'out', 'maps', name + '.col'))
        rows = T.C.ticks(dd, True)
        for r_, _, fs in os.walk(dd):
            if 'static.dhtel' in fs: tb = T.C.S.read_file(os.path.join(r_, 'static.dhtel'))[1][0][2]['surf_table']
        coefs = [struct.unpack_from('<f', tb, q * 32)[0] for q in range(32)]; lastE = np.zeros(3); lastF = np.zeros(3); lastR = np.zeros(3); cnt = 0
        for (t0, c, _), (t1, c1, _) in zip(rows, rows[1:]):
            E1 = F(c1, 0xE0)
            if np.linalg.norm(E1) > 0: lastE = E1; lastF = F(c1, 0xF0); lastR = E1 - F(c1, 0x100) - F(c1, 0x110)
            if not .019 < t1 - t0 < .021 or c1[0x60:0x80] == c[0x60:0x80]: continue
            cnt += 1
            if cnt % a.stride: continue
            S = T.load(c); S['loc'].append(F(c, 0x3E0)); rad = [F(c, 0x2E0 + 0x0C, 1)[0], F(c, 0x360 + 0x0C, 1)[0], F(c, 0x3E0 + 0x0C, 1)[0]]
            st = (u8(c1, 0x354), u8(c1, 0x3D4)); Et, Ft = (E1, F(c1, 0xF0)) if np.linalg.norm(E1) > 0 else (lastE, lastF)
            if np.linalg.norm(E1) == 0: Et = F(c1, 0x100) + F(c1, 0x110) + lastR
            scl = [F(c, 0x2E0 + 0x10), F(c, 0x360 + 0x10), F(c, 0x3E0 + 0x10)]; ramp = min(F(c, 0x48C, 1)[0], 1.0)
            o = R.tick(S, Et, Ft, col, coefs, ramp, st, rad, scl)
            f = lambda *v: [float(x) for q in v for x in np.ravel(q)]
            inp = f(S['im'], S['cml'], S['cm'], S['P'], S['L'], S['w'], S['M'], S['loc'][0], S['loc'][1], S['loc'][2], S['susp'], Et, Ft, ramp, coefs, st, rad, scl)
            assert len(inp) == 93, len(inp)
            out = f(o['cm'], o['P'], o['L'], o['M'], o['w'], o['susp'], o['j'], o['nimp'], o.get('npen', 0), 1.0 if o['err'] else 0.0); assert len(out) == 31, len(out)
            recs.append(inp + out)
            if a.max and len(recs) >= a.max: break
    out_path = a.out or os.path.expanduser('~/dh-states/h3/etick_%s.bin' % name); os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with open(out_path, 'wb') as fo:
        fo.write(struct.pack('<I', len(recs)))
        for r in recs: fo.write(struct.pack('<%dd' % len(r), *r))
    print(f'{len(recs)} ticks -> {out_path}; col: out/maps/{name}.col')

if __name__ == '__main__': main()
