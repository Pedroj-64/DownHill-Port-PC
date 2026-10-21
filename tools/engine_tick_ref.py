#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""T7: referencia Python de UN tick del cuerpo de conducción con un impacto de rueda (FUN_00131C90 + FUN_00132C10, especificación en ~/dh-notes/T7-paso2-impacto.md),
contrastada con las grabaciones ~/dh-states/awiiwa. Entrada: muestra i (inicio) y muestra i+1 (fin) con UNA rueda en estado 0 (impacto en el tick) y ctrl+0x430 > 0.
Modelo: integra h = t·dt con fuerza E (último E0 no nulo) y suspensión; impulso normal e = 0 (j = -vn/K, vn con d·s60), J' = J - 0.25·d(J·d), s60 += 0.005·(J·d);
el resto (1-t)·dt sin fuerzas. t se ajusta (única incógnita no observable). Informa los errores del estado final y de ctrl+0x430.
Uso: engine_tick_ref.py [--assert]    / Python reference for one impact tick of the riding body."""
import argparse, glob, os, struct, sys
import numpy as np
sys.path.insert(0, os.path.dirname(__file__)); sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'telemetry'))
import contact_impulse_check as C
F = C.F
u8 = lambda c, o: struct.unpack_from('<I', c, o)[0] & 0xff
DT = 0.02
def rot(M, w, h):
    a = np.linalg.norm(w) * h
    if a < 1e-12: return M
    k = w / np.linalg.norm(w); K = np.array([[0,-k[2],k[1]],[k[2],0,-k[0]],[-k[1],k[0],0]])
    return M @ (np.eye(3) + np.sin(a) * K + (1 - np.cos(a)) * K @ K).T
def suspstep(w, h):
    e, off, s = w; a = 80 * (0.5 - (e + off)) - (10 if s < 0 else 7) * s
    return [e + s * h, off, s + a * h]

def load(c):
    d = dict(im=F(c, 0, 1)[0], cml=F(c, 0x10), cm=F(c, 0x50), P=F(c, 0x60), L=F(c, 0x70), w=F(c, 0xD0), M=np.array([F(c, 0x1C0 + 0x10 * i) for i in range(3)]))
    d['loc'] = [F(c, 0x2E0 + 0x80 * k) for k in (0, 1)]
    d['susp'] = [[-1.8 - F(c, 0x2E0 + 0x80 * k)[2], F(c, 0x2E0 + 0x80 * k + 0x70, 1)[0], F(c, 0x2E0 + 0x80 * k + 0x60, 1)[0]] for k in (0, 1)]
    return d

def fric(v, n, lat, ramp, coef0, calls):
    lat = lat - n * (lat @ n); lat = lat / np.linalg.norm(lat); cap = 0.0225 * coef0
    for _ in range(calls):
        vt = v - n * (v @ n); sv = lat @ vt; dd = np.sign(sv) * min(abs(sv) * ramp, cap); v = v - lat * dd
    return v

def tick(s, E, Fq, k, n, t, Iinv=0.001, coef0=None, ramp=1.0, calls=1):
    """s: estado inicial; devuelve estado final, j, x250."""
    im = s['im']; cm = s['cm'].copy(); P = s['P'].copy(); L = s['L'].copy(); M = s['M'].copy(); w = s['w'].copy(); su = [list(x) for x in s['susp']]
    h = t * DT
    cm = cm + h * P * im; M = rot(M, w, h); P = P + h * E; L = L + h * Fq; w = Iinv * L; su = [suspstep(x, h) for x in su]
    r = (np.array([s['loc'][k][0], s['loc'][k][1], -1.8 - su[k][0]]) - s['cml']) @ M
    d = -M[2]; vp = P * im + np.cross(w, r) + d * su[k][2]; vn = n @ vp
    if vn > -1e-3: return None
    K = im + n @ np.cross(Iinv * np.cross(r, n), r); j = -vn / K; J = j * n; Jp = J - 0.25 * d * (J @ d)
    P = P + Jp; L = L + np.cross(r, Jp); w = Iinv * L; su[k][2] += 0.005 * (J @ d)
    if coef0 is not None:   # rozamiento DEL IMPACTO (FUN_00132FD8 idx k, tras el normal): eje = orientación en el instante del choque
        P = fric(P * im, n, M[0], ramp, coef0, 1) / im
    h2 = DT - h
    cm = cm + h2 * P * im; M = rot(M, w, h2); su = [suspstep(x, h2) for x in su]
    if coef0 is not None and calls > 1:   # rozamiento de SONDA de la otra rueda (FUN_00135D28): eje de la orientación del tick siguiente (validado en wheelfric_check)
        P = fric(P * im, n, M[0], ramp, coef0, 1) / im
    return dict(cm=cm, P=P, v=P * im, L=L, w=w, M=M, susp=su, j=j, x250=Jp)

def main():
    ap = argparse.ArgumentParser(); ap.add_argument('--assert', dest='chk', action='store_true'); a = ap.parse_args()
    R = []
    for dd in sorted(glob.glob(os.path.expanduser('~/dh-states/awiiwa/dhtel_*_TNOS'))):
        rows = C.ticks(dd, True); tb = None
        for r_, _, fs in os.walk(dd):
            if 'static.dhtel' in fs: tb = C.S.read_file(os.path.join(r_, 'static.dhtel'))[1][0][2]['surf_table']
        coef = [struct.unpack_from('<f', tb, q * 32)[0] for q in range(32)]
        lastE = np.zeros(3); lastF = np.zeros(3)
        for (t0, c, _), (t1, c1, _) in zip(rows, rows[1:]):
            E1 = F(c1, 0xE0)
            if np.linalg.norm(E1) > 0: lastE = E1; lastF = F(c1, 0xF0)
            if not .019 < t1 - t0 < .021 or F(c1, 0x430, 1)[0] <= 0: continue
            idx = [k for k, st in enumerate((u8(c1, 0x354), u8(c1, 0x3D4))) if st == 0]
            if len(idx) != 1: continue
            k = idx[0]; s = load(c); n = F(c1, 0x220); n = n / np.linalg.norm(n); ref = load(c1)
            best = None
            for tt in np.linspace(0, 1, 401):
                o = tick(s, lastE, lastF, k, n, tt, coef0=coef[struct.unpack_from('<H', c1, 0x206)[0] & 31], ramp=min(F(c, 0x48C, 1)[0], 1.0), calls=1 + (u8(c1, 0x354 if k == 1 else 0x3D4) == 1))
                if o is None: continue
                err = abs(o['j'] - F(c1, 0x430, 1)[0]) if os.environ.get('SELJ') else np.linalg.norm(o['cm'] - ref['cm']) / DT + np.linalg.norm(o['v'] - ref['P'] * ref['im'])
                if best is None or err < best[0]: best = (err, tt, o)
            if not best: continue
            _, tt, o = best
            R.append((tt, np.linalg.norm(o['cm'] - ref['cm']), np.linalg.norm(o['v'] - ref['P'] * ref['im']), np.linalg.norm(o['w'] - ref['w']),
                      abs(o['susp'][k][0] - ref['susp'][k][0]), abs(o['susp'][k][2] - ref['susp'][k][2]), abs(o['j'] - F(c1, 0x430, 1)[0]) / F(c1, 0x430, 1)[0],
                      np.linalg.norm(o['x250'] - F(c1, 0x250)) / max(np.linalg.norm(F(c1, 0x250)), 1e-9)))
    A = np.array(R); names = ['t*', '|dCM|', '|dv|', '|dw|', '|de|', '|ds60|', 'rel j', 'rel x250']
    print(f'ticks con un impacto de rueda: {len(A)}')
    for i, nme in enumerate(names): print(f'  {nme:9s} p50 {np.percentile(A[:,i],50):.4g}  p90 {np.percentile(A[:,i],90):.4g}')
    okv = (A[:, 2] <= 0.05).mean(); okj = (A[:, 6] <= 1e-2).mean()
    print(f'|dv| <= 0.05 u/s en {100*okv:.1f} %; j dentro de 1e-2 en {100*okj:.1f} % (PROPUESTA de criterio: >= 90 % ambos)')
    if a.chk and not (okv >= .9 and okj >= .9): sys.exit(1)
if __name__ == '__main__': main()
