#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Hoja de contacto de .mdl: renderiza cada uno con dhview (auto-encuadre) y los junta en una imagen con su nombre.
Uso: contact_sheet.py carpeta_con_mdl salida.png [--cols N] [--size PX] [--from I --count K] [--orbit "yaw pitch"]  (requiere dhview compilado en build/ y Pillow)"""
import sys, os, glob, subprocess, tempfile
from PIL import Image, ImageDraw
d, out = sys.argv[1], sys.argv[2]
arg = lambda k, dflt: int(sys.argv[sys.argv.index(k) + 1]) if k in sys.argv else dflt
cols, size, first, count = arg('--cols', 6), arg('--size', 220), arg('--from', 0), arg('--count', 10**9)
orbit = sys.argv[sys.argv.index('--orbit') + 1] if '--orbit' in sys.argv else None
files = sorted(f for f in glob.glob(f'{d}/*.mdl') if not f.endswith('.sky.mdl'))[first:first + count]
viewer = os.path.join(os.path.dirname(__file__), '..', 'build', 'dhview')
rows = (len(files) + cols - 1) // cols; sheet = Image.new('RGB', (cols * size, rows * (size + 14)), (20, 20, 28)); dr = ImageDraw.Draw(sheet)
for n, f in enumerate(files):
    with tempfile.TemporaryDirectory() as t:
        bmp = os.path.join(t, 'a.bmp')
        subprocess.run([viewer, f], env={**os.environ, 'DH_SHOT': bmp, **({'DH_ORBIT': orbit} if orbit else {})}, capture_output=True, timeout=120)
        im = Image.open(bmp).convert('RGB') if os.path.exists(bmp) else Image.new('RGB', (size, size), (90, 0, 0))
    w, h = im.size; s = min(w, h); im = im.crop(((w - s) // 2, (h - s) // 2, (w + s) // 2, (h + s) // 2)).resize((size, size))
    x, y = (n % cols) * size, (n // cols) * (size + 14); sheet.paste(im, (x, y + 14)); dr.text((x + 2, y + 1), os.path.basename(f)[:-4], fill=(220, 220, 220))
sheet.save(out); print(f'{len(files)} modelos -> {out}')
