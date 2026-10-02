#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Recorre la cadena de registros de un .TEX/.RTX ya descomprimido (ver docs/en/formats.md)."""
import struct, sys, collections
d = open(sys.argv[1], 'rb').read()
nbins, first = struct.unpack_from('<II', d, 0)
bins = [struct.unpack_from('<II', d, 8 + 8*i) for i in range(nbins)]
off, n, stats = first * 16, 0, collections.Counter()
while off + 16 <= len(d):
    nxt, h, lo, hi = struct.unpack_from('<IIII', d, off)
    w, ht = 1 << (hi >> 8 & 0xf), 1 << (hi >> 12 & 0xf)
    psm, mips, bn = hi & 0x3f, hi >> 19 & 0xf, hi >> 27
    if n < 12 or '-v' in sys.argv: print(f'{off:#8x} next={nxt:#x} hash={h:08x} id={lo&0xffff} {w}x{ht} psm={psm:#x} mips={mips} bin={bn}')
    stats[(w, ht, psm)] += 1; n += 1
    if nxt == 0: break
    off += (nxt & ~3) * 4   # puntero uint*: el desplazamiento está en palabras
print(n, 'registros; bins:', bins); print(stats.most_common(8))
