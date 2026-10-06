#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Exporta TODAS las texturas de una escena de interfaz (SHELL/UI*, LOADBAR/L*) como PNG a color (CLUT aplicado, volteo vertical corregido) para estudiar el estilo del original
(tipografía, marcos, barras). Uso: ui_textures_export.py unpacked/SHELL/UI outdir   (outdir fuera del repo: los PNG son material del juego)"""
import sys, os, io, contextlib, runpy
from PIL import Image
base, out = sys.argv[1], sys.argv[2]; os.makedirs(out, exist_ok=True)
sys.argv = ['extract_model.py', base, os.path.join(out, 'tmp.mdl')]
with contextlib.redirect_stdout(io.StringIO()): g = runpy.run_path(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'extract_model.py'), run_name='uiexport')
seen = set(); n = 0
for m in g['mats']:
    tid = m[1]
    if tid in seen or not g['texs'].get(tid): continue
    seen.add(tid); t = g['make_texture'](m[1], m[2], m[3], m[4], m[5])
    if t is None: continue
    w, h, rgba = t; Image.frombytes('RGBA', (w, h), rgba).transpose(Image.FLIP_TOP_BOTTOM).save(os.path.join(out, f'tex{tid:03d}_{w}x{h}.png')); n += 1
os.remove(os.path.join(out, 'tmp.mdl')) if os.path.exists(os.path.join(out, 'tmp.mdl')) else None
print(n, 'texturas ->', out)
