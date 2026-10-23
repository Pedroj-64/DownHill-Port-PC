#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""T7: referencia Python de UN tick del cuerpo de conducción con el bucle de FUN_00131C90 tal como sale de la decompilación (HIPÓTESIS hasta ≥ 90 % en Σj en los 4 niveles):
por iteración (≤ 9) paso f = min(resto, (50·π/2)/|ω|); barrido lineal de las 3 esferas del centro inicial al final (sweep_ref.py) con deformación ELIPSOIDAL de los triángulos
(matriz del nodo y escala `esfera+0x10` = (4,1,1) en ruedas r = 1.25, (2,1,1) en el cuerpo r = 2; activa porque DAT_002c5384 = 1 en la RAM grabada); impacto más temprano;
t < 0 (solape estático) => despenetración FUN_00131490 (desplaza el CM, sin impulso) y nueva iteración; t >= 0 => avance f·t, impulso normal j = -vn/K (FUN_00132C10/FUN_00130f00)
y rozamiento de rueda (wheelfric); impactos simultáneos (mismo t) en la misma iteración; sin impulso efectivo => se avanza el paso entero. Suspensión con u ∈ [0, 1] acotado (FUN_00133810).
Compara con la muestra siguiente de ~/dh-states/awiiwa (Σj = ctrl+0x430, CM, v, ω). Uso: engine_tick_ref4.py [--level ALPINE_] [--max N] [--skip N]
/ T7 Python reference of one riding-body tick using the engine's real collision loop (ellipsoid sweeps; hypotheses until the Σj criterion is met)."""
import argparse, glob, os, struct, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), 'telemetry'))
import colgeo, engine_tick_ref as T, sweep_ref as W
F = T.F; u8 = T.u8; DT = T.DT; rot = T.rot; suspstep = T.suspstep; fric = T.fric
IINV = 0.001
def suspstep2(w, h):
    e, off, sv = w; u = 0.5 + e + off
    if u <= 0: u = 0.0; sv = sv if sv >= 0 else 0.0
    elif u >= 1: u = 1.0; sv = sv if sv <= 0 else 0.0
    c = 10 if sv < 0 else 7
    nu = min(max(u + sv * h, 0.0), 1.0)
    return [nu - 0.5 - off, off, sv + (80 * (1 - u) - c * sv) * h]
suspstep = suspstep2

def centre(S, k, cm, M, e):
    loc = S['loc'][k].copy()
    if k < 2: loc[2] = -1.8 - e
    return cm + (loc - S['cml']) @ M

