#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Extrae todos los grupos NGP/PTR/TEX/RTX completos de una carpeta descomprimida a .mdl. Uso: batch_models.py carpeta outdir"""
import sys, os, subprocess, glob
src, out = sys.argv[1], sys.argv[2]; os.makedirs(out, exist_ok=True)
bases = sorted(p[:-4] for p in glob.glob(f'{src}/*.NGP') if all(os.path.exists(p[:-4] + e) for e in ('.PTR', '.TEX', '.RTX')))
ok = bad = 0
for b in bases:
    name = os.path.basename(b)
    r = subprocess.run([sys.executable, os.path.join(os.path.dirname(__file__), 'extract_model.py'), b, f'{out}/{name}.mdl'], capture_output=True, text=True)
    if r.returncode: bad += 1; print(f'FALLO {name}: {r.stderr.strip().splitlines()[-1] if r.stderr.strip() else "?"}')
    else: ok += 1; print(f'{name:10} {r.stdout.strip()}')
print(f'{ok} modelos, {bad} fallos')
