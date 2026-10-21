#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Geometría de colisión para las referencias de T7: carga un .col (u32 n, n·{9 f32, u16 superficie, u16 relleno}, espacio NGP) y devuelve, para una esfera, la distancia al triángulo
más cercano y su normal (punto más cercano: cara, arista o vértice). Numpy, sin dependencias. / Collision geometry helper: nearest-triangle distance and normal for a sphere."""
import numpy as np

class Col:
    def __init__(self, path):
        raw = open(path, 'rb').read(); n = int(np.frombuffer(raw[:4], '<u4')[0])
        dt = np.dtype([('v', '<f4', 9), ('s', '<u2'), ('p', '<u2')])
        a = np.frombuffer(raw, dt, count=n, offset=4)
        self.T = a['v'].astype(np.float64).reshape(n, 3, 3); self.surf = a['s'].astype(np.int64)
        self.lo = self.T.min(axis=1); self.hi = self.T.max(axis=1)
    def near(self, c, rad):
        m = np.all(self.hi >= c - rad, axis=1) & np.all(self.lo <= c + rad, axis=1)
        return np.nonzero(m)[0]

def _seg(p, a, b):
    ab = b - a; t = np.clip(((p - a) * ab).sum(1) / np.maximum((ab * ab).sum(1), 1e-30), 0, 1); return a + ab * t[:, None]

def closest(c, T):
    """c: (3,), T: (N,3,3). -> (punto (N,3), distancia (N,))"""
    A, B, C = T[:, 0], T[:, 1], T[:, 2]; n = np.cross(B - A, C - A); nl = np.linalg.norm(n, axis=1); nl = np.maximum(nl, 1e-30); n = n / nl[:, None]
    q = c - ((c - A) * n).sum(1)[:, None] * n
    def side(P, Q): return (np.cross(Q - P, q - P) * n).sum(1) >= 0
    inside = side(A, B) & side(B, C) & side(C, A)
    best = q.copy(); bd = np.full(len(T), np.inf)
    for P, Q in ((A, B), (B, C), (C, A)):
        s = _seg(c, P, Q); d = np.linalg.norm(c - s, axis=1); m = d < bd; best[m] = s[m]; bd[m] = d[m]
    best[inside] = q[inside]; d = np.linalg.norm(c - best, axis=1)
    return best, d

def probe(col, c, R, idx=None):
    """Esfera de radio R en c: devuelve (g, normal, superficie, punto) con g = distancia mínima - R."""
    if idx is None: idx = col.near(c, R + 4)
    if len(idx) == 0: return 1e9, None, 0, None
    P, d = closest(c, col.T[idx]); i = int(np.argmin(d)); dv = c - P[i]; nl = np.linalg.norm(dv)
    if nl < 1e-9:
        T = col.T[idx[i]]; nrm = np.cross(T[1] - T[0], T[2] - T[0]); ln = np.linalg.norm(nrm)
        nrm = nrm / ln if ln > 1e-12 else np.array([0., 0., 1.])   # triángulo degenerado: normal arbitraria hacia arriba (Z arriba en el NGP)
    else: nrm = dv / nl
    return d[i] - R, nrm, int(col.surf[idx[i]]), P[i]
