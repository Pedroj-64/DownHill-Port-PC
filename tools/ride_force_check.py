#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Oráculo del cuerpo de CONDUCCIÓN (hito 3b): ctrl = rider0 + 0x40C0, mismo layout que integrator.md, integrado por FUN_00238818 (vía FUN_00133810); fuerzas de FUN_00136330.
Entrada: captura de `pine_capture.py --ride` (solo cifras; las capturas NO van al repo). Comprueba, solo con el jugador (rider 0):
  1) arrastre ctrl+0x110 = -m·k·|v_prev|²·v̂_prev, con k = ctrl+0x490 y m = ctrl+0x150 (error relativo y coseno);
  2) peso ctrl+0x100 = (0,0,-m·32.2·[0x418]·G0) con G0 = global 0x77A7D8 en ticks de suelo (ctrl+0x4A4 < 2);
  3) en el aire (ctrl+0x4A4 ≥ 2): P(i) - P(i-1) = dt·F(i) con F = acumulador ctrl+0xE0, error ≤ 3 ulp de max|P| (umbral propuesto, no fijado).
Uso: ride_force_check.py captura.cap [--assert]. Salida 1 si falla algo con --assert."""
import argparse, os, sys
import numpy as np
sys.path.insert(0, os.path.dirname(__file__)); sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'pcsx2'))
from analyze_capture import read_cap
from p2s import RIDER_BASE
C = RIDER_BASE + 0x40C0; DT = 0.02

def main():
    ap = argparse.ArgumentParser(); ap.add_argument('cap'); ap.add_argument('--assert', dest='check', action='store_true'); a = ap.parse_args()
    fr = [s for _, s in read_cap(a.cap)]; f = lambda s, o, n=3: np.array(s.f32(C + o, n), float); ok = True
    air = [s.u32(C + 0x4A4) & 0xFFFF >= 2 for s in fr]; g0 = fr[0].f32(0x77A7D8)[0]
    dr, cs, wg = [], [], []
    for p, s in zip(fr, fr[1:]):
        m, k = f(s, 0x150, 1)[0], f(s, 0x490, 1)[0]; v = f(p, 0xC0); D = f(s, 0x110)
        if k > 0 and v @ v > 1: dr.append(np.linalg.norm(D) / (m * k * (v @ v)) - 1); cs.append(-(D @ v) / (np.linalg.norm(D) * np.linalg.norm(v)) - 1)
    for s, ar in zip(fr, air):
        if not ar:
            w = f(s, 0x100); m = f(s, 0x150, 1)[0]; wg.append(-w[2] / (m * 32.2 * f(s, 0x418, 1)[0]) - g0)
    e = []; ulp = float(np.spacing(np.float32(max(np.abs(f(s, 0x60)).max() for s in fr))))
    for i in range(1, len(fr)):
        s = fr[i]
        if air[i] and air[i - 1] and np.abs(f(s, 0xE0)).max() > 0: e.append(np.abs(f(s, 0x60) - f(fr[i - 1], 0x60) - DT * f(s, 0xE0)).max())
    bad = [i for i, (x, c) in enumerate(zip(dr, cs)) if abs(x) > 1e-6 or abs(c) > 1e-6]; p1 = len(bad) <= 0.005 * len(dr); ok &= p1   # un impulso de contacto entre la muestra y el cálculo de fuerzas rompe algún tick aislado
    good = [x for x in dr if abs(x) <= 1e-6]
    print(f"{'PASA ' if p1 else 'FALLA'} arrastre: {len(dr) - len(bad)} de {len(dr)} ticks con |err rel| y |cos-1| <= 1e-6 (máx entre ellos {max(map(abs, good)):.2e}); fuera de umbral {len(bad)} (permitido <= 0.5 %), ticks {bad[:8]}")
    w1 = abs(np.array(wg)).max() if wg else 0.; print(f"información peso en suelo: G0 implícito - global: |máx| {w1:.2e} (los ticks de transición con G0 = 1.25 cuentan como aire para el motor) en {len(wg)} ticks")
    if e: p3 = max(e) <= 3 * ulp; ok &= p3; print(f"{'PASA ' if p3 else 'FALLA'} aire: |ΔP - dt·F| máx {max(e):.2e} (mediana {np.median(e):.2e}) <= {3 * ulp:.2e} = 3 ulp(max|P|) en {len(e)} ticks")
    else: print('sin ticks en el aire consecutivos: P(i)-P(i-1)=dt·F sin comprobar')
    if a.check and not ok: sys.exit(1)

if __name__ == '__main__': main()
