#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Vuelca SOLO NÚMEROS de una captura con impacto (air_capture.py) para tests/impact_replay_check.cpp. Sale fuera del repo.
Formato: línea 1 = invMass com(3) invInertia(3) restitución(+0x114) fricción(+0x118) nPuntos; luego nPuntos líneas = local(3) radio; luego una línea por tick = nodo(3) R(9) pos(3) P(3) L(3).
Uso: impact_dump_ref.py captura.cap salida.txt [--rider 1]"""
import argparse, os, sys
sys.path.insert(0, os.path.dirname(__file__)); sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'pcsx2'))
import integrator_check as ic
from analyze_capture import read_cap

def main():
    ap = argparse.ArgumentParser(); ap.add_argument('cap'); ap.add_argument('out'); ap.add_argument('--rider', type=int, default=1); a = ap.parse_args()
    F = read_cap(a.cap); f = lambda v: ' '.join('%.9g' % x for x in v); rows = []
    for _, s in F:
        r = next(q for q in s.riders() if q['index'] == a.rider); b = ic.body(s, r); rows.append((s, r, b))
    s, r, b = rows[0]; e, mu = s.f32(r['phys'] + 0x114, 2); pts = r['contacts']
    with open(a.out, 'w') as o:
        o.write(f"{b['invm']:.9g} {f(b['com'])} {f(b['d'])} {e:.9g} {mu:.9g} {len(pts)}\n")
        for loc, _w, rad in pts: o.write(f"{f(loc)} {rad:.9g}\n")
        for s, r, b in rows: o.write(f"{f(r['pos'])} {f(b['R'].ravel())} {f(b['pos'])} {f(b['P'])} {f(b['L'])}\n")
    print(f'{len(rows)} ticks, {len(pts)} puntos, e={e:.3g} mu={mu:.3g} -> {a.out}')

if __name__ == '__main__': main()
