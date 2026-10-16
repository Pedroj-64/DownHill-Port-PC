#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Carga nativa de dhview de punta a punta: la captura (DH_SHOT, SDL offscreen, cámara fija) de `dhview NIVEL.NGP` debe ser IDÉNTICA píxel a píxel a la de `dhview out/maps/NIVEL.mdl` (export de Python) con la misma línea de carrera.
Necesita unpacked/, out/maps/ y build/dhview (datos locales, no va a CI). / End-to-end native load of dhview: the screenshot of `dhview LEVEL.NGP` must be PIXEL-IDENTICAL to `dhview out/maps/LEVEL.mdl` (Python export) with the same racing line. Needs local data, not for CI.
Uso / Usage:  python3 tools/native_view_check.py [--assert] [NIVEL ...]   (por defecto ALP2 ALPINE MOAB JUNGLE CITY)"""
import os, subprocess, sys, tempfile
R = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

def shot(arg, out, env_extra):
    env = dict(os.environ, SDL_VIDEODRIVER='offscreen', DH_SIZE='640 360', DH_SHOT=out, DH_CAM='0 3000 0 0.8 -0.9', **env_extra)
    subprocess.run([os.path.join(R, 'build/dhview'), arg], cwd=R, env=env, capture_output=True, timeout=600)
    return open(out, 'rb').read() if os.path.exists(out) else None

def main():
    names = [a for a in sys.argv[1:] if not a.startswith('--')] or ['ALP2', 'ALPINE', 'MOAB', 'JUNGLE', 'CITY']
    bad = 0
    with tempfile.TemporaryDirectory() as t:
        for n in names:
            chain = os.path.join(t, n + '.chain0.pts')
            subprocess.run([sys.executable, os.path.join(R, 'tools/pts_path.py'), os.path.join(R, 'unpacked/PTS', n + '.PTS'), chain, '--chain', '0'], capture_output=True, check=True)
            a = shot(os.path.join('out/maps', n + '.mdl'), os.path.join(t, 'py.bmp'), {'DH_PTS': chain})
            b = shot(os.path.join('unpacked/LVL', n + '.NGP'), os.path.join(t, 'nat.bmp'), {'DH_PTS': chain})
            ok = a is not None and a == b; bad += not ok; print(('PASA ' if ok else 'FALLA'), n)
    if '--assert' in sys.argv and bad: sys.exit(1)

if __name__ == '__main__':
    main()
