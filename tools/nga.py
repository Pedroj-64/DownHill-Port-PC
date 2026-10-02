#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Contenedor .NGA (animaciones): tabla de clips y cabecera de clip. Layout y límites en docs/formats/nga.md.
Uso: nga.py ARCHIVO.NGA   -> resumen. EN: .NGA animation container (clip table + clip header); curve payload NOT decoded."""
import struct, sys

def parse_nga(d):
    """-> (magic, clips). clips = lista de dict(index, off, size, hdr, id, dur, tracks, counts).
    Tabla: u32 magic, luego (u32 n, u32 off) con n consecutivo (empieza en 1 en BANIM/RANIM y continúa en SH*ANIM: 198, 205…) y off creciente; cada clip llega hasta el siguiente offset (el último, hasta el final)."""
    magic, = struct.unpack_from('<I', d); T = []; i = 4; first = None
    while i + 8 <= len(d):
        n, off = struct.unpack_from('<II', d, i)
        first = n if first is None else first
        if n != first + len(T) or off <= i or off > len(d) or (T and off <= T[-1]): break
        T.append(off); i += 8
    ends = T[1:] + [len(d)]; clips = []
    for k, (o, e) in enumerate(zip(T, ends)):
        hdr, cid, z, dur, nt = struct.unpack_from('<2HIfI', d, o)   # z: 0 casi siempre; en 4 clips de RANIM es un f32 (10.0…), sin descifrar
        counts = struct.unpack_from(f'<{nt}H', d, o + 0x12) if o + 0x12 + 2 * nt <= e else None
        clips.append(dict(index=(first or 1) + k, off=o, size=e - o, hdr=hdr, id=cid, dur=dur, tracks=nt, counts=counts, param=z))
    return magic, clips

if __name__ == '__main__':
    m, C = parse_nga(open(sys.argv[1], 'rb').read())
    print(f'magic {m:#x}, {len(C)} clips; pistas {min((c["tracks"] for c in C), default=0)}..{max((c["tracks"] for c in C), default=0)}')
