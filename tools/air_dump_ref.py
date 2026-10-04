#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Vuelca SOLO NÚMEROS de una captura de vuelo libre (.cap de tools/pcsx2/air_capture.py) para tests/air_replay_check.cpp. El archivo NO va al repo (sale fuera, p. ej. ~/dh-states/h3/).
Formato de texto: línea 1 = invMass com(3) invInertia(3); luego una línea por tick = pos(3) R(9, filas) P(3) L(3). Uso: air_dump_ref.py captura.cap salida.txt [--rider 1]"""
import argparse, os, sys
sys.path.insert(0, os.path.dirname(__file__))
import integrator_airfit as af

def main():
    ap = argparse.ArgumentParser(); ap.add_argument('cap'); ap.add_argument('out'); ap.add_argument('--rider', type=int, default=1); a = ap.parse_args()
    B = af.load(a.cap, a.rider); f = lambda v: ' '.join('%.9g' % x for x in v)
    with open(a.out, 'w') as o:
        o.write(f"{B[0]['invm']:.9g} {f(B[0]['com'])} {f(B[0]['d'])}\n")
        for b in B: o.write(f"{f(b['pos'])} {f(b['R'].ravel())} {f(b['P'])} {f(b['L'])}\n")
    print(f'{len(B)} ticks -> {a.out}')

if __name__ == '__main__': main()
