#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Cámara del juego en un savestate (EE RAM 0x3A8D00, SLES-52202): filas 0x3A8D10/20/30 = derecha / adelante / arriba (Z arriba, mano derecha), posición f32x3 en 0x3A8D50,
desplazamiento local respecto al jugador (x,y,z) en 0x3A8D70 (posición = jugador + x·fila0 + y·fila1 + z·fila2; verificado en 2 estados: error < 0.1 u).
Uso: p2s_camera.py estado.p2s   -> imprime DH_CAM para dhview (espacio Y arriba: (x,y,z)->(x,z,-y)).  La dirección de RAM es de este ELF (hipótesis: igual en todos los niveles)."""
import math, sys, os
sys.path.insert(0, os.path.dirname(__file__))
from p2s import State

def camera(st):
    rows = [st.f32(0x3A8D10 + 16 * k, 3) for k in range(3)]; pos = st.f32(0x3A8D50, 3); off = st.f32(0x3A8D70, 3)
    return rows, pos, off

def dh_cam(st):
    rows, pos, _ = camera(st); f = rows[1]; fy = (f[0], f[2], -f[1])                      # adelante en Y arriba
    pitch = math.asin(max(-1, min(1, fy[1]))); c = math.cos(pitch); yaw = math.atan2(fy[0] / c, -fy[2] / c) if c > 1e-6 else 0.0
    return (pos[0], pos[2], -pos[1]), yaw, pitch

if __name__ == '__main__':
    st = State(sys.argv[1]); (x, y, z), yaw, pitch = dh_cam(st); print(f'DH_CAM="{x:.2f} {y:.2f} {z:.2f} {yaw:.5f} {pitch:.5f}"')
