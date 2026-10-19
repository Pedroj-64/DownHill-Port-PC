#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""T7 (rozamiento lateral de rueda, FUN_00132FD8 / FUN_00131078): modelo por tick contra una captura de ticks consecutivos.
Modelo (hipótesis contrastada, solo cifras; las capturas no van al repo):
  a  = fila 0 de M=*(ctrl+0x38)=0x2DD740 en el tick i+1, proyectada al plano tangente (normal ctrl+0x220) y normalizada
  v  = v_i + E·dt/m   (E = ctrl+0xE0 del tick i+1: velocidad tras integrar las fuerzas)
  por cada rueda en contacto (ctrl+0x354 == 1, byte bajo de ctrl+0x3D4 == 1), en orden:
      d = sign(s)·min(|s|·ramp, 0.0225·coef0[clase]),  s = a·v_tangente,  v -= d·a  (impulso con e=1, tope por llamada)
  ramp = min(ctrl+0x48C del tick i, 1);  coef0 = columna 0 de la tabla del nivel (RST+0xCA4, fila = clase = u16 ctrl+0x206 & 0x1F).
  predicción de J·a = -Σd/dt  frente a m·ctrl+0x250/0.7 proyectado en a.
Uso: lateral_friction_check.py captura.cap nivel.RST [--list] [--assert]   (capturas en ~/dh-states/h3)
Criterio PROPUESTO: >= 85 % de los ticks con contacto y acumulador presente con |obs-pred| <= 1 %·|obs| + 0.02.
/ T7 lateral wheel-friction model per tick."""
import argparse, os, sys
import numpy as np
sys.path.insert(0, os.path.dirname(__file__)); sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'pcsx2'))
from analyze_capture import read_cap
from p2s import RIDER_BASE
C0 = RIDER_BASE + 0x40C0; DT = 0.02
def f(s, a, n=3): return np.array([x[0] if isinstance(x, tuple) else x for x in s.f32(a, n)], float)

def main():
    ap = argparse.ArgumentParser(); ap.add_argument('cap'); ap.add_argument('rst'); ap.add_argument('--list', action='store_true'); ap.add_argument('--assert', dest='chk', action='store_true'); a_ = ap.parse_args()
    T = np.frombuffer(open(a_.rst, 'rb').read()[0xCA4:0xCA4 + 1024], dtype='<f4').reshape(32, 8)
    cap = a_.cap if os.path.exists(a_.cap) else os.path.expanduser('~/dh-states/h3/' + a_.cap)
    fr = [s for _, s in read_cap(cap)]; n = ok = 0; bad = []
    for p, s in zip(fr, fr[1:]):
        if np.allclose(f(p, C0 + 0x50), f(s, C0 + 0x50)): continue
        m = f(s, C0 + 0x150, 1)[0]; x = f(s, C0 + 0x250); E = f(s, C0 + 0xE0)
        if np.linalg.norm(x) < 1 or np.linalg.norm(E) == 0: continue          # sin contacto / acumulador ya consumido
        J = x / 0.7; nrm = f(s, C0 + 0x220); nrm /= np.linalg.norm(nrm)
        ax = f(s, 0x2DD740); ax = ax - np.dot(ax, nrm) * nrm; ax /= np.linalg.norm(ax)
        v = f(p, C0 + 0xC0) + E / m * DT; sv = np.dot(ax, v - np.dot(v, nrm) * nrm)
        coef = float(T[s.u32(C0 + 0x206) & 31, 0]); capd = 0.0225 * coef; rb = min(f(p, C0 + 0x48C, 1)[0], 1.0)
        w = int(s.u32(C0 + 0x354) & 0xff == 1) + int((s.u32(C0 + 0x3D4) & 0xff) == 1); dv = 0.0
        for _ in range(w): d = np.sign(sv) * min(abs(sv) * rb, capd); dv -= d; sv -= d
        pred, obs = dv / DT, np.dot(ax, J); good = abs(obs - pred) <= 0.01 * abs(obs) + 0.02
        n += 1; ok += good
        if not good: bad.append((obs, pred, w, rb, coef))
    pc = 100 * ok / max(n, 1); print(f"{os.path.basename(cap)}: {ok}/{n} ticks ({pc:.1f} %) con |obs-pred| <= 1 % + 0.02")
    if a_.list:
        for b in bad[:40]: print("  obs %9.3f pred %9.3f ruedas %d ramp %.3f coef %.0f" % b)
    if a_.chk and pc < 85: print('FALLA criterio propuesto >= 85 %'); sys.exit(1)

if __name__ == '__main__': main()
