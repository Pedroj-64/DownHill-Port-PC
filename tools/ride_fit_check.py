#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Oráculo empírico de la conducción (hito 3b, términos HIPÓTESIS de src/bike.hpp): con capturas `pine_capture.py --ride` (solo cifras; no van al repo)
  1) rodadura: ventanas de W ticks de suelo seguido, v' = v + dt·(share·g_t − k·v²) integrado sobre la dirección de marcha capturada; reporta la fracción de ventanas
     cuya velocidad final queda dentro del 15 % de la capturada y la mediana del error (criterio de amplitud, bike-physics.md);
  2) saltos: tramos aéreos >= 15 ticks, balística g con arrastre k_aire desde el primer tick; error relativo del alcance horizontal y de la caída vertical.
Uso: ride_fit_check.py cap1 [cap2 ...] [--share 0.46 --k 0.0063 --kair 0.0077]. Constantes: G = 96.6, dt = 1/50, ctrl = rider0+0x40C0 (+0xC0 vel, +0x50 posición, +0x4A4 estado de suelo)."""
import argparse, os, sys
import numpy as np
sys.path.insert(0, os.path.dirname(__file__)); sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'pcsx2'))
from analyze_capture import read_cap
from p2s import RIDER_BASE
C = RIDER_BASE + 0x40C0; G = 96.6; DT = 0.02

def main():
    ap = argparse.ArgumentParser(); ap.add_argument('caps', nargs='+'); ap.add_argument('--share', type=float, default=0.46); ap.add_argument('--k', type=float, default=0.0063)
    ap.add_argument('--kair', type=float, default=0.0077); ap.add_argument('--window', type=int, default=50); a = ap.parse_args()
    roll, jump = [], []
    for name in a.caps:
        fr = [s for _, s in read_cap(os.path.expanduser(name))]; f = lambda s, o: np.array(s.f32(C + o, 3), float)
        gr = [(s.u32(C + 0x4A4) & 0xFFFF) < 2 for s in fr]; V = [np.linalg.norm(f(s, 0xC0)) for s in fr]
        T = [-G * f(s, 0xC0)[2] / max(np.linalg.norm(f(s, 0xC0)), 1e-6) for s in fr]          # gravedad a lo largo de la marcha
        for i in range(0, len(fr) - a.window, 10):
            if all(gr[i:i + a.window + 1]) and V[i] > 3:
                v = V[i]
                for gt in T[i:i + a.window]: v = max(v + DT * (a.share * gt - a.k * v * v), 0)
                roll.append(abs(v - V[i + a.window]) / max(V[i + a.window], 5))
        i = 0
        while i < len(fr):
            if gr[i] or 'ride' not in os.path.basename(name): i += 1; continue   # los roll_*.cap no traen la posición del cuerpo de conducción (no son --ride)
            j = i
            while j + 1 < len(fr) and not gr[j + 1]: j += 1
            if j - i >= 15:
                p, v = f(fr[i], 0x50), f(fr[i], 0xC0); p0 = p.copy()
                for _ in range(j - i): v = v + DT * (np.array([0, 0, -G]) - a.kair * np.linalg.norm(v) * v); p = p + DT * v
                r = f(fr[j], 0x50) - p0; m = p - p0; jump.append((abs(np.linalg.norm(m[:2]) - np.linalg.norm(r[:2])) / max(np.linalg.norm(r[:2]), 1), abs(m[2] - r[2]) / max(abs(r[2]), 1)))
            i = j + 1
    roll = np.array(roll); jump = np.array(jump)
    print(f'rodadura: {len(roll)} ventanas de {a.window} ticks; dentro del 15 %: {(roll <= .15).mean():.2f}; mediana del error {np.median(roll):.3f}')
    if len(jump): print(f'saltos: {len(jump)} tramos; alcance dentro del 15 %: {(jump[:, 0] <= .15).sum()}/{len(jump)} (mediana {np.median(jump[:, 0]):.3f}); caída vertical mediana {np.median(jump[:, 1]):.3f} máx {jump[:, 1].max():.3f}')

if __name__ == '__main__': main()
