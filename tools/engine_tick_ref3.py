#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""T7: referencia Python de UN tick del cuerpo de conducción con la GEOMETRÍA REAL del nivel (.col) y las tres esferas (ruedas 0/1 r = 1.25 y cuerpo r = 2), sin parámetros libres.
Bucle del motor (FUN_00131C90; ~/dh-notes/T7-paso2-impacto.md): hasta 10 iteraciones; primer cruce (distancia al triángulo más cercano <= r + piel) entre las tres esferas;
choque en h = 0 si ya tocan; impulso normal e = 0 (j = -vn/K, vn con d·s60 en ruedas; sin J' ni suspensión en el cuerpo); E0 = F0 = 0 tras el primer impacto con efecto;
rozamiento de rueda (wheelfric, coef0 de la superficie del triángulo) tras cada impacto de rueda; rozamiento de sonda de la rueda en estado 1 sin impacto al final.
Compara con ctrl de la muestra siguiente: ctrl+0x430 (Σj), P, ω, CM, ext y s60. Uso: engine_tick_ref3.py [--level ALPINE_] [--max N] [--assert]"""
import argparse, glob, os, struct, sys
import numpy as np
sys.path.insert(0, os.path.dirname(__file__))
import colgeo, engine_tick_ref as T
F = T.F; u8 = T.u8; DT = T.DT; rot = T.rot; suspstep = T.suspstep; fric = T.fric
SKIN = float(os.environ.get('SKIN', 0.0)); IINV = 0.001

def sph_centre(S, k, cm, M, e):
    loc = S['loc'][k].copy()
    if k < 2: loc[2] = -1.8 - e
    return cm + (loc - S['cml']) @ M

def tick_geo(S, E, Fq, col, coefs, ramp, state, rad, iters=10, dbg=False):
    im = S['im']; cm = S['cm'].copy(); P = S['P'].copy(); L = S['L'].copy(); M = S['M'].copy(); w = S['w'].copy(); su = [list(x) for x in S['susp']]
    E = E.copy(); Fq = Fq.copy(); rem = 1.0; jsum = 0.0; nimp = 0; hit_wheel = [False, False]
    idx = col.near(cm, 16)
    def adv(h):
        nonlocal cm, M, P, L, w, su
        cm = cm + h * P * im; M = rot(M, w, h); P = P + h * E; L = L + h * Fq; w = IINV * L; su = [suspstep(x, h) for x in su]
    def gs(k, hh):
        c2 = cm + hh * P * im; M2 = rot(M, w, hh); e2 = suspstep(su[k], hh)[0] if k < 2 else 0
        return colgeo.probe(col, sph_centre(S, k, c2, M2, e2), rad[k] + SKIN, idx)
    for it in range(iters):
        ign = set()
        while True:
            best = None
            for k in (0, 1, 2):
                if k in ign: continue
                g0, nrm, sf, pt = gs(k, 0.0)
                if g0 <= 0: h = 0.0
                else:
                    hend = rem * DT; h = None; prev = 0.0
                    for i in range(1, 9):
                        hh = hend * i / 8
                        if gs(k, hh)[0] <= 0:
                            lo, hi = prev, hh
                            for _ in range(30):
                                mid = 0.5 * (lo + hi)
                                if gs(k, mid)[0] > 0: lo = mid
                                else: hi = mid
                            h = hi; break
                        prev = hh
                    if h is None: continue
                if best is None or h < best[0] - 1e-12: best = (h, k)
            if best is None: break
            h, k = best
            if h > 0: adv(h); rem -= h / DT
            g0, nrm, sf, pt = gs(k, 0.0)
            if g0 < 0: cm = cm + nrm * (-g0)   # despenetración sin tiempo
            c = sph_centre(S, k, cm, M, su[k][0] if k < 2 else 0); r = c - cm
            if k < 2:
                d = -M[2]; vp = P * im + np.cross(w, r) + d * su[k][2]
            else: vp = P * im + np.cross(w, r)
            vn = nrm @ vp
            if vn < -1e-3:
                K = im + nrm @ np.cross(IINV * np.cross(r, nrm), r); j = -vn / K; J = j * nrm
                Jp = J - 0.25 * d * (J @ d) if k < 2 else J
                P = P + Jp; L = L + np.cross(r, Jp); w = IINV * L
                if k < 2: su[k][2] += 0.005 * (J @ d); hit_wheel[k] = True
                jsum += j; nimp += 1; E[:] = 0; Fq[:] = 0
                if k < 2: P = fric(P * im, nrm, M[0], ramp, coefs[sf & 31], 1) / im
                break
            ign.add(k)
            if h > 0: break     # el avance ya se consumió; reintentar con el resto
        else: pass
        if best is None: break
        if rem * DT < 0.01 * DT: rem = 0
        if rem <= 0: break
        if len(ign) == 3: break
    if rem > 0: adv(rem * DT)
    for k in (0, 1):
        if state[k] == 1 and not hit_wheel[k]:
            g, nrm, sf, pt = gs(k, 0.0)
            if nrm is not None: P = fric(P * im, nrm, M[0], ramp, coefs[sf & 31], 1) / im
    return dict(cm=cm, P=P, v=P * im, L=L, w=w, M=M, susp=su, j=jsum, nimp=nimp)

def main():
    ap = argparse.ArgumentParser(); ap.add_argument('--assert', dest='chk', action='store_true'); ap.add_argument('--max', type=int, default=0); ap.add_argument('--level', default='')
    a = ap.parse_args(); out = []
    for dd in sorted(glob.glob(os.path.expanduser('~/dh-states/awiiwa/dhtel_*%s*TNOS' % a.level))):
        name = os.path.basename(dd).split('_')[2]; col = colgeo.Col(os.path.join(os.path.dirname(__file__), '..', 'out', 'maps', name + '.col'))
        rows = T.C.ticks(dd, True); tb = None
        for r_, _, fs in os.walk(dd):
            if 'static.dhtel' in fs: tb = T.C.S.read_file(os.path.join(r_, 'static.dhtel'))[1][0][2]['surf_table']
        coefs = [struct.unpack_from('<f', tb, q * 32)[0] for q in range(32)]; lastE = np.zeros(3); lastF = np.zeros(3); cnt = 0
        for (t0, c, _), (t1, c1, _) in zip(rows, rows[1:]):
            E1 = F(c1, 0xE0)
            if np.linalg.norm(E1) > 0: lastE = E1; lastF = F(c1, 0xF0)
            if not .019 < t1 - t0 < .021: continue
            jo = F(c1, 0x430, 1)[0]
            S = T.load(c); S['loc'].append(F(c, 0x3E0)); rad = [F(c, 0x2E0 + 0x0C, 1)[0], F(c, 0x360 + 0x0C, 1)[0], F(c, 0x3E0 + 0x0C, 1)[0]]
            ref = T.load(c1); st = (u8(c1, 0x354), u8(c1, 0x3D4))
            Et, Ft = (E1, F(c1, 0xF0)) if np.linalg.norm(E1) > 0 else (lastE, lastF)   # E0 del propio tick si no se consumió; si hubo impacto, el último conocido
            o = tick_geo(S, Et, Ft, col, coefs, min(F(c, 0x48C, 1)[0], 1.0), st, rad)
            ph = int(np.linalg.norm(F(c, 0xE0)) > 0) * 2 + int(np.linalg.norm(E1) > 0)
            out.append((jo > 0, o['nimp'] > 0, np.linalg.norm(o['cm'] - ref['cm']), np.linalg.norm(o['v'] - ref['P'] * ref['im']), np.linalg.norm(o['w'] - ref['w']), abs(o['j'] - jo) / max(jo, 1e-3) if jo > 0 else abs(o['j']), max(abs(o['susp'][q][2] - ref['susp'][q][2]) for q in (0, 1)), ph))
            cnt += 1
            if a.max and cnt >= a.max: break
    A = np.array(out, dtype=float); imp = A[:, 0] > 0
    print(f'ticks: {len(A)}; con impacto en la grabación {imp.sum()}; el modelo detecta impacto en {int((A[:,1]>0).sum())}; acierto de detección {100*((A[:,1]>0)==imp).mean():.1f} %')
    for nm, m in (('con impacto', imp), ('sin impacto', ~imp)):
        if m.sum() == 0: continue
        print(f' {nm} ({m.sum()}):')
        for i, n_ in ((2, '|dCM|'), (3, '|dv|'), (4, '|dw|'), (5, 'Σj rel' if nm == 'con impacto' else 'Σj abs'), (6, '|ds60|')): print(f'   {n_:7s} p50 {np.percentile(A[m,i],50):.4g}  p90 {np.percentile(A[m,i],90):.4g}')
        print(f'   |dv| <= 0.05: {100*(A[m,3]<=.05).mean():.1f} %' + (f';  Σj dentro de 1e-2: {100*(A[m,5]<=1e-2).mean():.1f} %' if nm == 'con impacto' else ''))
    for ph in range(4):
        m = A[:, 7] == ph
        if m.sum() == 0: continue
        mi = m & imp
        print(f' fase (E0 en c={ph>>1}, E0 en c1={ph&1}): {m.sum()} ticks, detección {100*((A[m,1]>0)==imp[m]).mean():.1f} %; con impacto {mi.sum()}: |dv|<=0.05 {100*(A[mi,3]<=.05).mean() if mi.sum() else 0:.0f} %, Σj<=1e-2 {100*(A[mi,5]<=1e-2).mean() if mi.sum() else 0:.0f} %')
    okv = (A[imp, 3] <= .05).mean(); okj = (A[imp, 5] <= 1e-2).mean()
    if a.chk and not (okv >= .9 and okj >= .9): sys.exit(1)
if __name__ == '__main__': main()
