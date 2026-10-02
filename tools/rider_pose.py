#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Posa un modelo de piloto (docs/formats/rider.md) con un clip .NGA: piel de 3 huesos por vértice (VU1) + esqueleto de nodos tipo 35/17 (rider_skel.py).
Uso: rider_pose.py modelo.mdl modelo.skin nivel.NGP raíz_hex anim.NGA clip_id tiempo salida.mdl
Convenciones (HIPÓTESIS salvo lo citado): vector fila; local = Euler(-e) [* rotación de referencia si flags & 0x20000] | loc (FUN_0020effc caso 0x11 @0x20f4e8, FUN_00227da8 construye la matriz, FUN_00227698 = A*B);
mundo = local * mundo_padre; piel = inv_bind * mundo; v' = suma w_i * v * piel[hueso_i]. Canales de Euler de cada hueso = chan[1..3] de su tabla; canales 3-5 de la raíz sin usar aquí."""
import sys, os, struct, math
sys.path.insert(0, os.path.dirname(__file__))
import numpy as np
from rider_skel import load, preorder
import nga

def euler_matrix(a, b, c):
    """FUN_00227da8 (decompilado): filas en memoria = Rx(a)Ry(b)Rz(c) en convención columna, usada con vectores fila."""
    sa, ca, sb, cb, sc, cc = math.sin(a), math.cos(a), math.sin(b), math.cos(b), math.sin(c), math.cos(c)
    return np.array([[cb*cc, -cb*sc, sb, 0],
                     [sa*sb*cc + ca*sc, -sa*sb*sc + ca*cc, -sa*cb, 0],
                     [-ca*sb*cc + sa*sc, ca*sb*sc + sa*cc, ca*cb, 0],
                     [0, 0, 0, 1.0]])
def translate(t): m = np.eye(4); m[3, :3] = t; return m

def skin_matrices(J, order, pose):
    """-> matrices de piel [15] en orden de paleta. pose[canal] = ángulo (rad)."""
    parent = {}
    for n in order:
        for c in J[n].children_nodes: parent[c] = n
    World = {}
    for n in order:                                        # preorden: el padre ya está calculado
        j = J[n]
        e = [float(pose.get(c, 0.0)) if c is not None else 0.0 for c in j.chan[1:4]]
        L = euler_matrix(-e[0], -e[1], -e[2])
        if j.flags & 0x20000:                              # marco espejado (cadena izquierda): L = Euler * rotación de referencia (tabla + 0x80)
            R = np.eye(4); R[:3, :3] = np.array(j.bind_rot); L = L @ R
        L[3, :3] = j.loc                                   # fila de traslación = loc (el motor suma loc a una fila 3 nula)
        World[n] = L if n not in parent else L @ World[parent[n]]
    return [np.array(J[n].inv_bind).reshape(4, 4) @ World[n] for n in order]

def read_mdl(path):
    d = open(path, 'rb').read(); magic = d[:4]; nt, nv, ni = struct.unpack_from('<III', d, 4); o = 16; tex = []
    for _ in range(nt):
        w, h = struct.unpack_from('<II', d, o); tex.append(d[o:o + 8 + w*h*4]); o += 8 + w*h*4
    V = np.frombuffer(d, '<f4', nv * 10, o).reshape(nv, 10).copy(); o += 40 * nv
    return magic, tex, V, d[o:], ni   # índices + tabla de chunks DHM3 (si la hay) se conservan tal cual
def write_mdl(path, magic, tex, V, idx_bytes, ni):
    with open(path, 'wb') as f:
        f.write(magic + struct.pack('<III', len(tex), len(V), ni)); [f.write(t) for t in tex]; f.write(V.astype('<f4').tobytes()); f.write(idx_bytes)

def pose_from_clip(nga_path, clip_id, t):
    d = open(nga_path, 'rb').read(); _, clips = nga.parse_nga(d)
    c = next(c for c in clips if c['id'] == clip_id)
    return {tr['channel']: nga.sample_track(tr, t) for tr, _ in nga.clip_tracks(d, c)}

def apply(mdl, skin, ngp, root, pose, out):
    d = open(ngp, 'rb').read(); J = load(d, root); order = preorder(J, root)
    S = skin_matrices(J, order, pose)
    magic, tex, V, idx, ni = read_mdl(mdl); sk = open(skin, 'rb').read()
    assert sk[:4] == b'DHSK' and struct.unpack_from('<I', sk, 4)[0] == len(V)
    rec = np.frombuffer(sk, np.dtype([('b', 'u1', 4), ('w', '<f4', 3)]), len(V), 8)
    P = np.hstack([V[:, :3], np.ones((len(V), 1), np.float32)]).astype(np.float64); out_p = np.zeros((len(V), 3))
    Sm = np.array(S)                                        # (15, 4, 4)
    for k in range(3):
        b = rec['b'][:, k].astype(int); w = np.clip(rec['w'][:, k], 0, 1).astype(np.float64)
        out_p += w[:, None] * np.einsum('ni,nij->nj', P, Sm[b])[:, :3]
    V[:, :3] = out_p; write_mdl(out, magic, tex, V, idx, ni)

if __name__ == '__main__':
    mdl, skin, ngp, root, anim, cid, t, out = sys.argv[1:9]
    apply(mdl, skin, ngp, int(root, 0), pose_from_clip(anim, int(cid), float(t)), out)
