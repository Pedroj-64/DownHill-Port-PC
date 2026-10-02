#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Compara los puntos de contacto de la física del juego (leídos de un savestate) con la malla de colisión.
Uso: p2s_check.py nivel.col estado1.p2s [estado2.p2s ...] [--sweep-cli build/sweep_cli]
Para cada punto de contacto (esfera de radio r en la RAM) calcula, con la geometría del .col (mismo espacio que la RAM: el del NGP, Z arriba):
  clearance = distancia del centro al triángulo FRONTAL más cercano - r      (el motor deja la esfera tocando el suelo con una holgura de ~0.01 u: FUN_002193C0)
  y la altura/normal/superficie de ese triángulo. Con --sweep-cli añade el resultado de Ground::sweep (hacia -Z, 1 u) para el mismo punto."""
import struct, sys, subprocess, argparse
import numpy as np
sys.path.insert(0, __import__('os').path.dirname(__file__))
from p2s import State

def load_col(path):
    d = open(path, 'rb').read(); n = struct.unpack_from('<I', d)[0]
    a = np.frombuffer(d, dtype=np.dtype([('v', '<f4', 9), ('s', '<u2'), ('p', '<u2')]), count=n, offset=4)
    return a['v'].reshape(-1, 3, 3).astype(np.float64), a['s'].astype(np.int64)

def closest_pt_tri(P, A, B, C):
    """Punto más cercano de P a cada triángulo (Ericson, vectorizado). P: (3,), A,B,C: (n,3)."""
    ab, ac, ap = B - A, C - A, P - A
    d1, d2 = (ab * ap).sum(1), (ac * ap).sum(1)
    bp = P - B; d3, d4 = (ab * bp).sum(1), (ac * bp).sum(1)
    cp = P - C; d5, d6 = (ab * cp).sum(1), (ac * cp).sum(1)
    vc = d1 * d4 - d3 * d2; vb = d5 * d2 - d1 * d6; va = d3 * d6 - d5 * d4
    n = len(A); out = np.empty((n, 3))
    den = va + vb + vc; den = np.where(den == 0, 1, den)
    v = vb / den; w = vc / den; out[:] = A + ab * v[:, None] + ac * w[:, None]           # interior
    m = (d1 <= 0) & (d2 <= 0); out[m] = A[m]
    m = (d3 >= 0) & (d4 <= d3); out[m] = B[m]
    m = (d6 >= 0) & (d5 <= d6); out[m] = C[m]
    m = (vc <= 0) & (d1 >= 0) & (d3 <= 0); t = (d1 / np.where(d1 - d3 == 0, 1, d1 - d3)); out[m] = (A + ab * t[:, None])[m]
    m = (vb <= 0) & (d2 >= 0) & (d6 <= 0); t = (d2 / np.where(d2 - d6 == 0, 1, d2 - d6)); out[m] = (A + ac * t[:, None])[m]
    m = (va <= 0) & ((d4 - d3) >= 0) & ((d5 - d6) >= 0); t = ((d4 - d3) / np.where((d4 - d3) + (d5 - d6) == 0, 1, (d4 - d3) + (d5 - d6))); out[m] = (B + (C - B) * t[:, None])[m]
    return out

def clearance(V, S, c, r, reach=40.0):
    """-> dict(clearance, height, normal, surface, tri) del triángulo frontal más cercano al centro c (dentro de `reach`), o None."""
    lo, hi = V.min(1), V.max(1); m = np.all(lo <= c + reach, 1) & np.all(hi >= c - reach, 1); idx = np.nonzero(m)[0]
    if not len(idx): return None
    A, B, C = V[idx, 0], V[idx, 1], V[idx, 2]
    nrm = np.cross(C - B, A - B); ln = np.linalg.norm(nrm, axis=1); ok = ln > 1e-9; nrm[ok] /= ln[ok, None]            # (v2-v1)x(v0-v1), como FUN_002193C0
    side = ((c - B) * nrm).sum(1)                                                                                          # >= 0: el centro está delante de la cara
    q = closest_pt_tri(c, A, B, C); dist = np.linalg.norm(q - c, axis=1)
    cand = ok & (side >= -1e-6); 
    if not cand.any(): return None
    j = np.nonzero(cand)[0][np.argmin(dist[cand])]
    return dict(clearance=float(dist[j] - r), point=q[j], normal=nrm[j], surface=int(S[idx[j]]), tri=int(idx[j]))

def triangle_for_point(V, S, p, reach=2.0):
    """Triángulo (cualquier cara) más cercano a un punto de contacto del motor -> (índice, distancia, n, d, surface)."""
    lo, hi = V.min(1), V.max(1); idx = np.nonzero(np.all(lo <= p + reach, 1) & np.all(hi >= p - reach, 1))[0]
    if not len(idx): return None
    q = closest_pt_tri(p, V[idx, 0], V[idx, 1], V[idx, 2]); dist = np.linalg.norm(q - p, axis=1); j = int(np.argmin(dist)); t = int(idx[j])
    n = np.cross(V[t, 2] - V[t, 1], V[t, 0] - V[t, 1]); n /= np.linalg.norm(n)
    return t, float(dist[j]), n, float(n @ V[t, 1]), int(S[t])

if __name__ == '__main__':
    ap = argparse.ArgumentParser(); ap.add_argument('col'); ap.add_argument('states', nargs='+'); ap.add_argument('--sweep-cli'); a = ap.parse_args()
    from p2s import hit_records
    V, S = load_col(a.col)
    for sp in a.states:
        st = State(sp); nm = sp.split('/')[-1]; rs = st.riders()
        print(f'\n=== {nm}: {len(rs)} pilotos (1 u ~ 0.3048 m); jugador: ' + ', '.join(f"{r['pos'][i]:.1f}" for r in rs[:1] for i in range(3)))
        print('A) puntos de contacto de la física (RAM) frente a la malla: clearance = distancia al triángulo frontal más cercano - radio')
        print('   piloto tipo punto  centro(x,y,z)                 r     clearance  z_suelo   normal(malla)          surface')
        for rd in rs:
            for k, (loc, w, rad) in enumerate(rd['contacts']):
                res = clearance(V, S, np.array(w), rad)
                if res is None: print(f'   {rd["index"]:4} {rd["type"]:4} {k:5}  {tuple(round(x,1) for x in w)}  {rad:.2f}  sin suelo cercano'); continue
                print(f'   {rd["index"]:4} {rd["type"]:4} {k:5}  {tuple(round(x,1) for x in w)}  {rad:.2f}  {res["clearance"]:+9.3f}  {res["point"][2]:8.2f}  {tuple(float(round(x,3)) for x in res["normal"])}  {res["surface"]:#06x}')
        print('B) registros de impacto del motor que quedan en la pila (0x30 B) frente al triángulo de la malla más cercano a su punto')
        print('   addr      surface(motor) surface(malla)  frac     |dn|max   d(motor)   d(malla)   dist(punto,tri)  tri')
        near = [r for r in rs if r['type'] == 1] or rs
        for h in hit_records(st):
            p = np.array(h['point'])
            if min(np.linalg.norm(p - np.array(c[1])) for r in near for c in r['contacts']) > 3.0: continue    # sólo los de la bici del jugador
            t = triangle_for_point(V, S, p)
            if t is None: continue
            ti, dist, n, d, surf = t
            print(f'   {h["addr"]:#09x}  {h["surface"]:#06x}         {surf:#06x}          {h["frac"]:.4f}  {np.abs(n-np.array(h["normal"])).max():.2e}  {h["d"]:10.3f} {d:10.3f}   {dist:.4f}          {ti}')
        if a.sweep_cli:
            print('C) Ground::sweep (esfera r hacia -Z, 2 u de recorrido) en cada punto de contacto del jugador')
            segs = [(w[0], w[1], w[2] + 1.0, w[0], w[1], w[2] - 1.0, rad) for r in near for _, w, rad in r['contacts']]
            out = subprocess.run([a.sweep_cli, a.col], input=''.join(' '.join(repr(float(x)) for x in s) + '\n' for s in segs), capture_output=True, text=True).stdout.splitlines()
            for s_, o in zip(segs, out): print('  ', tuple(round(x, 1) for x in s_[:3]), o)
