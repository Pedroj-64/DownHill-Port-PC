#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Exporta la imagen de cada pantalla de carga (unpacked/LOADBAR/L<NIVEL><IDIOMA>) como PNG, ya con el volteo vertical corregido
(la textura se guarda boca abajo respecto a como la usa el juego: PSMT8 lineal 512x512 + CLUT, ver extract_model.py).
Uso: loadbar_export.py unpacked/LOADBAR outdir   (outdir fuera del repo: los PNG son material del juego)"""
import sys, os, glob, io, contextlib, runpy
from PIL import Image
src, out = sys.argv[1], sys.argv[2]; os.makedirs(out, exist_ok=True); n = 0
for f in sorted(glob.glob(os.path.join(src, '*.NGP'))):
    base = f[:-4]
    sys.argv = ['extract_model.py', base, os.path.join(out, 'tmp.mdl')]
    with contextlib.redirect_stdout(io.StringIO()): g = runpy.run_path(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'extract_model.py'), run_name='loadbar')
    big = [m for m in g['mats'] if g['texs'].get(m[1]) and g['texs'][m[1]][2] == 19 and g['texs'][m[1]][:2] == (512, 512)]
    if not big: print('sin imagen:', base); continue
    t = g['make_texture'](big[0][1], big[0][2], big[0][3], big[0][4], big[0][5])
    if t is None: print('no decodificable:', base); continue
    w, h, rgba = t; Image.frombytes('RGBA', (w, h), rgba).transpose(Image.FLIP_TOP_BOTTOM).save(os.path.join(out, os.path.basename(base) + '.png')); n += 1
if os.path.exists(os.path.join(out, 'tmp.mdl')): os.remove(os.path.join(out, 'tmp.mdl'))
print(n, 'pantallas ->', out)