def tick(S, E, Fq, col, coefs, ramp, state, rad, scl):
    im = S['im']; st = dict(cm=S['cm'].copy(), P=S['P'].copy(), L=S['L'].copy(), M=S['M'].copy(), w=S['w'].copy(), su=[list(x) for x in S['susp']])
    E = E.copy(); Fq = Fq.copy(); rem = 1.0; jsum = 0.0; nimp = 0; hitw = [False, False]; npen = 0; it = 1; ks = []; ts = []
    def adv(s, h):
        s = dict(s); s['cm'] = s['cm'] + h * s['P'] * im; s['M'] = rot(s['M'], s['w'], h); s['P'] = s['P'] + h * E; s['L'] = s['L'] + h * Fq; s['w'] = IINV * s['L']; s['su'] = [suspstep(x, h) for x in s['su']]
        return s
    def cen(s): return [centre(S, k, s['cm'], s['M'], s['su'][k][0] if k < 2 else 0) for k in range(3)]
    while True:
        wn = np.linalg.norm(st['w']); f = rem if wn < 1e-9 else min(rem, (50 * 1.5707964) / wn)
        c0 = cen(st); st1 = adv(st, f * DT); c1 = cen(st1)
        hits = []
        for k in range(3):
            for h in W.sweep(col, c0[k], c1[k], rad[k], 10, st['M'], scl[k]): hits.append((h['t'], k, len(hits), h))
        hits.sort(key=lambda x: (x[0], x[1], x[2]))
        advanced = False
        if not hits: st = st1; rem -= f; advanced = True
        elif hits[0][0] < 0:
            pen = [x for x in hits if x[0] < 0]; zer = [x for x in hits if x[0] == 0]; used = pen + zer
            sm = np.zeros(3)
            for i, x in enumerate(pen):
                v = x[3]['n'] * x[3]['depth']
                for j, y in enumerate(used):
                    if j != i:
                        dd = v @ y[3]['n']
                        if dd < 0: v = v + y[3]['n'] * (-dd)
                sm = sm + v
            ln = np.linalg.norm(sm)
            if ln < 1e-12: pass
            else:
                dr = sm / ln; sc = 0.0
                for x in pen:
                    q = x[3]['depth'] / (dr @ x[3]['n']) if dr @ x[3]['n'] != 0 else 0.0; sc = max(sc, q)
                push = dr * sc
                if np.linalg.norm(push) > 1.0: return dict(err=True, cm=st['cm'], P=st['P'], v=st['P'] * im, L=st['L'], w=st['w'], M=st['M'], susp=st['su'], j=jsum, nimp=nimp)
                st['cm'] = st['cm'] + push
            npen += 1
        else:
            done = False; i = 0
            while i < len(hits):
                t, k, _, h = hits[i]; i += 1
                stt = adv(st, f * t * DT)
                ck = centre(S, k, stt['cm'], stt['M'], stt['su'][k][0] if k < 2 else 0); r = ck - stt['cm']; nrm = h['n']
                if k < 2: d = -stt['M'][2]; vp = stt['P'] * im + np.cross(stt['w'], r) + d * stt['su'][k][2]
                else: vp = stt['P'] * im + np.cross(stt['w'], r)
                vn = nrm @ vp
                if vn < -1e-3:
                    K = im + nrm @ np.cross(IINV * np.cross(r, nrm), r); j = -vn / K; J = j * nrm
                    Jp = J - 0.25 * d * (J @ d) if k < 2 else J
                    stt['P'] = stt['P'] + Jp; stt['L'] = stt['L'] + np.cross(r, Jp); stt['w'] = IINV * stt['L']
                    if k < 2: stt['su'][k][2] += 0.005 * (J @ d); hitw[k] = True
                    jsum += j; nimp += 1; E[:] = 0; Fq[:] = 0; ks.append(k); ts.append(round(float(t), 4))
                    if k < 2: stt['P'] = fric(stt['P'] * im, nrm, stt['M'][0], ramp, coefs[h['surf'] & 31], 1) / im
                    st = stt; rem -= f * t; done = True
                    # impactos simultáneos
                    while i < len(hits) and hits[i][0] == t:
                        _, k2, _, h2 = hits[i]; i += 1
                        ck = centre(S, k2, st['cm'], st['M'], st['su'][k2][0] if k2 < 2 else 0); r = ck - st['cm']; n2 = h2['n']
                        if k2 < 2: d2 = -st['M'][2]; vp = st['P'] * im + np.cross(st['w'], r) + d2 * st['su'][k2][2]
                        else: d2 = None; vp = st['P'] * im + np.cross(st['w'], r)
                        vn2 = n2 @ vp
                        if vn2 < -1e-3:
                            K = im + n2 @ np.cross(IINV * np.cross(r, n2), r); j = -vn2 / K; J = j * n2
                            Jp = J - 0.25 * d2 * (J @ d2) if k2 < 2 else J
                            st['P'] = st['P'] + Jp; st['L'] = st['L'] + np.cross(r, Jp); st['w'] = IINV * st['L']
                            if k2 < 2: st['su'][k2][2] += 0.005 * (J @ d2); hitw[k2] = True
                            jsum += j; nimp += 1; ks.append(k2); ts.append(round(float(t), 4))
                            if k2 < 2: st['P'] = fric(st['P'] * im, n2, st['M'][0], ramp, coefs[h2['surf'] & 31], 1) / im
                    break
            if not done: st = adv(st, f * DT); rem -= f
        if rem < 0.01: rem = 0.0
        it += 1
        if rem <= 0 or it > 9: break
    for k in (0, 1):
        if state[k] == 1 and not hitw[k]:
            g, nrm, sf, pt = colgeo.probe(col, centre(S, k, st['cm'], st['M'], st['su'][k][0]), rad[k], None)
            if nrm is not None: st['P'] = fric(st['P'] * im, nrm, st['M'][0], ramp, coefs[sf & 31], 1) / im
    return dict(err=False, cm=st['cm'], P=st['P'], v=st['P'] * im, L=st['L'], w=st['w'], M=st['M'], susp=st['su'], j=jsum, nimp=nimp, npen=npen, ks=ks, ts=ts)

