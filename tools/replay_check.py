#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Oráculo por repetición del port en suelo (hito M1): exporta las telemetrías DHTEL1, corre build/replay_demo y comprueba umbrales. Necesita datos del juego (fuera del repo), no va a CI.
/ Port replay oracle on the ground (milestone M1): exports the DHTEL1 telemetries, runs build/replay_demo and checks thresholds. Needs game data (outside the repo), not for CI.
Uso / Usage:  python3 tools/replay_check.py [--assert] [carpeta_con_grabaciones=~/dh-states/awiiwa]
Umbrales (propuestos, no fijados) sobre las ventanas de 0.5 s que empiezan en suelo: error de posición mediano <= 3.5 u y de |v| mediano <= 8 u/s en CADA nivel.
/ Proposed (not fixed) thresholds over 0.5 s windows that start on the ground: median position error <= 3.5 u and median |v| error <= 8 u/s on EACH level."""
import glob, os, re, subprocess, sys
R = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LEVELS = ['ALPINE', 'ALPINEMX', 'ALPINET', 'MOAB']
POS_MAX, VEL_MAX = 3.5, 8.0

def main():
    a = [x for x in sys.argv[1:] if not x.startswith('--')]; check = '--assert' in sys.argv
    env = dict(os.environ, DH_REAL='1', DH_ROLLTAB='1') if '--real' in sys.argv else None   # --real: fuerzas de suelo del motor sin términos empíricos (T6: peso, arrastre g/vterm² con rampa de esfuerzo, rodadura por clase, pedal constante, impulso de arranque)
    base = os.path.expanduser(a[0] if a else '~/dh-states/awiiwa'); out = os.path.expanduser('~/dh-states/replay'); os.makedirs(out, exist_ok=True); ok = True
    for n in LEVELS:
        dirs = sorted(glob.glob(os.path.join(base, f'dhtel_*_{n}_*')))
        dirs = [d for d in dirs if os.path.isdir(d)]
        if not dirs: print(f'{n}: sin grabación en {base}'); ok = False; continue
        rp = os.path.join(out, f'{n}.replay'); subprocess.run([sys.executable, os.path.join(R, 'tools/telemetry/export_replay.py'), dirs[-1], '-o', rp], check=True, capture_output=True)
        col = os.path.join(R, 'out/maps', f'{n}.col')
        o = subprocess.run([os.path.join(R, 'build/replay_demo'), col, rp, '2', '0.5'], capture_output=True, text=True, env=env).stdout
        m = re.search(r'G05 n=(\d+) e05_med=([\d.]+) e05_p90=([\d.]+) v05_med=([\d.]+)', o)
        if not m: print(f'{n}: sin salida de replay_demo'); ok = False; continue
        e, v = float(m.group(2)), float(m.group(4)); good = e <= POS_MAX and v <= VEL_MAX; ok &= good
        print(f"{'PASA ' if good else 'FALLA'} {n}: {m.group(1)} ventanas, error de posición mediano {e:.2f} u (<= {POS_MAX}), de |v| {v:.2f} u/s (<= {VEL_MAX})")
    if check and not ok: sys.exit(1)

if __name__ == '__main__':
    main()
