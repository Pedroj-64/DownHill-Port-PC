#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Paso 0 del loader nativo: el lector ISO9660 de src/iso9660.hpp debe listar EXACTAMENTE los archivos de iso_extract/ (la extracción con 7z): mismos nombres, tamaños y CRC32. Necesita la imagen y iso_extract/ (datos locales, no va a CI).
/ Native-loader step 0: the ISO9660 reader of src/iso9660.hpp must list EXACTLY the files of iso_extract/ (the 7z extraction): same names, sizes and CRC32. Needs the image and iso_extract/ (local data, not for CI).
Con --unpack la lista pasa por src/ie.hpp (contenedores IE descomprimidos) y se compara con unpacked/ (salida de tools/unpack_ie.py). / With --unpack the list goes through src/ie.hpp (unpacked IE containers) and is compared with unpacked/ (tools/unpack_ie.py output).
Uso / Usage:  python3 tools/iso_check.py [--assert] [--unpack] [imagen.iso]   (por defecto la única *.iso de la raíz del repo / by default the only *.iso in the repo root)"""
import glob, os, subprocess, sys, zlib
R = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

def crc(path):
    c = 0
    with open(path, 'rb') as f:
        while True:
            b = f.read(1 << 22)
            if not b: break
            c = zlib.crc32(b, c)
    return c & 0xffffffff

def main():
    given = [a for a in sys.argv[1:] if not a.startswith('--')]
    iso = given[0] if given else (glob.glob(os.path.join(R, '*.iso')) or [None])[0]
    if not iso: print('no hay imagen .iso'); sys.exit(2)
    unp = '--unpack' in sys.argv
    r = subprocess.run([os.path.join(R, 'build/iso_check'), iso] + (['--unpack'] if unp else []), capture_output=True, text=True)
    if r.returncode: print('ERROR', r.stderr.strip()); sys.exit(1)
    files = {}; dirs = []
    for l in r.stdout.splitlines():
        if l.startswith('DIR '): dirs.append(l[4:]); continue
        size, c, path = l.split(' ', 2); files[path] = (int(size), int(c, 16))
    base = os.path.join(R, 'unpacked' if unp else 'iso_extract'); bad = 0
    for path, (size, c) in sorted(files.items()):
        p = os.path.join(base, path)
        if not os.path.isfile(p): bad += 1; print('FALTA en iso_extract', path); continue
        if os.path.getsize(p) != size: bad += 1; print('TAMAÑO', path, size, os.path.getsize(p)); continue
        if crc(p) != c: bad += 1; print('CRC', path)
    extra = [os.path.relpath(os.path.join(d, f), base) for d, _, fs in os.walk(base) for f in fs if os.path.relpath(os.path.join(d, f), base) not in files]
    print(f'{len(files)} archivos y {len(dirs)} directorios en la imagen; {bad} distintos de {os.path.basename(base)}/; {len(extra)} archivos de {os.path.basename(base)}/ que no están en la imagen {extra[:3]}')
    if '--assert' in sys.argv and (bad or not files): sys.exit(1)

if __name__ == '__main__':
    main()
