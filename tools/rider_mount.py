#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Anclajes de la bici (agarres, pedales) y ajuste rígido piloto -> bici. Evidencia y hipótesis: docs/formats/rider-mount.md.
Uso: rider_mount.py [BIKE/CARTMX.NGP BIKE/BIKESKEL.NGP]   -> tabla de anclajes y articulaciones (sin argumentos: autocomprobación sintética).
Espacio de la bici = el de assemble_bike.py / piezas de BIKE/ (largo Y adelante, arriba Z, X a la derecha, 1 u ~ 0.28-0.30 m)."""
import struct, sys, os, math
sys.path.insert(0, os.path.dirname(__file__))
import numpy as np
from scene import Scene, local, mul, IDENT
KINDS = {2081: 'grip_R', 2084: 'grip_L', 2087: 'pedal_R', 2090: 'pedal_L'}   # nodos tipo 40 (attach) del cuadro; R/L por el signo de X (X = derecha): nombres = hipótesis, posiciones medidas

def anchors(d):
    """Nodos tipo 40 de un .NGP de bici -> {nombre: (posición, filas de rotación 3x3)} en el espacio del modelo. Recorre también los tipo 25 (+0x2c); matrices tipo 3/4 compuestas fila-vector (hijo * padre)."""
    S = Scene(d); out = {}; seen = set()
    def rec(o, m):
        if o is None or o in seen: return
        seen.add(o); h = S.u32(o); t = h & 0x3f
        if t == 40 and (h >> 18) in KINDS: out[KINDS[h >> 18]] = (np.array(m[12:15]), np.array(m).reshape(4, 4)[:3, :3])
        l = local(S, o); m2 = mul(l, m) if l else m
        for c in ([S.ptr(o + 0x2c)] if t == 25 else S.children(o)): rec(c, m2)
    for r in [S.ptr(4 + 4*i) for i in range(S.u32(0))]:
        if S.u32(r) & 0x3f != 7: rec(r, IDENT)
    return out

def bike_joints(d):
    """Tablas de articulación (tipo 0x11) de BIKESKEL.NGP -> {índice: dict(chan, loc, eul0)}. Canales 0-5 raíz, 6-8 horquilla, 9 rueda, 10-16 biela+pedal D, 17-23 biela+pedal I, 24-26 manillar."""
    u = lambda o: struct.unpack_from('<I', d, o)[0]; J = {}
    for t in range(0, len(d) - 0xe0, 16):
        if u(t) & 0x3f == 0x11 and u(t + 0xc4) < 16 and u(t + 0xc0) & 0x30300:
            ch = [c for c in struct.unpack_from('<8H', d, t + 0x10)[1:7] if c != 0xffff]
            J[u(t + 0xc4)] = dict(chan=ch, loc=struct.unpack_from('<3f', d, t + 0x30), eul0=struct.unpack_from('<3f', d, t + 0x20))
    return J

def live_pedals(pose):
    """Posición viva de los pedales en el espacio de la bici: canales 14-16 (D) y 21-23 (I) del array de pose = traslación del 'asa' de pedal (coincide con las matrices del esqueleto en RAM)."""
    return np.array([pose[14], pose[15], pose[16]]), np.array([pose[21], pose[22], pose[23]])

def kabsch(P, Q, yaw_only=False):
    """Ajuste rígido P -> Q: v' = R v + t (columna). yaw_only = sólo giro alrededor de Z. Devuelve (R, t, residuos por punto)."""
    P, Q = np.asarray(P, float), np.asarray(Q, float); pc, qc = P.mean(0), Q.mean(0)
    if yaw_only:
        a, b = P[:, :2] - pc[:2], Q[:, :2] - qc[:2]; th = math.atan2((a[:, 0]*b[:, 1] - a[:, 1]*b[:, 0]).sum(), (a*b).sum())
        R = np.array([[math.cos(th), -math.sin(th), 0], [math.sin(th), math.cos(th), 0], [0, 0, 1]])
    else:
        U, _, Vt = np.linalg.svd((P - pc).T @ (Q - qc)); R = Vt.T @ np.diag([1, 1, np.sign(np.linalg.det(Vt.T @ U.T))]) @ U.T
    t = qc - R @ pc
    return R, t, np.linalg.norm((P @ R.T + t) - Q, axis=1)

def selftest():
    P = np.random.default_rng(1).normal(size=(4, 3)); th = 0.7
    R0 = np.array([[math.cos(th), -math.sin(th), 0], [math.sin(th), math.cos(th), 0], [0, 0, 1]]); t0 = np.array([1., 2., 3.])
    for yaw in (True, False):
        R, t, res = kabsch(P, P @ R0.T + t0, yaw); assert res.max() < 1e-9 and np.allclose(t, t0), (yaw, res)
    print('rider_mount selftest OK')

if __name__ == '__main__':
    if len(sys.argv) < 3: selftest(); sys.exit()
    for k, (p, R) in anchors(open(sys.argv[1], 'rb').read()).items(): print(f'{k:8s} pos {np.round(p, 3)}  rot rows {np.round(R, 3).tolist()}')
    for i, j in sorted(bike_joints(open(sys.argv[2], 'rb').read()).items()): print('BIKESKEL joint', i, j['chan'], np.round(j['loc'], 3), np.round(j['eul0'], 3))
