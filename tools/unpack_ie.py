#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Descomprime contenedores 'IE\\x03\\x04' (header 20B + u16 namelen... + nombre + deflate crudo)."""
import sys, zlib, struct, pathlib
src, dst = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2])
ok = bad = raw = 0
for f in src.rglob('*'):
    if not f.is_file(): continue
    d = f.read_bytes()
    out = dst / f.relative_to(src)
    out.parent.mkdir(parents=True, exist_ok=True)
    if d[:4] != b'IE\x03\x04':
        out.write_bytes(d); raw += 1; continue
    nlen = struct.unpack_from('<I', d, 0x1c-4+0)[0] if False else None
    # nombre empieza en 0x1e tras u32 len en 0x1a..; deflate sigue al nombre
    n = struct.unpack_from('<I', d, 0x1a)[0] if d[0x1a:0x1e] != b'' else 0
    start = 0x1e + n
    try:
        out.write_bytes(zlib.decompressobj(-15).decompress(d[start:])); ok += 1
    except Exception as e:
        out.write_bytes(d); bad += 1; print('FAIL', f, e)
print(f'descomprimidos={ok} sin_comprimir={raw} fallidos={bad}')
