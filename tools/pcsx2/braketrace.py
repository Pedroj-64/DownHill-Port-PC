#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Traza en vivo (PINE) del frenado del jugador: velocidad, eje pedal/freno del módulo de física (ctrl+0x488), banderas ctrl+0x410/0x414, arrastre ctrl+0x490 y posición.
Módulo de conducción del piloto 0 = RIDER_BASE + 0x40C0 (+0x158 apunta al piloto, +0x150 = masa 100; hallado buscando punteros al piloto en un savestate). Salida CSV fuera del repo (por defecto ~/dh-captures/braketrace.csv).
Uso: python3 tools/pcsx2/braketrace.py [--seconds 30] [--hz 40] [--out archivo.csv]"""
import argparse, os, struct, sys, time, math
sys.path.insert(0, os.path.dirname(__file__)); sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
import pine
from p2s import RIDER_BASE, RIDER_COUNT_ADDR

def main():
    a = argparse.ArgumentParser(); a.add_argument('--seconds', type=float, default=30); a.add_argument('--hz', type=float, default=40)
    a.add_argument('--out', default=os.path.expanduser('~/dh-captures/braketrace.csv')); o = a.parse_args()
    os.makedirs(os.path.dirname(o.out), exist_ok=True); p = pine.Pine(pine.default_socket())
    PH = RIDER_BASE + 0x40C0; t0 = time.time(); n = 0   # módulo de conducción (FUN_00136330 param_1): +0x158 apunta al piloto, +0x150 = masa (100)
    with open(o.out, 'w') as f:
        f.write('t,speed,vx,vy,vz,axis488,f410,f414,drag490,mass,px,py,pz\n')
        while time.time() - t0 < o.seconds:
            t = time.time()
            if p.read32(RIDER_COUNT_ADDR) < 1: time.sleep(0.3); continue
            w = p.read_windows([(PH + 0xC0, 16), (PH + 0x150, 8), (PH + 0x410, 8), (PH + 0x488, 16), (RIDER_BASE + 0x7928, 8)])
            vx, vy, vz = struct.unpack('<3f', w[0][:12]); mass = struct.unpack('<f', w[1][:4])[0]; f410, f414 = struct.unpack('<2f', w[2])
            ax = struct.unpack('<f', w[3][:4])[0]; dr = struct.unpack('<f', w[3][8:12])[0]; node = struct.unpack('<I', w[4][4:8])[0]
            if not 0x100000 <= node < 0x2000000: continue
            px, py, pz = struct.unpack('<3f', p.read_windows([(node + 0x10, 16)])[0][:12])
            f.write(f'{t - t0:.3f},{math.sqrt(vx*vx+vy*vy+vz*vz):.2f},{vx:.2f},{vy:.2f},{vz:.2f},{ax:.4f},{f410:.4f},{f414:.4f},{dr:.6f},{mass:.1f},{px:.1f},{py:.1f},{pz:.1f}\n'); n += 1; f.flush()
            time.sleep(max(0, 1 / o.hz - (time.time() - t)))
    print(f'{n} muestras -> {o.out}')
if __name__ == '__main__': main()
