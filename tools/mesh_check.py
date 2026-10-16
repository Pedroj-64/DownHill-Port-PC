#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Paso 5a del loader nativo: los modelos sueltos (fuera de LVL: bici, piloto, etc.) deben salir BYTE A BYTE iguales entre build/mesh_check (src/mesh.hpp) y tools/extract_model.py. Necesita unpacked/ (datos locales, no va a CI).
/ Native-loader step 5a: standalone models (outside LVL: bike, rider...) must come out BYTE-IDENTICAL between build/mesh_check (src/mesh.hpp) and tools/extract_model.py. Needs unpacked/ (local data, not for CI).
Uso / Usage:  python3 tools/mesh_check.py [--assert] [BASE ...]   (BASE = ruta sin extensión bajo unpacked/; sin argumentos = todos los modelos con .NGP/.PTR/.TEX/.RTX fuera de LVL)"""
import glob, os, subprocess, sys, tempfile
R = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

def bases():
    out = []
    for f in sorted(glob.glob(os.path.join(R, 'unpacked/**/*.NGP'), recursive=True)):
        b = f[:-4]
        if '/LVL/' in b or not all(os.path.exists(b + e) for e in ('.PTR', '.TEX', '.RTX')): continue
        out.append(b)
    return out

def main():
    given = [os.path.join(R, a) if not os.path.isabs(a) else a for a in sys.argv[1:] if not a.startswith('--')]
    bad = done = 0
    with tempfile.TemporaryDirectory() as t:
        for b in given or bases():
            py, nat = os.path.join(t, 'py.mdl'), os.path.join(t, 'nat.mdl')
            r = subprocess.run([sys.executable, os.path.join(R, 'tools/extract_model.py'), b, py], capture_output=True, text=True)
            if r.returncode: print('PYTHON-ERROR', os.path.relpath(b, R), r.stderr.strip()[-120:]); bad += 1; continue
            subprocess.run([os.path.join(R, 'build/mesh_check'), b, nat], check=True)
            a, c = open(py, 'rb').read(), open(nat, 'rb').read(); done += 1
            if a != c: bad += 1; print('DISTINTO', os.path.relpath(b, R), len(a), len(c))
    print(f'{done} modelos comparados, {bad} distintos / models compared, differing')
    if '--assert' in sys.argv and (bad or not done): sys.exit(1)

if __name__ == '__main__':
    main()