def main():
    ap = argparse.ArgumentParser(); ap.add_argument('--max', type=int, default=0); ap.add_argument('--level', default=''); ap.add_argument('--skip', type=int, default=0)
    a = ap.parse_args(); out = []
    for dd in sorted(glob.glob(os.path.expanduser('~/dh-states/awiiwa/dhtel_*%s*TNOS' % a.level))):
        name = os.path.basename(dd).split('_')[2]; col = colgeo.Col(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'out', 'maps', name + '.col'))
        rows = T.C.ticks(dd, True)
        for r_, _, fs in os.walk(dd):
            if 'static.dhtel' in fs: tb = T.C.S.read_file(os.path.join(r_, 'static.dhtel'))[1][0][2]['surf_table']
        coefs = [struct.unpack_from('<f', tb, q * 32)[0] for q in range(32)]; lastE = np.zeros(3); lastF = np.zeros(3); lastR = np.zeros(3); cnt = 0
        for (t0, c, _), (t1, c1, _) in zip(rows, rows[1:]):
            E1 = F(c1, 0xE0)
            if np.linalg.norm(E1) > 0: lastE = E1; lastF = F(c1, 0xF0); lastR = E1 - F(c1, 0x100) - F(c1, 0x110)
            if not .019 < t1 - t0 < .021 or c1[0x60:0x80] == c[0x60:0x80]: continue   # fuera los ticks sin integrar (P y L idénticos: el cuerpo de conducción no corre, p. ej. caída)
            cnt += 1
            if cnt <= a.skip: continue
            jo = F(c1, 0x430, 1)[0]
            S = T.load(c); S['loc'].append(F(c, 0x3E0)); rad = [F(c, 0x2E0 + 0x0C, 1)[0], F(c, 0x360 + 0x0C, 1)[0], F(c, 0x3E0 + 0x0C, 1)[0]]
            ref = T.load(c1); st = (u8(c1, 0x354), u8(c1, 0x3D4))
            Et, Ft = (E1, F(c1, 0xF0)) if np.linalg.norm(E1) > 0 else (lastE, lastF)
            if np.linalg.norm(E1) == 0: Et = F(c1, 0x100) + F(c1, 0x110) + lastR   # peso y arrastre sobreviven al borrado de E0 en el impacto; resto (pedal, rodadura) del último tick sin impacto
            scl = [F(c, 0x2E0 + 0x10), F(c, 0x360 + 0x10), F(c, 0x3E0 + 0x10)]; o = tick(S, Et, Ft, col, coefs, min(F(c, 0x48C, 1)[0], 1.0), st, rad, scl)
            ph = int(np.linalg.norm(F(c, 0xE0)) > 0) * 2 + int(np.linalg.norm(E1) > 0)
            out.append((jo > 0, o['nimp'] > 0, np.linalg.norm(o['cm'] - ref['cm']), np.linalg.norm(o['v'] - ref['P'] * ref['im']), np.linalg.norm(o['w'] - ref['w']), abs(o['j'] - jo) / max(jo, 1e-3) if jo > 0 else abs(o['j']), max(abs(o['susp'][q][2] - ref['susp'][q][2]) for q in (0, 1)), ph))
            if a.max and len(out) >= a.max: break
    A = np.array(out, dtype=float); imp = A[:, 0] > 0
    print(f'ticks: {len(A)}; con impacto en la grabación {imp.sum()}; el modelo detecta {int((A[:,1]>0).sum())}; acierto de detección {100*((A[:,1]>0)==imp).mean():.1f} %')
    for nm, m in (('con impacto', imp), ('sin impacto', ~imp)):
        if m.sum() == 0: continue
        print(f' {nm} ({m.sum()}): |dCM| p50 {np.percentile(A[m,2],50):.4g} p90 {np.percentile(A[m,2],90):.4g}; |dv| p50 {np.percentile(A[m,3],50):.4g} p90 {np.percentile(A[m,3],90):.4g}; |dv|<=.05: {100*(A[m,3]<=.05).mean():.1f} %' + (f'; Σj<=1e-2: {100*(A[m,5]<=1e-2).mean():.1f} %' if nm == 'con impacto' else ''))
    for ph in range(4):
        m = A[:, 7] == ph
        if m.sum(): mi = m & imp; print(f' fase {ph}: {m.sum()} ticks, detección {100*((A[m,1]>0)==imp[m]).mean():.1f} %' + (f'; con impacto {mi.sum()}: |dv|<=.05 {100*(A[mi,3]<=.05).mean():.0f} %, Σj<=1e-2 {100*(A[mi,5]<=1e-2).mean():.0f} %' if mi.sum() else ''))
if __name__ == '__main__': main()

