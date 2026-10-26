#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""T7: el port C++ src/enginetick.hpp (bucle de colisión del motor, float64) debe coincidir con la referencia Python engine_tick_ref4 (< 1e-9 en estado e impulso) en ticks reales.
Genera los vectores con enginetick_export.py (fuera del repo) y ejecuta build/enginetick_check. Uso: enginetick_check.py [--assert] [--no-export]  (necesita grabaciones en ~/dh-states/awiiwa y out/maps/*.col)
/ The C++ port of the T7 collision tick must match the Python reference on recorded ticks."""
import os, subprocess, sys
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
LEVELS = (('ALPINE_', 'ALPINE'), ('ALPINEMX', 'ALPINEMX'), ('ALPINET', 'ALPINET'), ('MOAB', 'MOAB'))
def main(argv):
    bad = 0
    for lv, nm in LEVELS:
        b = os.path.expanduser(f'~/dh-states/h3/etick_{nm}.bin')
        if '--no-export' not in argv or not os.path.exists(b):
            subprocess.run([sys.executable, os.path.join(ROOT, 'tools/enginetick_export.py'), '--level', lv, '--stride', '7', '--max', '600'], check=True, capture_output=True)
        r = subprocess.run([os.path.join(ROOT, 'build/enginetick_check'), b, os.path.join(ROOT, f'out/maps/{nm}.col'), '--assert'], capture_output=True, text=True)
        print(nm, r.stdout.strip().splitlines()[-1] if r.stdout else r.stderr.strip()); bad += r.returncode != 0
    if '--assert' in argv and bad: raise SystemExit(f'FALLA: {bad} niveles')
if __name__ == '__main__': main(sys.argv[1:])
