#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""T7: barrido de esfera/elipsoide contra triángulos de colisión, transcripción en float64 de la lógica de FUN_002193c0 (triángulo), FUN_00218fb8 (arista/vértice),
FUN_00218e00 (solape estático con arista) y FUN_00219970 (lista de impactos por barrido, con la deformación elipsoidal M^T·S·M de los triángulos respecto al centro inicial).
Reglas leídas en la decompilación (HIPÓTESIS hasta validar contra 54 niveles): triángulos de una sola cara (centro inicial detrás del plano = sin impacto), solape estático => t = -FLT_MAX
(despenetración, no impulso) con profundidad R + 0.01 - d, barrido dinámico con t' = t - 0.01/(d0 - d1) recortado a 0, varios impactos simultáneos solo con t <= 0.
Biblioteca para engine_tick_ref4.py. / T7: sphere/ellipsoid-vs-collision-triangle sweep (float64 transcription; hypotheses until validated on all 54 levels)."""
import numpy as np
BIG = -3.4028235e38
SK = 0.01

def _inside(P, A, B, C):
    v1 = np.cross(P - A, P - B); v2 = np.cross(P - B, P - C); v3 = np.cross(P - C, P - A)
    return v1 @ v2 > 0 and v1 @ v3 > 0

def _edge(R, cur, p0, d, bmin, bmax, Va, Vb):
    """FUN_00218fb8. devuelve (t, punto, normal|None, depth) o None"""
    for i in range(3):
        lo, hi = (Va[i], Vb[i]) if Va[i] < Vb[i] else (Vb[i], Va[i])
        if bmax[i] < lo or hi < bmin[i]: return None
    s = p0 - Va; e = Vb - Va; se = s @ e; ee = e @ e
    # estático (FUN_00218e00)
    if se < ee:
        if se < 0:
            if R * R < s @ s: pass
            else:
                L = np.sqrt(s @ s); n = s / L
                return (BIG, Va + n * SK, n, (R - L) + SK)
        else:
            q = Va + e * (se / ee); w = p0 - q
            if R * R >= w @ w:
                L = np.sqrt(w @ w); n = w / L
                return (BIG, q + n * SK, n, (R - L) + SK)
    ed = e @ d; sd = s @ d; ss = s @ s; dd = d @ d
    b = 2 * (se * ed - sd * ee); a = ed * ed - ee * dd
    if abs(a) > 1e-4:
        disc = b * b - a * ((se * se + R * R * ee) - ss * ee) * 4.0
        if disc <= 0: return None
        sq = np.sqrt(disc); t1 = (-b - sq) / (2 * a); t2 = (-b + sq) / (2 * a); t = min(t1, t2)
        if t < 0 or cur < t: return None
        u = ((p0 + d * t - Va) @ e) / ee
        if 0 <= u <= 1:
            q = Va + e * u; return (t, q, None, 0.0)
    b2 = 2 * sd
    if abs(dd) < 1e-4: return None
    disc = b2 * b2 - dd * (ss - R * R) * 4.0
    if disc <= 0: return None
    sq = np.sqrt(disc); t = min((-b2 - sq) / (2 * dd), (-b2 + sq) / (2 * dd))
    if t < 0: return None
    if t <= cur: return (t, Va.copy(), None, 0.0)
    return None

def tri_hit(R, cur, p0, p1, d, bmin, bmax, T):
    """FUN_002193c0. T: (3,3). devuelve dict(t, n, pt, depth) o None"""
    A, B, C = T
    n = np.cross(C - B, A - B); ln = np.linalg.norm(n)
    if ln < 1e-30: return None
    n = n / ln; dpl = n @ B
    d0 = n @ p0 - dpl
    if d0 < 0: return None
    d0r = d0 - R
    if d0r <= 0:
        P = p0 + n * (SK - d0)
        if _inside(P, A, B, C): return dict(t=BIG, n=n, pt=P, depth=SK - d0r)
    else:
        d1 = n @ p1 - dpl - R
        if d1 > 0: return None
        t = d0r / (d0r - d1)
        if t < 0 or t > 1: return None
        t2 = t - SK / (d0r - d1)
        if cur < t2: return None
        P = p0 - n * R + d * t
        if _inside(P, A, B, C):
            tc = max(t2, 0.0); return dict(t=tc, n=n, pt=p0 - n * R + d * tc, depth=SK - d1)
    if R == 0: return None
    best = None; ce = cur
    for i in range(3):
        h = _edge(R, ce, p0, d, bmin, bmax, T[i], T[(i + 1) % 3])
        if h is not None and (best is None or h[0] < best[0]):
            best = h; ce = max(h[0], 0.0)
    if best is None: return None
    t, q, nn, dep = best
    if t < 0: return dict(t=t, n=nn, pt=q, depth=dep)
    nrm = ((p0 + d * t) - q) / R
    return dict(t=t, n=nrm, pt=q, depth=0.0)

def _stretch(M, sc, p0):
    A = M.T @ np.diag(sc) @ M          # v' = p0 + M^T S M (v - p0)
    Ai = M.T @ np.diag(1.0 / sc) @ M
    return A, Ai

def sweep(col, p0, p1, R, cap=10, M=None, sc=None):
    """FUN_00219970 filtrado por rejilla sustituido por fuerza bruta en la caja. -> lista de impactos (orden de índice de triángulo)"""
    lo = np.minimum(p0, p1) - R; hi = np.maximum(p0, p1) + R
    m = np.all(col.hi >= lo, axis=1) & np.all(col.lo <= hi, axis=1)
    idx = np.nonzero(m)[0]; d = p1 - p0; cur = 1.0; hits = []
    if M is not None: A, Ai = _stretch(M, sc, p0)
    for i in idx:
        Tt = col.T[i]
        if M is not None: Tt = p0 + (Tt - p0) @ A.T
        h = tri_hit(R, cur, p0, p1, d, lo, hi, Tt)
        if h is None: continue
        if M is not None:
            ns = h['n']; nn = M.T @ (sc * (M @ ns)); h['n'] = nn / np.linalg.norm(nn)
            h['pt'] = p0 + Ai @ (h['pt'] - p0)
            if h['depth'] != 0.0: h['depth'] = float(np.linalg.norm(Ai @ (ns * h['depth'])))
        t = h['t']; h['surf'] = int(col.surf[i]); h['tri'] = int(i)
        if t > 0 or cur > 0:
            if cur <= t: continue
            hits = [h]
        else:
            hits.append(h)
            if len(hits) > cap: return hits[:cap]
        cur = t if t >= 0 else 0.0
    return hits
