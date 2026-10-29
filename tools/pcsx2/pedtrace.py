#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Traza en vivo (PINE, sin pausar el juego) del estado de los peatones durante una carrera: una fila CSV por peatón vivo y muestra.
Columnas: t, jugador xyz, ranura, grupo, tipo (.PED u16 +20), state+0/+4/+8, bandera +0x28, peatón xyz, distancia al piloto más cercano y cuál es. Salida FUERA del repo (por defecto ~/dh-captures/pedtrace.csv).
Uso: python3 tools/pcsx2/pedtrace.py [--seconds 90] [--hz 20] [--out archivo.csv]    (direcciones de docs/formats/gs-dump.md)"""
import argparse, math, os, struct, sys, time
sys.path.insert(0, os.path.dirname(__file__)); sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
import pine
from p2s import RIDER_BASE, RIDER_COUNT_ADDR, RIDER_STRIDE

M = 0x332F40; POOL = M + 0x5DD0; STRIDE = 0x230; STACK = M + 0x21350; USED = M + 0x21418

def main():
    a = argparse.ArgumentParser(); a.add_argument('--seconds', type=float, default=90); a.add_argument('--hz', type=float, default=20)
    a.add_argument('--out', default=os.path.expanduser('~/dh-captures/pedtrace.csv')); o = a.parse_args()
    os.makedirs(os.path.dirname(o.out), exist_ok=True); p = pine.Pine(pine.default_socket())
    if p.status() != 0: sys.exit('el juego no está corriendo')
    t0 = time.time(); n = 0
    with open(o.out, 'w') as f:
        f.write('t,px,py,pz,slot,group,type,w0,w1,s8,flag,x,y,z,dmin,rmin\n')
        while time.time() - t0 < o.seconds:
            t = time.time()
            if p.read32(RIDER_COUNT_ADDR) < 1: time.sleep(0.5); continue   # fuera de carrera
            nr = min(p.read32(RIDER_COUNT_ADDR), 8)
            *nodes, used, stack = p.read_windows([(RIDER_BASE + i * RIDER_STRIDE + 0x7928, 8) for i in range(nr)] + [(USED, 8), (STACK, 200)])
            nodes = [struct.unpack('<I', x[4:8])[0] for x in nodes]; node = nodes[0]; used = min(struct.unpack('<I', used[:4])[0], 200)
            if not 0x100000 <= node < 0x2000000: continue
            slots = list(stack[:used]); ws = [(n + 0x10, 16) for n in nodes if 0x100000 <= n < 0x2000000] + [(POOL + s * STRIDE, 0x30) for s in slots] + [(POOL + s * STRIDE + 0x158, 16) for s in slots]
            d = p.read_windows(ws); nn = len(nodes); rp = [struct.unpack('<3f', x[:12]) for x in d[:nn]]; pl = rp[0]; hd = d[nn:nn + used]; tl = d[nn + used:]
            pps = [struct.unpack('<I', tl[i][8:12])[0] for i in range(used)]   # +0x160 = punto, +0x15C = grupo (ventana desde +0x158)
            gps =[struct.unpack('<I', tl[i][4:8])[0] for i in range(used)]
            pts = p.read_windows([(pp, 24) for pp in pps]) if used else []
            for i, s in enumerate(slots):
                w0, w1, s8 = struct.unpack('<3I', hd[i][:12]); flag = struct.unpack('<I', hd[i][0x28:0x2C])[0]
                typ = struct.unpack('<H', pts[i][20:22])[0]
                xyz = struct.unpack('<3f', pts[i][:12]); g = (gps[i] - (M + 0x10)) // 0xF0
                dr = [math.dist(xyz, r) for r in rp]; dm = min(dr)
                f.write(f'{t - t0:.3f},{pl[0]:.1f},{pl[1]:.1f},{pl[2]:.1f},{s},{g},{typ:#x},{w0},{w1},{s8},{flag},{xyz[0]:.1f},{xyz[1]:.1f},{xyz[2]:.1f},{dm:.1f},{dr.index(dm)}\n')
            n += 1; f.flush(); time.sleep(max(0, 1 / o.hz - (time.time() - t)))
    print(f'{n} muestras -> {o.out}')

if __name__ == '__main__': main()
