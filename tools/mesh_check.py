#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Paso 5a del loader nativo: los modelos sueltos (fuera de LVL: bici, piloto, etc.) deben salir BYTE A BYTE iguales entre build/mesh_check (src/mesh.hpp) y tools/extract_model.py. Necesita unpacked/ (datos locales, no va a CI).
/ Native-loader step 5a: standalone models (outside LVL: bike, rider...) must come out BYTE-IDENTICAL between build/mesh_check (src/mesh.hpp) and tools/extract_model.py. Needs unpacked/ (local data, not for CI).
Paso 5b: con --levels se comparan los NIVELES (.mdl DHM3 + .sky.mdl + .dome.mdl) con los que dejó tools/export_all.sh en out/maps/ (existencia incluida).
/ Step 5b: with --levels the LEVELS (.mdl DHM3 + .sky.mdl + .dome.mdl) are compared with those tools/export_all.sh left in out/maps/ (existence included).
Uso / Usage:  python3 tools/mesh_check.py [--assert] [--levels] [BASE|NIVEL ...]   (BASE = ruta sin extensión bajo unpacked/; sin argumentos = todos los modelos sueltos con .NGP/.PTR/.TEX/.RTX fuera de LVL; con --levels, NIVEL = nombre)"""
import glob, os, subprocess, sys, tempfile
R = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

def bases():
    out = []
    for f in sorted(glob.glob(os.path.join(R, 'unpacked/**/*.NGP'), recursive=True)):
        b = f[:-4]
        if '/LVL/' in b or not all(os.path.exists(b + e) for e in ('.PTR', '.TEX', '.RTX')): continue
        out.append(b)
    return out

def levels(names):
    bad = done = 0
    with tempfile.TemporaryDirectory() as t:
        for n in names or [os.path.basename(f)[:-4] for f in sorted(glob.glob(os.path.join(R, 'unpacked/LVL/*.NGP')))]:
            if not os.path.exists(os.path.join(R, 'out/maps', n + '.mdl')): continue
            out = os.path.join(t, n + '.mdl'); subprocess.run([os.path.join(R, 'build/mesh_check'), os.path.join(R, 'unpacked/LVL', n), out, '--level'], check=True); done += 1
            for suf in ('.mdl', '.sky.mdl', '.dome.mdl'):
                ref, nat = os.path.join(R, 'out/maps', n + suf), os.path.join(t, n + suf)
                if os.path.exists(ref) != os.path.exists(nat): bad += 1; print('EXISTENCIA', n + suf, 'python', os.path.exists(ref), 'nativo', os.path.exists(nat)); continue
                if os.path.exists(ref) and open(ref, 'rb').read() != open(nat, 'rb').read(): bad += 1; print('DISTINTO', n + suf)
                if os.path.exists(nat): os.remove(nat)
    print(f'{done} niveles comparados, {bad} archivos distintos / levels compared, differing files')
    return bad, done

def main():
    if '--levels' in sys.argv:
        bad, done = levels([a for a in sys.argv[1:] if not a.startswith('--')])
        if '--assert' in sys.argv and (bad or not done): sys.exit(1)
        return
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
