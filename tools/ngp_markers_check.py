#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Paso 3 del loader nativo: build/ngp_markers_check debe dar BYTE A BYTE los .gates, los puntos de rejilla y la cadena 0 del .PTS de tools/markers.py y tools/pts_path.py. Necesita unpacked/ (datos locales, no va a CI).
/ Native-loader step 3: build/ngp_markers_check must give BYTE-IDENTICAL .gates, start-grid points and PTS chain 0 versus tools/markers.py and tools/pts_path.py. Needs unpacked/ (local data, not for CI).
Uso / Usage:  python3 tools/ngp_markers_check.py [--assert] [NIVEL ...]"""
import glob, os, subprocess, sys, tempfile
R = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

def main():
    names = [a for a in sys.argv[1:] if not a.startswith('--')] or [os.path.basename(f)[:-4] for f in sorted(glob.glob(os.path.join(R, 'unpacked/LVL/*.NGP')))]
    bad, done = 0, 0
    with tempfile.TemporaryDirectory() as t:
        for n in names:
            ngp, pts = os.path.join(R, 'unpacked/LVL', n + '.NGP'), os.path.join(R, 'unpacked/PTS', n + '.PTS')
            if not os.path.exists(pts): continue
            done += 1; py = {k: os.path.join(t, 'py_' + k) for k in ('gates', 'start', 'chain0')}; nat = os.path.join(t, 'nat'); os.makedirs(nat, exist_ok=True)
            subprocess.run([sys.executable, os.path.join(R, 'tools/markers.py'), ngp, 'x', py['start'], py['gates']], capture_output=True, check=True)
            subprocess.run([sys.executable, os.path.join(R, 'tools/pts_path.py'), pts, py['chain0'], '--chain', '0'], capture_output=True, check=True)
            subprocess.run([os.path.join(R, 'build/ngp_markers_check'), ngp, pts, nat], check=True)
            for k, f in (('gates', 'gates.bin'), ('start', 'start.bin'), ('chain0', 'chain0.bin')):
                a = open(py[k], 'rb').read() if os.path.exists(py[k]) else b''; b = open(os.path.join(nat, f), 'rb').read()
                if a != b: bad += 1; print('DISTINTO', n, k, len(a), len(b))
    print(f'{done} niveles comparados, {bad} archivos distintos / compared, differing files')
    if '--assert' in sys.argv and (bad or not done): sys.exit(1)

if __name__ == '__main__':
    main()
