#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""T7: referencia Python de UN tick del cuerpo de conducción con barrido iterativo contra el PLANO del último choque registrado (ctrl+0x210 punto, +0x220 normal),
SIN parámetros libres (engine_tick_ref.py ajustaba t). Esfera de rueda r = 1.25 (medido: n·(centro - p) = 1.25 ± 0.15 en los ticks de impacto; la escala 4,1,1 no es del barrido).
Bucle del motor (FUN_00131C90, ~/dh-notes/T7-paso2-impacto.md): hasta 10 iteraciones; en cada una se busca el primer cruce del plano con la rueda; si g(0) <= 0 se despenetra por n
(sin consumir tiempo) y el choque ocurre en h = 0; el impulso normal (e = 0, j = -vn/K, vn con d·s60) solo vale si vn < -1e-3; tras el primer impacto con efecto E0 = F0 = 0;
el rozamiento (wheelfric) va tras cada impacto con efecto; la otra rueda en estado 1 recibe rozamiento de sonda al final con el eje de la orientación final.
Salida: errores del estado final y de ctrl+0x430 (= Σ j). Uso: engine_tick_ref2.py [--assert] [--max N]   / iterative-sweep reference, no fitted parameters."""
import argparse, glob, os, struct, sys
import numpy as np
sys.path.insert(0, os.path.dirname(__file__))
import engine_tick_ref as T
F = T.F; u8 = T.u8; DT = T.DT; rot = T.rot; suspstep = T.suspstep; fric = T.fric
R = 1.25; IINV = 0.001

def centre(cm, M, loc, cml, e): return cm + (np.array([loc[0], loc[1], -1.8 - e]) - cml) @ M

def tick_multi(s, E, Fq, k, n, p, coef0, ramp, sonda, iters=10):
    im = s['im']; cm = s['cm'].copy(); P = s['P'].copy(); L = s['L'].copy(); M = s['M'].copy(); w = s['w'].copy(); su = [list(x) for x in s['susp']]
    E = E.copy(); Fq = Fq.copy(); rem = 1.0; jsum = 0.0; x250 = np.zeros(3); nimp = 0
    def adv(h):
        nonlocal cm, M, P, L, w, su
        cm = cm + h * P * im; M = rot(M, w, h); P = P + h * E; L = L + h * Fq; w = IINV * L; su = [suspstep(x, h) for x in su]
    for it in range(iters):
        g0 = n @ (centre(cm, M, s['loc'][k], s['cml'], su[k][0]) - p) - R
        if g0 <= 0:
            cm = cm + n * (-g0); h = 0.0        # despenetración sin tiempo
        else:
            # cruce del plano dentro del resto del tick (bisección sobre una copia del estado)
            def g_at(hh):
                c2 = cm + hh * P * im; M2 = rot(M, w, hh); su2 = suspstep(su[k], hh) if hh else su[k]
                return n @ (centre(c2, M2, s['loc'][k], s['cml'], su2[0]) - p) - R
            hend = rem * DT
            if g_at(hend) > 0: adv(hend); rem = 0; break
            lo, hi = 0.0, hend
            for _ in range(40):
                mid = 0.5 * (lo + hi)
                if g_at(mid) > 0: lo = mid
                else: hi = mid
            h = hi; adv(h); rem -= h / DT
        # respuesta
        if os.environ.get('DBG'): print('   it',it,'h',round(h/DT,4),'g0',round(g0,4),'rem',round(rem,4))
        r = (np.array([s['loc'][k][0], s['loc'][k][1], -1.8 - su[k][0]]) - s['cml']) @ M
        d = -M[2]; vp = P * im + np.cross(w, r) + d * su[k][2]; vn = n @ vp
        if vn < -1e-3:
            K = im + n @ np.cross(IINV * np.cross(r, n), r); j = -vn / K; J = j * n; Jp = J - 0.25 * d * (J @ d)
            if os.environ.get('DBG'): print('     j',round(j,2),'vn',round(vn,3))
            P = P + Jp; L = L + np.cross(r, Jp); w = IINV * L; su[k][2] += 0.005 * (J @ d); jsum += j; x250 = x250 + Jp
            E[:] = 0; Fq[:] = 0
            P = fric(P * im, n, M[0], ramp, coef0, 1) / im
            if h == 0.0 and False: pass
        elif h > 0.0 or True:
            pass
        if rem * DT < 0.01 * DT: rem = 0
        if rem <= 0: break
        if h == 0.0 and vn >= -1e-3:
            # respuesta sin efecto en h = 0: el motor integra el paso completo restante
            hend = rem * DT; adv(hend); rem = 0; break
    if rem > 0: adv(rem * DT)
    if sonda: P = fric(P * im, n, M[0], ramp, coef0, 1) / im
    return dict(cm=cm, P=P, v=P * im, L=L, w=w, M=M, susp=su, j=jsum, x250=x250, nimp=nimp)

def main():
    ap = argparse.ArgumentParser(); ap.add_argument('--assert', dest='chk', action='store_true'); ap.add_argument('--max', type=int, default=0); ap.add_argument('--level', default='')
    a = ap.parse_args(); out = []
    for dd in sorted(glob.glob(os.path.expanduser('~/dh-states/awiiwa/dhtel_*%s*TNOS' % a.level))):
        rows = T.C.ticks(dd, True); tb = None
        for r_, _, fs in os.walk(dd):
            if 'static.dhtel' in fs: tb = T.C.S.read_file(os.path.join(r_, 'static.dhtel'))[1][0][2]['surf_table']
        coef = [struct.unpack_from('<f', tb, q * 32)[0] for q in range(32)]; lastE = np.zeros(3); lastF = np.zeros(3); cnt = 0
        for (t0, c, _), (t1, c1, _) in zip(rows, rows[1:]):
            E1 = F(c1, 0xE0)
            if np.linalg.norm(E1) > 0: lastE = E1; lastF = F(c1, 0xF0)
            if not .019 < t1 - t0 < .021 or F(c1, 0x430, 1)[0] <= 0: continue
            st = (u8(c1, 0x354), u8(c1, 0x3D4)); idx = [k for k, x in enumerate(st) if x == 0]
            if len(idx) != 1: continue
            k = idx[0]; s = T.load(c); ref = T.load(c1); n = F(c1, 0x220); n = n / np.linalg.norm(n); p = F(c1, 0x210)
            o = tick_multi(s, lastE, lastF, k, n, p, coef[struct.unpack_from('<H', c1, 0x206)[0] & 31], min(F(c, 0x48C, 1)[0], 1.0), st[1 - k] == 1)
            jo = F(c1, 0x430, 1)[0]
            cen0 = centre(s['cm'], s['M'], s['loc'][k], s['cml'], s['susp'][k][0]); g0 = n @ (cen0 - p) - R
            r0 = (np.array([s['loc'][k][0], s['loc'][k][1], -1.8 - s['susp'][k][0]]) - s['cml']) @ s['M']; vn0 = n @ (s['P'] * s['im'] + np.cross(s['w'], r0) - s['M'][2] * s['susp'][k][2])
            out.append((np.linalg.norm(o['cm'] - ref['cm']), np.linalg.norm(o['v'] - ref['P'] * ref['im']), np.linalg.norm(o['w'] - ref['w']), abs(o['j'] - jo) / jo, abs(o['susp'][k][2] - ref['susp'][k][2]), g0, vn0))
            cnt += 1
            if a.max and cnt >= a.max: break
    A = np.array(out); S = (np.abs(A[:, 5]) < 0.3) & (A[:, 6] < -1e-3)   # estrato A: contacto con el plano ya conocido al inicio del tick (rueda penetrando y acercándose)
    names = ['|dCM|', '|dv|', '|dw|', 'rel Σj', '|ds60|']
    print(f'ticks con un impacto de rueda: {len(A)} (sin parámetros libres)')
    for i, nm in enumerate(names): print(f'  {nm:7s} p50 {np.percentile(A[:,i],50):.4g}  p90 {np.percentile(A[:,i],90):.4g}')
    okv = (A[:, 1] <= 0.05).mean(); okj = (A[:, 3] <= 1e-2).mean()
    print(f'|dv| <= 0.05 u/s en {100*okv:.1f} %; Σj dentro de 1e-2 en {100*okj:.1f} % (PROPUESTA: >= 90 % ambos)')
    print(f'ESTRATO A (|g0| < 0.3 y vn0 < 0: plano conocido al inicio): {S.sum()} ticks ({100*S.mean():.1f} %)')
    for i, nm in enumerate(names[:4]): print(f'  {nm:7s} p50 {np.percentile(A[S,i],50):.4g}  p90 {np.percentile(A[S,i],90):.4g}')
    okvA = (A[S, 1] <= 0.05).mean(); okjA = (A[S, 3] <= 1e-2).mean(); print(f'  estrato A: |dv| <= 0.05 en {100*okvA:.1f} %; Σj dentro de 1e-2 en {100*okjA:.1f} %')
    if a.chk and not (okvA >= .9 and okjA >= .9): sys.exit(1)
if __name__ == '__main__': main()
