#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Compara el render de dhview con la captura del propio juego guardada en un savestate (misma cámara).
Uso: compare_view.py estado.p2s modelo.mdl salida.png [--fov RAD]   (dhview debe estar compilado: build/dhview)"""
import os, subprocess, sys, zipfile, io
sys.path.insert(0, os.path.dirname(__file__))
from p2s_camera import dh_cam
from p2s import State
from PIL import Image

def main():
    st_path, mdl, out = sys.argv[1:4]; st = State(st_path); (x, y, z), yaw, pitch = dh_cam(st)
    shot = Image.open(io.BytesIO(zipfile.ZipFile(st_path).read('Screenshot.png'))).convert('RGB').resize((640, 480))
    tmp = out + '.bmp'; env = dict(os.environ, DH_SIZE='640 480', DH_CAM=f'{x} {y} {z} {yaw} {pitch}', DH_SHOT=tmp, DH_DT='0.016667', DH_NOCOLDRAW='1')
    if '--fov' in sys.argv: env['DH_FOV'] = sys.argv[sys.argv.index('--fov') + 1]
    subprocess.run([os.path.join(os.path.dirname(__file__), '..', 'build/dhview'), mdl], env=env, timeout=300, capture_output=True)
    mine = Image.open(tmp).convert('RGB'); w, h = mine.size; mine = mine.resize((640, int(640 * h / w)))
    c = Image.new('RGB', (1280, max(480, mine.size[1]))); c.paste(shot, (0, 0)); c.paste(mine, (640, 0)); c.save(out); os.remove(tmp)

if __name__ == '__main__': main()
