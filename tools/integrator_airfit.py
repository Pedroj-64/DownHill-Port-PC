#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Valida el integrador fiel (FUN_00238818, docs/formats/integrator.md) con una captura en caída libre (.cap de tools/pcsx2/air_capture.py), sin suponer que F_eff sea sólo gravedad.
Hipótesis (se ajusta y se comprueba): ANTES de integrar el juego aplica amortiguación lineal a los momentos, P *= 1-c·dt y L *= 1-cL·dt (c, cL por mínimos cuadrados de ΔP, ΔL);
FUN_00238818 recibe la gravedad como fuerza F = (0,0,-m·G) (G por mínimos cuadrados). La comprobación de pos y R NO usa c, cL ni G salvo por el estado previo: es independiente del ajuste de momentos.
Dos niveles (decisión del usuario, 2026-10-03):
  MODELO: residuos de pos, R, P y L relativos frente a umbrales = 3 × el suelo de ruido float32 (ulp de lo comparado: |pos| máx medido; 1.0 para R; eps relativo para P y L).
  PROCEDENCIA: c, cL y G deben localizarse en el binario o en RAM; mientras no (PROVENANCE vacío) son "ajuste empírico" y NO cuentan como validadas.
Uso: integrator_airfit.py ajuste.cap [--rider 1] [--eval otra.cap --eval-rider 5]   (ajusta con la primera, evalúa con la segunda; sin --eval evalúa con la misma). --assert: salida 1 si el nivel modelo falla."""
import argparse, os, sys
import numpy as np
sys.path.insert(0, os.path.dirname(__file__)); sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'pcsx2'))
import integrator_check as ic
DT = ic.DT
PROVENANCE = {'c': None, 'cL': None, 'G': None}   # dirección/función donde viva cada constante; None = sólo ajuste empírico (hipótesis)
K_FLOOR = 3.0
NAMES = ['|dpos|', '|dR|', 'relP', 'relL', '|dv|', '|dw|']

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

def floors(B):
    """Suelo de ruido float32 de cada magnitud comparada: [|dpos|, |dR|, relP, relL]. pos = ulp del mayor |pos| (u); R = ulp(1.0) (elementos <= 1); P y L relativos = ulp(1.0) (eps relativo)."""
    ulp = float(np.spacing(np.float32(max(np.abs(b['pos']).max() for b in B)))); eps = float(np.spacing(np.float32(1.0)))
    return np.array([ulp, eps, eps, eps])

def thresholds(B): return K_FLOOR * floors(B)

def report(B_fit, B_eval, label):
    """Ajusta con B_fit y evalúa con B_eval; imprime residuos y PASA/FALLA por magnitud del nivel modelo; devuelve (constantes, ok)."""
    c, cL, G = fit(B_fit); E = errors(B_eval, c, cL, G); th = thresholds(B_eval); fl = floors(B_eval)
    print(f'== {label}: {len(E)} pasos; constantes ajustadas c={c:.4f}/s cL={cL:.4f}/s G={G:.3f} u/s² (ajuste empírico, NO validadas: procedencia {[k for k, v in PROVENANCE.items() if v is None]} sin localizar)')
    ok = True
    for i in range(4):
        m = E[:, i].max(); p = m <= th[i]; ok &= p; print(f"  {'PASA ' if p else 'FALLA'} {NAMES[i]:6s} máx {m:.2e} (media {E[:, i].mean():.2e}) <= {th[i]:.2e} = {K_FLOOR:g}×suelo {fl[i]:.2e}; {m / fl[i]:.2f} ulp")
    print(f"  información: |dv| máx {E[:, 4].max():.2e}, |dw| máx {E[:, 5].max():.2e}")
    return (c, cL, G), ok

def load(cap, rider):
    from analyze_capture import read_cap
    return [ic.body(s, next(q for q in s.riders() if q['index'] == rider)) for _, s in read_cap(cap)]

def main():
    ap = argparse.ArgumentParser(); ap.add_argument('cap'); ap.add_argument('--rider', type=int, default=1); ap.add_argument('--eval'); ap.add_argument('--eval-rider', type=int)
    ap.add_argument('--assert', dest='check', action='store_true', help='salida 1 si el nivel modelo falla'); a = ap.parse_args()
    B = load(a.cap, a.rider); other = load(a.eval, a.eval_rider if a.eval_rider is not None else a.rider) if a.eval else B
    _, ok = report(B, other, os.path.basename(a.cap) + (f' -> {os.path.basename(a.eval)}' if a.eval else ''))
    print('NIVEL MODELO:', 'PASA' if ok else 'FALLA', '| NIVEL PROCEDENCIA: PENDIENTE (ajuste empírico)' if None in PROVENANCE.values() else '| NIVEL PROCEDENCIA: localizado')
    if a.check and not ok: sys.exit(1)

if __name__ == '__main__': main()
