#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Valida el integrador fiel (FUN_00238818, docs/formats/integrator.md) con una captura en caída libre (.cap de tools/pcsx2/air_capture.py), sin suponer que F_eff sea sólo gravedad.
Hipótesis (se ajusta y se comprueba): ANTES de integrar el juego aplica amortiguación lineal a los momentos, P *= 1-c·dt y L *= 1-cL·dt (c, cL por mínimos cuadrados de ΔP, ΔL);
FUN_00238818 recibe la gravedad como fuerza F = (0,0,-m·G) (G por mínimos cuadrados). La comprobación de pos y R NO usa c, cL ni G salvo por el estado previo: es independiente del ajuste de momentos.
Uso: integrator_airfit.py captura.cap [--rider 1]   -> constantes ajustadas y errores máx/medios por paso (pos, R, P y L relativos, v, omega)."""
import argparse, os, sys
import numpy as np
sys.path.insert(0, os.path.dirname(__file__)); sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'pcsx2'))
import integrator_check as ic
DT = ic.DT

def fit(B):
    """-> (c, cL, G): v1 = v0·(1-c·dt) - dt·G·ẑ ; L1 = L0·(1-cL·dt)."""
    A, y, Al, yl = [], [], [], []
    for a, b in zip(B, B[1:]):
        for j in range(3): A.append([-a['vel'][j] * DT, -DT if j == 2 else 0]); y.append(b['vel'][j] - a['vel'][j]); Al.append([-a['L'][j] * DT]); yl.append(b['L'][j] - a['L'][j])
    (c, G), (cL,) = np.linalg.lstsq(np.array(A), np.array(y), rcond=None)[0], np.linalg.lstsq(np.array(Al), np.array(yl), rcond=None)[0]
    return c, cL, G

def errors(B, c, cL, G):
    """Errores por paso al predecir b desde a: estado previo = amortiguado; luego ic.step con F = (0,0,-m·G) y T = 0."""
    E = []
    for a, b in zip(B, B[1:]):
        pre = dict(a); pre['P'] = a['P'] * (1 - c * DT); pre['L'] = a['L'] * (1 - cL * DT); pre['vel'] = pre['P'] * a['invm']; pre['omega'] = a['d'] * pre['L']
        p = ic.step(pre, DT, np.array([0, 0, -G / a['invm']]), np.zeros(3))
        E.append([np.abs(p['pos'] - b['pos']).max(), np.abs(p['R'] - b['R']).max(), np.abs(p['P'] - b['P']).max() / np.abs(b['P']).max(), np.abs(p['L'] - b['L']).max() / np.abs(b['L']).max(),
                  np.abs(p['vel'] - b['vel']).max(), np.abs(p['omega'] - b['omega']).max()])
    return np.array(E)

def main():
    ap = argparse.ArgumentParser(); ap.add_argument('cap'); ap.add_argument('--rider', type=int, default=1); a = ap.parse_args()
    from analyze_capture import read_cap
    B = [ic.body(s, next(q for q in s.riders() if q['index'] == a.rider)) for _, s in read_cap(a.cap)]
    c, cL, G = fit(B); E = errors(B, c, cL, G); n = ['|dpos|', '|dR|', 'relP', 'relL', '|dv|', '|dw|']
    print(f'{len(E)} pasos consecutivos; c = {c:.4f} /s, cL = {cL:.4f} /s, G = {G:.3f} u/s² (ajustadas)')
    print('máx  ' + '  '.join(f'{k} {v:.2e}' for k, v in zip(n, E.max(0)))); print('media ' + '  '.join(f'{k} {v:.2e}' for k, v in zip(n, E.mean(0))))
    print(f'float32 ulp de |pos| máx = {np.spacing(np.float32(np.abs(np.array([b["pos"] for b in B])).max())):.2e} (suelo de ruido de |dpos|)')

if __name__ == '__main__': main()
