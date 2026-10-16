#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Interfaz nativa (src/uiextract.hpp): el bloque del HUD de un nivel y la imagen de cada pantalla de carga LOADBAR deben salir BYTE A BYTE iguales a tools/hud_export.py y tools/loadbar_export.py. Necesita unpacked/ (no va a CI).
/ Native UI (src/uiextract.hpp): a level's HUD block and each LOADBAR loading-screen image must be BYTE-IDENTICAL to tools/hud_export.py and tools/loadbar_export.py. Needs unpacked/ (not for CI).
Uso / Usage:  python3 tools/ui_check.py [--assert] [--no-hud]   (el HUD con Python tarda ~1 min: extract_model.py de un nivel; --no-hud lo omite)"""
import glob, os, subprocess, sys, tempfile
R = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

def main():
    bad = done = 0
    with tempfile.TemporaryDirectory() as t:
        if '--no-hud' not in sys.argv:
            py, nat = os.path.join(t, 'py', 'hud.dat'), os.path.join(t, 'nat.dat'); lv = os.path.join(R, 'unpacked/LVL/ALP2')
            subprocess.run([sys.executable, os.path.join(R, 'tools/hud_export.py'), lv, py], capture_output=True, check=True)
            subprocess.run([os.path.join(R, 'build/ui_check'), lv, 'hud', nat], check=True); done += 1
            ok = open(py, 'rb').read() == open(nat, 'rb').read(); bad += not ok; print(('PASA ' if ok else 'FALLA'), 'HUD (ALP2)')
        pyd = os.path.join(t, 'lbr'); subprocess.run([sys.executable, os.path.join(R, 'tools/loadbar_export.py'), os.path.join(R, 'unpacked/LOADBAR'), pyd], capture_output=True, check=True)
        n = 0
        for f in sorted(glob.glob(os.path.join(R, 'unpacked/LOADBAR/*.NGP'))):
            name = os.path.basename(f)[:-4]; ref = os.path.join(pyd, name + '.lbr'); nat = os.path.join(t, name + '.lbr'); r = subprocess.run([os.path.join(R, 'build/ui_check'), f[:-4], 'lbr', nat])
            have_ref, have_nat = os.path.exists(ref), r.returncode == 0
            if have_ref != have_nat: bad += 1; print('EXISTENCIA', name, have_ref, have_nat); continue
            if have_ref and open(ref, 'rb').read() != open(nat, 'rb').read(): bad += 1; print('DISTINTO', name)
            n += have_ref
        done += n; print(f'{n} pantallas de carga comparadas')
    print(f'{done} archivos comparados, {bad} distintos / compared, differing')
    if '--assert' in sys.argv and (bad or not done): sys.exit(1)

if __name__ == '__main__':
    main()
