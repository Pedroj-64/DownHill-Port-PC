#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Junta todos los .zip de la carpeta SEND/ en un solo archivo para enviar. / Merges every .zip in the SEND/ folder into one file to send.
Uso / Usage:  python3 tools/telemetry/bundle.py [--out ~/dh-telemetry]"""
import argparse, datetime, os, sys, zipfile

def main(argv=None):
    ap = argparse.ArgumentParser(); ap.add_argument('--out', default=os.path.join(os.path.expanduser('~'), 'dh-telemetry')); a = ap.parse_args(argv)
    base = os.path.abspath(a.out); send = os.path.join(base, 'SEND')
    zips = sorted(f for f in os.listdir(send) if f.startswith('dhtel_') and f.endswith('.zip')) if os.path.isdir(send) else []
    if not zips: print(f'No hay .zip en / No .zip files in {send}. Graba primero con / Record first with record.py'); return 1
    name = os.path.join(base, 'dhtel_bundle_' + datetime.datetime.now().strftime('%Y%m%d_%H%M%S') + '.zip')
    with zipfile.ZipFile(name, 'w', zipfile.ZIP_STORED) as zf:
        for z in zips: zf.write(os.path.join(send, z), z)
    print(f'{len(zips)} grabación(es) -> {name}  ({os.path.getsize(name) / 1e6:.1f} MB)\n>>> ENVÍA ESTE ARCHIVO / SEND THIS FILE: {name}'); return 0

if __name__ == '__main__': sys.exit(main())
