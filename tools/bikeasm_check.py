#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Ensamblado nativo de la bici (src/bikeasm.hpp): build/bikeasm_check debe dar BYTE A BYTE el .mdl de extract_model.py (3 piezas) + assemble_bike.py regenerados ahora. Necesita unpacked/BIKE (datos locales, no va a CI).
/ Native bike assembly: build/bikeasm_check must give BYTE-IDENTICAL output to extract_model.py (3 parts) + assemble_bike.py regenerated now. Needs unpacked/BIKE (local data, not for CI).
Uso / Usage:  python3 tools/bikeasm_check.py [--assert] [CUADRO MANILLAR RUEDA ...]   (por defecto las de tools/dh.py: CANFIELD MAN100 WHBRIAN; además otras combinaciones)"""
import itertools, os, subprocess, sys, tempfile
R = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
COMBOS = [('CANFIELD', 'MAN100', 'WHBRIAN'), ('CANFIELD', 'MAN100', 'WHBRIAN')]

def main():
    given = [a for a in sys.argv[1:] if not a.startswith('--')]
    combos = [tuple(given[:3])] if len(given) >= 3 else sorted(set(COMBOS)) + [c for c in [('BOARBIKE', 'MAN100', 'WHBRIAN')] if os.path.exists(os.path.join(R, 'unpacked/BIKE', c[0] + '.NGP'))]
    bad = 0
    with tempfile.TemporaryDirectory() as t:
        for c in combos:
            parts = []
            for p in c:
                out = os.path.join(t, p + '.mdl'); subprocess.run([sys.executable, os.path.join(R, 'tools/extract_model.py'), os.path.join(R, 'unpacked/BIKE', p), out], capture_output=True, check=True); parts.append(out)
            py, nat = os.path.join(t, 'py.mdl'), os.path.join(t, 'nat.mdl')
            subprocess.run([sys.executable, os.path.join(R, 'tools/assemble_bike.py'), py, *parts], capture_output=True, check=True)
            subprocess.run([os.path.join(R, 'build/bikeasm_check'), os.path.join(R, 'unpacked/BIKE'), nat, *c], check=True)
            ok = open(py, 'rb').read() == open(nat, 'rb').read(); bad += not ok; print(('PASA ' if ok else 'FALLA'), *c)
    if '--assert' in sys.argv and bad: sys.exit(1)

if __name__ == '__main__':
    main()
