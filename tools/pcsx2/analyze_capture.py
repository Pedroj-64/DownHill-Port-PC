#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Analiza una captura de pine_capture.py contra la malla de colisión.
Uso: analyze_capture.py captura.cap nivel.col [--sweep-cli build/sweep_cli] [--max-print 12]
Por cada muestra (paso de física) reconstruye el estado de los pilotos y comprueba:
  1) registros de impacto del motor (0x30 B en la pila de la física): ¿mismo triángulo, normal, d = n.v1 y superficie que la malla?
  2) puntos de contacto del jugador: holgura (distancia al suelo - radio) de los que tocan el suelo.
  3) (informativo) consistencia del test de cara de Ground::sweep con el desplazamiento D que implica el registro del motor y la posición del paso anterior."""
import argparse, os, struct, subprocess, sys
import numpy as np
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
from p2s import State, hit_records
import p2s_check as pc

def read_cap(path):
    d = open(path, 'rb').read()
    if d[:8] != b'DHCAP1\0\0': raise SystemExit('no es una captura DHCAP1')
    nw, _ = struct.unpack_from('<II', d, 8); wins = [struct.unpack_from('<II', d, 16 + 8 * i) for i in range(nw)]; o = 16 + 8 * nw; out = []
    while o + 12 <= len(d):
        t, n = struct.unpack_from('<dI', d, o); o += 12; blob = d[o:o + n]; o += n; datas = []; q = 0
        for _, ln in wins: datas.append(blob[q:q + ln]); q += ln
        out.append((t, State.from_windows(wins, datas)))
    return out

def main():
    ap = argparse.ArgumentParser(); ap.add_argument('cap'); ap.add_argument('col'); ap.add_argument('--sweep-cli'); ap.add_argument('--max-print', type=int, default=12); a = ap.parse_args()
    V, S = pc.load_col(a.col); frames = read_cap(a.cap); print(f'{len(frames)} muestras, {frames[-1][0] - frames[0][0]:.1f} s')
    tot = ok = 0; bad = []; clr = []; surfs = {}; seen = set(); implied = []
    prev = None
    for fi, (t, st) in enumerate(frames):
        rs = st.riders(); pl = next((r for r in rs if r['type'] == 1), None)
        if pl is None: prev = None; continue
        for h in hit_records(st, 0x2DD600, 0x2DDE00):
            key = (h['addr'], round(h['point'][0], 3), round(h['point'][1], 3), round(h['point'][2], 3), h['frac'])
            if key in seen: continue                                  # el mismo resto de pila persiste varias muestras
            seen.add(key); p = np.array(h['point'])
            if min(np.linalg.norm(p - np.array(c[1])) for c in pl['contacts']) > 3.0: continue
            r = pc.triangle_for_point(V, S, p)
            if r is None: continue
            ti, dist, n, d, surf = r; tot += 1; surfs[surf] = surfs.get(surf, 0) + 1
            good = surf == h['surface'] and np.abs(n - np.array(h['normal'])).max() < 1e-4 and abs(d - h['d']) < 0.01 and dist < 0.05
            ok += good
            if not good: bad.append((fi, h['addr'], hex(h['surface']), hex(surf), float(np.abs(n - np.array(h['normal'])).max()), h['d'], d, dist))
            elif prev is not None and a.sweep_cli:
                # D implícito: p = (s - n r) + D t  (+ holgura) con s = centro del contacto más cercano en la muestra anterior
                for c in prev['contacts']:
                    s0 = np.array(c[1]); rad = c[2]; nn = np.array(h['normal']); D = (p - (s0 - nn * rad)) / max(h['frac'], 1e-6)
                    if 0.0 < np.linalg.norm(D) < 5.0 and abs(float(nn @ D)) > 1e-3: implied.append((s0, s0 + D, rad, h['frac'])); break
        for k, (_, w, rad) in enumerate(pl['contacts']):
            res = pc.clearance(V, S, np.array(w), rad)
            if res is not None and res['clearance'] < 1.0: clr.append(res['clearance'])
        prev = pl
    print(f'\n1) registros de impacto del jugador distintos: {tot}; coinciden (superficie, normal < 1e-4, d < 0.01, punto < 0.05 u del triángulo): {ok}')
    print('   superficies vistas en impactos:', {hex(k): v for k, v in sorted(surfs.items())})
    for b in bad[:a.max_print]: print('   DISCREPANCIA muestra %d @%#x surface motor %s malla %s |dn| %.2e d motor %.3f malla %.3f dist %.4f' % b)
    if clr:
        c = np.array(clr); print(f'2) holgura de puntos de contacto en suelo (< 1 u), {len(c)} lecturas: media {c.mean():+.3f}, p5 {np.percentile(c,5):+.3f}, p95 {np.percentile(c,95):+.3f}, min {c.min():+.3f}, max {c.max():+.3f} u')
    if implied and a.sweep_cli:
        inp = ''.join(' '.join(repr(float(x)) for x in (*s, *e, r)) + '\\n' for s, e, r, _ in implied)
        out = subprocess.run([a.sweep_cli, a.col], input=inp, capture_output=True, text=True).stdout.splitlines()
        diffs = [abs(float(o.split()[1]) - f) for o, (_, _, _, f) in zip(out, implied) if o.startswith('HIT') and float(o.split()[1]) > -1e30]
        if diffs: d = np.array(diffs); print(f'3) (informativo) fracción de Ground::sweep con el D implícito: {len(d)} casos, |Δfrac| media {d.mean():.4f}, p95 {np.percentile(d,95):.4f}, máx {d.max():.4f}')

if __name__ == '__main__': main()
