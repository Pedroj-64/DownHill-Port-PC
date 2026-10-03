# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Caída libre fabricada para el oráculo H3 (PCSX2 ya abierto con PINE). Carga un savestate, espera a que el cuerpo rígido del piloto RIDER pase a integrarse
(módulo + 8 = nodo; el motor sólo lo integra en ciertos tramos) y, en ese instante, SUBE su posición `--lift` u en +z (eje vertical, docs/DECISIONS.md) escribiendo SÓLO
+0x50 del cuerpo. Después no se toca nada: el juego integra solo (FUN_00238818 recalcula el nodo = pos - com·R) y se captura con sincronía por el cuerpo de ese piloto.
Cuenta como par válido únicamente lo capturado tras la escritura, con F_eff ≈ (0,0,-m·g) (comprobar con integrator_check.py)."""
import argparse, os, struct, sys, time
sys.path.insert(0, os.path.dirname(__file__))
import live, pine_capture

def main():
    ap = argparse.ArgumentParser(); ap.add_argument('--slot', type=int, default=1); ap.add_argument('--rider', type=int, default=1); ap.add_argument('--lift', type=float, default=1000.0)
    ap.add_argument('--seconds', type=float, default=2.0); ap.add_argument('--out', required=True); ap.add_argument('--timeout', type=float, default=40.0); a = ap.parse_args()
    p = live.connect(); p.load_state(a.slot); time.sleep(1.0); base = live.rider(a.rider); link = base + 0x6428; pos = base + 0x6420 + 0x10 + 0x50; t0 = time.time()
    while p.read32(link) == 0:                                            # módulo ligado a su nodo = cuerpo en uso
        if time.time() - t0 > a.timeout: sys.exit('el cuerpo del piloto %d no se activó' % a.rider)
    time.sleep(0.005)                                                     # el cuerpo se siembra tras ligarse el nodo: esperar a ver su primer paso
    z = struct.unpack('<f', struct.pack('<I', p.read32(pos + 8)))[0]; live.wr_f32(p, pos + 8, z + a.lift)
    time.sleep(0.05); z2 = struct.unpack('<f', struct.pack('<I', p.read32(pos + 8)))[0]
    print(f'z {z:.1f} -> {z + a.lift:.1f}; releído {z2:.1f} (activo tras {time.time() - t0:.2f} s)')
    if z2 < z + a.lift / 2: sys.exit('la escritura no se mantuvo (el cuerpo se resembró)')
    pine_capture.record(p, a.out, max(a.rider + 1, 6), a.seconds, a.rider)

if __name__ == '__main__': main()
