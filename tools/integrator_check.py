#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Comprueba el integrador fiel (FUN_00238818, docs/formats/integrator.md) contra savestates de PCSX2. Los savestates/capturas NO van al repo: se pasan por ruta.
Uso:
  integrator_check.py estado.p2s                 -> invariantes de UN estado (v = P*invM, omega = D*L, iw = diag, R ortonormal, nodo = cm - com*R) para todos los pilotos
  integrator_check.py a.p2s b.p2s [--ticks N]    -> predice b desde a con N pasos de 1/50 s (N se ajusta si se omite) y compara posición, orientación y momentos
  integrator_check.py captura.cap                -> lo mismo con cada par de muestras consecutivas de tools/pcsx2/pine_capture.py (la captura trae nodo + módulo de física por paso)
Fuerza y torque se limpian al final de cada paso (FUN_00237C78) y valen 0 en los estados guardados: se infieren de la variación de momentos, (P1-P0)/dt y (L1-L0)/dt.
La referencia de abajo es la misma que src/integrator_fidel.hpp en float64 (tests/integrator_test.cpp cubre la versión C++)."""
import argparse, os, sys
import numpy as np
sys.path.insert(0, os.path.dirname(__file__)); sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'pcsx2'))
from p2s import State

DT = 1 / 50.0   # divisor del paso físico: u32 en 0x2C85FC (gp-0x4B74, FUN_0023F078) = 50 en los savestates PAL
# Criterios de --assert (plan H3, paso 0b; se recalibran con los primeros pares reales). G = hipótesis: 32.17 ft/s² con 1 u = 1 ft (docs/p2s-savestates.md), eje de subida = +z.
G, POS_TOL, R_TOL, FZ_REL, FXY_REL, T_TOL = 32.17, 1e-3, 1e-5, 1e-3, 1e-3, 1.0   # T_TOL: provisional (sin calibrar)

def body(s, r):
    """Campos del cuerpo rígido (cuerpo = módulo + 0x10) y del nodo de un piloto de State.riders()."""
    b = r['phys'] + 0x10; f = lambda o, n=3: np.array(s.f32(b + o, n), float)
    return dict(i=r['index'], type=r['type'], invm=f(0, 1)[0], com=f(0x10), d=f(0x20), pos=f(0x50), P=f(0x60), L=f(0x70), vel=f(0xC0), omega=f(0xD0), F=f(0xE0), T=f(0xF0),
                iw=np.array([s.f32(b + 0x80 + 16 * i, 3) for i in range(3)], float), R=np.array(r['rows'], float), node=np.array(r['pos'], float))

def ortho(R):
    """FUN_00228568: r0 = N(r1 x r2); r1 = N(r2 x r0); r2 = N(r0 x r1)."""
    R = R.copy(); n = lambda v: v / np.linalg.norm(v)
    R[0] = n(np.cross(R[1], R[2])); R[1] = n(np.cross(R[2], R[0])); R[2] = n(np.cross(R[0], R[1])); return R

def step(b, dt, F=None, T=None):
    """FUN_00238818: Euler explícito con vel/omega ANTERIORES; R[i] += dt * (omega x R[i]); reortonormaliza; P += dt*F; L += dt*T; vel = P*invM; omega = D*L (FUN_002389B8)."""
    F = np.zeros(3) if F is None else F; T = np.zeros(3) if T is None else T
    w = b['omega']; S = np.array([[0, -w[2], w[1]], [w[2], 0, -w[0]], [-w[1], w[0], 0]])
    R = ortho(b['R'] + dt * (S @ b['R'].T).T)   # (S R^T)^T = R S^T; fila i = omega x fila i
    P = b['P'] + dt * F; L = b['L'] + dt * T
    out = dict(b); out.update(pos=b['pos'] + dt * b['vel'], R=R, P=P, L=L, vel=P * b['invm'], omega=b['d'] * L); out['node'] = out['pos'] - b['com'] @ R; return out

def selfcheck(s, name):
    print(f'== {name}')
    for r in s.riders():
        b = body(s, r); R = b['R']; Iw = b['iw']; D = np.diag(b['d'])
        print(f"rider{b['i']} t{b['type']}: |v-P*invM|={np.abs(b['vel'] - b['P'] * b['invm']).max():.1e}  |w-iw.L|={np.abs(b['omega'] - Iw @ b['L']).max():.1e}  |w-D.L|={np.abs(b['omega'] - b['d'] * b['L']).max():.1e}  "
              f"|w-R^T D R.L|={np.abs(b['omega'] - R.T @ D @ R @ b['L']).max():.1e}  |iw-D|={np.abs(Iw - D).max():.1e}  |RR^T-I|={np.abs(R @ R.T - np.eye(3)).max():.1e}  |nodo-(cm-com.R)|={np.abs(b['node'] - (b['pos'] - b['com'] @ R)).max():.1f}")

def pair(s0, s1, ticks, label):
    rows = []
    for r0 in s0.riders():
        r1 = next((r for r in s1.riders() if r['index'] == r0['index']), None)
        if r1 is None: continue
        a, c = body(s0, r0), body(s1, r1)
        cand = [ticks] if ticks else range(1, 9)
        best = min(cand, key=lambda n: np.linalg.norm(a['pos'] + n * DT * a['vel'] - c['pos']))
        dt = best * DT; Feff = (c['P'] - a['P']) / dt; Teff = (c['L'] - a['L']) / dt
        # con N>1 la fuerza efectiva promedio no es exacta (la fuerza se recalcula en cada paso); la comprobación de pos usa Euler repetido con vel constante = aproximación
        p = step(a, dt, Feff, Teff)
        rows.append((r0['index'], r0['type'], best, np.abs(p['pos'] - c['pos']).max(), np.abs(p['R'] - c['R']).max(), np.abs(p['vel'] - c['vel']).max(), np.abs(p['omega'] - c['omega']).max(), Feff, np.linalg.norm(c['pos'] - a['pos']), Teff, a['invm']))
    return rows

def evaluate(pos_err, R_err, F, T, invm, t_tol=T_TOL):
    """Criterios del plan para UN par con la bici en el aire: [(nombre, pasa, valor, límite)]. F, T = fuerza y torque efectivos; m = 1/invM."""
    mg = G / invm; fz = abs(F[2] + mg) / mg; fxy = float(np.hypot(F[0], F[1])) / mg; t = float(np.linalg.norm(T))
    return [('|dpos| <= %g' % POS_TOL, pos_err <= POS_TOL, pos_err, POS_TOL), ('max|dR_ij| <= %g' % R_TOL, R_err <= R_TOL, R_err, R_TOL),
            ('F_z = -m*g (rel <= %g)' % FZ_REL, fz <= FZ_REL, fz, FZ_REL), ('|F_xy| <= %g*m*g' % FXY_REL, fxy <= FXY_REL, fxy, FXY_REL), ('|T_eff| <= %g' % t_tol, t <= t_tol, t, t_tol)]

def verdict(rows, label, t_tol=T_TOL):
    """Imprime PASA/FALLA por criterio para el jugador (primer piloto) de cada par; True si todo pasa."""
    ok = True; many = len(rows) > 1   # con varios pares (.cap) sólo se listan los criterios que fallan
    for k, r in enumerate(rows):
        res = evaluate(r[3], r[4], r[7], r[9], r[10], t_tol); ok &= all(p for _, p, _, _ in res)
        if not many or not all(p for _, p, _, _ in res): print(f'== {label}' + (f' par {k}' if many else ''))
        for name, p, v, lim in res:
            if not many or not p: print(f"  {'PASA ' if p else 'FALLA'} {name}: {v:.3e}")
    if many: print(f"{'PASA' if ok else 'FALLA'}: {len(rows)} pares")
    return ok

def show(rows, label):
    print(f'== {label}')
    for i, t, n, ep, eR, ev, ew, F, mv, *_ in rows:
        print(f'rider{i} t{t} ticks={n} |pos|err={ep:.2e} |R|err={eR:.2e} |v|err={ev:.2e} |w|err={ew:.2e}  F_eff=({F[0]:.1f},{F[1]:.1f},{F[2]:.1f})  desplaz.={mv:.3f}')

def main():
    ap = argparse.ArgumentParser(); ap.add_argument('files', nargs='+'); ap.add_argument('--ticks', type=int, default=0)
    ap.add_argument('--assert', dest='check', action='store_true', help='aplica los criterios del plan (jugador, bici en el aire): PASA/FALLA por criterio y código de salida 1 si falla')
    ap.add_argument('--t-tol', type=float, default=T_TOL, help='tolerancia de |T_eff| (provisional)'); a = ap.parse_args()
    if len(a.files) == 1 and a.files[0].endswith('.cap'):
        from analyze_capture import read_cap
        fr = read_cap(a.files[0]); print(f'{len(fr)} muestras')
        agg = []
        for k in range(len(fr) - 1):
            rows = pair(fr[k][1], fr[k + 1][1], a.ticks or 1, k)
            if rows: agg.append(rows[0])   # jugador (primer piloto)
        if agg:
            e = np.array([[r[3], r[4], r[5], r[6]] for r in agg]); print(f'jugador, {len(agg)} pares: mediana |pos|err={np.median(e[:, 0]):.2e}  |R|err={np.median(e[:, 1]):.2e}  p95 pos={np.percentile(e[:, 0], 95):.2e}  p95 R={np.percentile(e[:, 1], 95):.2e}')
            Fz = np.array([r[7] for r in agg]); print('F_eff mediana (x,y,z) =', np.median(Fz, axis=0).round(2), ' (en el aire sólo gravedad: F = m*g con m = 1/invM)')
            if a.check: sys.exit(0 if verdict(agg, os.path.basename(a.files[0]), a.t_tol) else 1)
        elif a.check: sys.exit('--assert: la captura no tiene pares')
    elif len(a.files) == 1:
        if a.check: sys.exit('--assert necesita dos estados o un .cap')
        selfcheck(State(a.files[0]), os.path.basename(a.files[0]))
    else:
        rows = pair(State(a.files[0]), State(a.files[1]), a.ticks, ''); label = f'{os.path.basename(a.files[0])} -> {os.path.basename(a.files[1])}'; show(rows, label)
        if a.check: sys.exit(0 if rows and verdict(rows[:1], label, a.t_tol) else 1)

if __name__ == '__main__': main()
