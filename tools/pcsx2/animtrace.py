#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Traza en vivo (PINE, PCSX2 ya abierto con el juego en carrera) de las animaciones ACTIVAS del jugador.
Cada manejador k = 1..253 de `rider+0x0 + 16*k` = {propietario, pista*, _, u16 índice, u16}; está activo cuando `pista[0x28] != 0` (FUN_002099F0, PAL; func_00204230 en la
decompilación USA de Downhill Revived) y `pista+0x18` (f32) es su tiempo; `pista+0x29` es la bandera de bucle (FUN_00209588). El índice k es el id de clip de R/RANIM.NGA (FUN_001AC418).
Salida JSON por línea: t (s), act [[k, tiempo, bucle]], pedal (rider+0x7A78), eff = esfuerzo (+0x7A69), fl = banderas bajas (+0x4098), sp = |velocidad| (+0x4180).
Uso: animtrace.py --seconds 30 --out ~/dh-states/anim/x.jsonl [--rider 0]   (el emulador anidado: tools/pcsx2/nested.sh start; el estado se carga con --slot N)
Sin datos del juego en el repo: la salida va fuera."""
import argparse, json, math, os, struct, sys, time
sys.path.insert(0, os.path.dirname(__file__)); sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
import pine
from p2s import RIDER_BASE, RIDER_STRIDE, RIDER_COUNT_ADDR, PHYS_OFF

def main():
    ap = argparse.ArgumentParser(); ap.add_argument('--seconds', type=float, default=20); ap.add_argument('--out', required=True)
    ap.add_argument('--rider', type=int, default=0); ap.add_argument('--slot', type=int, default=0, help='cargar este savestate antes (0 = no cargar)')
    a = ap.parse_args()
    p = pine.Pine(pine.default_socket())
    if a.slot: p.load_state(a.slot); time.sleep(2.0)
    r = RIDER_BASE + a.rider * RIDER_STRIDE
    hs = p.read_windows([(r, 254 * 16)])[0]
    trk = [struct.unpack_from('<I', hs, 16 * k + 4)[0] for k in range(254)]
    ptrs = [t for t in trk if 0x100000 <= t < 0x2000000]
    if not ptrs: sys.exit('sin pistas: ¿el estado es una carrera?')
    lo, hi = min(ptrs), max(ptrs) + 0x40; hi = (hi + 7) & ~7; lo &= ~7
    extra = [(r + 0x7A78, 8), (r + 0x4098, 8), (r + 0x4180, 16)]
    out = open(a.out, 'w'); t0 = time.time(); n = 0; last = None
    while time.time() - t0 < a.seconds:
        pool, ex0, ex1, ex2 = p.read_windows([(lo, hi - lo)] + extra)
        key = (pool[:64], ex2[:12])
        if key == last: time.sleep(0.002); continue
        last = key
        act = []
        for k in range(1, 254):
            tp = trk[k]
            if not (lo <= tp < hi - 0x30): continue
            o = tp - lo
            if pool[o + 0x28] != 0: act.append([k, round(struct.unpack_from('<f', pool, o + 0x18)[0], 3), pool[o + 0x29]])
        v = struct.unpack_from('<3f', ex2, 0); sp = math.sqrt(sum(x * x for x in v))
        pedal = struct.unpack_from('<f', ex0, 0)[0]; flags = struct.unpack_from('<I', ex1, 0)[0]
        # rider+0x7A69 (esfuerzo) cae en la misma palabra que +0x7A68: se lee el byte
        eff = struct.unpack_from('<B', p.read_windows([(r + 0x7A68, 8)])[0], 1)[0]
        out.write(json.dumps(dict(t=round(time.time() - t0, 3), act=act, pedal=round(pedal, 2), eff=eff, fl=flags & 0xFF, sp=round(sp, 1))) + '\n'); n += 1
    out.close(); print(n, 'muestras ->', a.out)

if __name__ == '__main__': main()
