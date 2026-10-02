#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
# Author: Brandon Gil (nested IE container support and tests)
"""Descomprime contenedores IE, incluidas sus envolturas anidadas."""
import sys, zlib, struct, pathlib

MAGIC = b'IE\x03\x04'


def unpack_ie_data(data, max_layers=64):
    """Desenvuelve un archivo IE y devuelve sus datos finales.

    La variante 0x0a contiene otra capa IE sin compresión; las demás
    variantes conocidas contienen deflate sin cabecera zlib.
    """
    for _ in range(max_layers):
        if data[:4] != MAGIC:
            return data
        if len(data) < 0x1e:
            raise ValueError('cabecera IE truncada')
        variant = struct.unpack_from('<I', data, 4)[0]
        name_len = struct.unpack_from('<I', data, 0x1a)[0]
        start = 0x1e + name_len
        if start > len(data):
            raise ValueError('nombre IE truncado')
        if variant == 0x0a:
            data = data[start:]
        else:
            data = zlib.decompressobj(-15).decompress(data[start:])
    raise ValueError('demasiadas capas IE anidadas')


def main(src, dst):
    """Descomprime recursivamente los archivos de ``src`` dentro de ``dst``."""
    ok = bad = raw = 0
    for f in src.rglob('*'):
        if not f.is_file():
            continue
        d = f.read_bytes()
        out = dst / f.relative_to(src)
        out.parent.mkdir(parents=True, exist_ok=True)
        if d[:4] != MAGIC:
            out.write_bytes(d)
            raw += 1
            continue
        try:
            out.write_bytes(unpack_ie_data(d))
            ok += 1
        except Exception as e:
            out.write_bytes(d)
            bad += 1
            print('FAIL', f, e)
    print(f'descomprimidos={ok} sin_comprimir={raw} fallidos={bad}')


if __name__ == '__main__':
    main(pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2]))
