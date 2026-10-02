#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Ensambla una bici a partir de piezas .mdl (DHM2) ya extraídas de BIKE/. Uso: assemble_bike.py out.mdl frame.mdl bars.mdl wheel.mdl [--rear Y] [--front Y] [--hub Z] [--head RAD] [--keep-axes]
Las piezas comparten el espacio de la bici (largo = eje Y, arriba = eje Z, 1 unidad ~ 0.28 m); las ruedas vienen centradas en el origen y se desplazan a los bujes
(por defecto Y = -1.60 trasero, +2.43 delantero; Z = -1.84; medidos sobre la textura del impostor de baja calidad del cuadro, que es una imagen lateral de la bici).
La pieza manillar+horquilla viene vertical: se inclina --head radianes (0.436 = 25 deg, igual que una constante del BANIM del taller; la horquilla del impostor da 25.7 deg) con la parte alta hacia atrás y su extremo inferior (eje de la rueda) se lleva al buje delantero."""
import struct, sys
args = [a for a in sys.argv[1:] if not a.startswith('--')]; opt = lambda k, d: float(sys.argv[sys.argv.index(k) + 1]) if k in sys.argv else d
out, frame, bars, wheel = args[:4]
rear, front, hub, head = opt('--rear', -1.60), opt('--front', 2.43), opt('--hub', -1.84), opt('--head', 0.436)
def load(p):
    b = open(p, 'rb').read(); assert b[:4] in (b'DHM2', b'DHM3'), p
    nt, nv, ni = struct.unpack_from('<III', b, 4); o = 16; T = []
    for _ in range(nt):
        w, h = struct.unpack_from('<II', b, o); T.append((w, h, b[o + 8:o + 8 + w * h * 4])); o += 8 + w * h * 4
    return T, [list(struct.unpack_from('<10f', b, o + 40 * i)) for i in range(nv)], list(struct.unpack_from(f'<{ni}I', b, o + 40 * nv))
T, V, I = [], [], []
import math
FORK_PIVOT = (0.13, -2.28)    # (y, z) del extremo inferior de la horquilla en la pieza manillar+horquilla (centro de los vértices con z < -1.9)
def add(path, dx=0, dy=0, dz=0, drop_lod=False, fork=False):
    t, v, i = load(path); base = len(V); tb = len(T); T.extend(t)
    if drop_lod:                                     # los cuadros traen un impostor de baja calidad (cuadrilátero plano con siluetas de ruedas que ocupa toda la caja): se descarta
        ymin, ymax = min(r[1] for r in v), max(r[1] for r in v); keep = []
        for k in range(0, len(i), 3):
            ys = [v[i[k + j]][1] for j in range(3)]; xs = [abs(v[i[k + j]][0]) for j in range(3)]
            if max(ys) - min(ys) > 0.9 * (ymax - ymin) and max(xs) < 0.05: continue
            keep += i[k:k + 3]
        i = keep
    for r in v:
        r = r[:]
        if fork:                                      # girar alrededor del pivote (parte alta hacia -Y) y llevar el pivote al buje delantero
            ay, az = r[1] - FORK_PIVOT[0], r[2] - FORK_PIVOT[1]; c, sn = math.cos(head), math.sin(head)
            r[1], r[2] = front + ay * c - az * sn, hub + ay * sn + az * c
        else: r[0] += dx; r[1] += dy; r[2] += dz
        if r[9] >= 0: r[9] += tb
        V.append(r)
    I.extend(base + k for k in i)
add(frame, drop_lod=True); add(bars, fork=True); add(wheel, 0, rear, hub); add(wheel, 0, front, hub)
if '--keep-axes' not in sys.argv:    # espacio de la bici (largo Y, arriba Z) -> espacio del visor (arriba Y, delante -Z): (x, y, z) -> (x, z, -y)
    for r in V: r[0], r[1], r[2] = r[0], r[2], -r[1]
with open(out, 'wb') as f:
    f.write(b'DHM2' + struct.pack('<III', len(T), len(V), len(I)))
    for w, h, px in T: f.write(struct.pack('<II', w, h) + px)
    for r in V: f.write(struct.pack('<10f', *r))
    f.write(struct.pack(f'<{len(I)}I', *I))
print(f'{len(T)} texturas, {len(V)} vértices, {len(I)//3} triángulos -> {out}')
