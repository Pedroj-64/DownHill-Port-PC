#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""T7 (oráculo de contacto): descompone el cambio de momento por tick del cuerpo de conducción.
  J = m·Δv/dt − ctrl+0xE0(i+1)   (acumulador de fuerza, exacto sin contacto)  ->  impulso de contacto por tick.
Hipótesis contrastada con las grabaciones de ~/dh-states/awiiwa/*_TNOS (ticks.dhtel*), solo cifras:
  H1  J = m·ctrl+0x250 / 0.7   (0.7 = 2·0.35, factor k de FUN_00132FD8; hipótesis de origen).
Ticks con ctrl+0xE0 == 0 (acumulador ya consumido al muestrear; coinciden con ctrl+0x430 >= 10 en el 100 % de los casos
medidos) NO son comparables y se cuentan aparte: antes se interpretaron mal como «impactos».
Uso: contact_impulse_check.py [dir_o_ticks.dhtel ...] [--assert]
/ T7 contact oracle: per-tick momentum split into force accumulator + contact impulse (ticks with a consumed accumulator are excluded)."""
import argparse, glob, os, struct, sys
import numpy as np
sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'telemetry'))
import schema as S
F = lambda c, o, n=3: np.array(struct.unpack_from('<%df' % n, c, o))

def ticks(d, full=False):
    path = d
    if os.path.isdir(d):
        for r, _, fs in os.walk(d):
            for f in fs:
                if f.startswith('ticks.dhtel'): path = os.path.join(r, f)
    rows, prev = [], None
    for t, _, p in S.read_file(path)[1]:
        P = tuple(F(p['ctrl'], 0x50))
        if P != prev: rows.append((t, p['ctrl'], p['node'])); prev = P      # una muestra por paso de física
    return rows

def main():
    ap = argparse.ArgumentParser(); ap.add_argument('src', nargs='*'); ap.add_argument('--assert', dest='chk', action='store_true'); a = ap.parse_args()
    src = a.src or sorted(glob.glob(os.path.expanduser('~/dh-states/awiiwa/dhtel_*_TNOS')))
    n = skip = ok = 0; err = []; ncap = nsat = nin = 0; nl = okl = 0
    for d in src:
        rows = ticks(d, True); tb = None
        for r, _, fs in os.walk(d):
            if 'static.dhtel' in fs: tb = S.read_file(os.path.join(r, 'static.dhtel'))[1][0][2]['surf_table']
        coef = [struct.unpack_from('<f', tb, k * 32)[0] for k in range(32)] if tb else None
        for (t, c, nd0), (t1, c1, nd1) in zip(rows, rows[1:]):
            if not .015 < t1 - t < .025: continue
            m = F(c1, 0x150, 1)[0]; x = F(c1, 0x250); E = F(c1, 0xE0)
            if np.linalg.norm(x) < 1: continue
            if np.linalg.norm(E) == 0: skip += 1; continue
            J = (F(c1, 0xC0) - F(c, 0xC0)) / 0.02 * m - E; e = np.linalg.norm(J - m * x / 0.7) / np.linalg.norm(J)
            n += 1; ok += e <= 1e-3; err.append(e)
            if coef:      # H3 (rozamiento lateral por ruedas, ver tools/lateral_friction_check.py): eje = fila 0 del nodo (+0x40) del tick i+1
                nrm = F(c1, 0x220); nrm = nrm / np.linalg.norm(nrm); ax = F(nd1, 0x40); ax = ax - np.dot(ax, nrm) * nrm; na = np.linalg.norm(ax)
                if na > 0.2:
                    ax = ax / na; v = F(c, 0xC0) + E / m * 0.02; sv = np.dot(ax, v - np.dot(v, nrm) * nrm); capd = 0.0225 * coef[struct.unpack_from('<H', c1, 0x206)[0] & 31]; rb = min(F(c, 0x48C, 1)[0], 1.0)
                    dv = 0.0
                    for _ in range(int(struct.unpack_from('<I', c1, 0x354)[0] & 0xff == 1) + int(struct.unpack_from('<I', c1, 0x3D4)[0] & 0xff == 1)):
                        dd = np.sign(sv) * min(abs(sv) * rb, capd); dv -= dd; sv -= dd
                    nl += 1; okl += abs(np.dot(ax, x / 0.7) - dv / 0.02) <= 0.01 * abs(np.dot(ax, x / 0.7)) + 0.02
            if coef:      # H2: |J|/m <= 225·coef/100 (coef = col 0 de la clase en la tabla del nivel, RST+0xCA4); satura exactamente en el tope
                cap = 2.25 * coef[struct.unpack_from('<H', c, 0x206)[0] & 31]; jm = np.linalg.norm(x / 0.7); ncap += 1; nin += jm <= cap * 1.001; nsat += abs(jm - cap) <= cap * 1e-3
    err = np.array(err)
    print(f"ticks de contacto con acumulador presente: {n} (excluidos {skip} con 0xE0 == 0)")
    print(f"H1 J = m·0x250/0.7: error relativo percentiles 50/90/95 = {np.percentile(err, [50, 90, 95])}; <= 1e-3 en {ok} ({100*ok/n:.1f} %)")
    if ncap: print(f"H2 tope |J|/m <= 225·coef0/100: dentro {nin}/{ncap} ({100*nin/ncap:.1f} %), saturados exactos (±0.1 %) {nsat} ({100*nsat/ncap:.1f} %)")
    print(f"H3 rozamiento lateral por rueda (0,0225·coef0 por llamada, ramp, eje del nodo i+1): {okl}/{nl} ({100*okl/max(nl,1):.1f} %) con |obs-pred| <= 1 % + 0.02")
    p = ok / n >= 0.95 and okl / max(nl, 1) >= 0.85
    print(('PASA ' if p else 'FALLA') + ' criterio propuesto: H1 >= 95 % (error <= 1e-3) y H3 >= 85 %')
    if a.chk and not p: sys.exit(1)

if __name__ == '__main__': main()
