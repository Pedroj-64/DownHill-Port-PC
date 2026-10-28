#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Lista los modelos de piloto de alta resolución de SHELL/UI.NGP (los de la pantalla de selección): paquetes VIF con `UNPACK V4-32 (n>=16)` seguido de `UNPACK V3-8` del mismo n
(formato de piloto, docs/formats/rider.md) agrupados por proximidad (hueco <= 0x3000) con vértices y caja. EN: lists the hi-res rider models of SHELL/UI.NGP.
Uso: ui_riders.py UI.NGP   (extraer antes con build/iso_cat imagen.iso SHELL/UI.NGP salida --unpack)"""
import sys, numpy as np
def groups(d, gap=0x3000):
    w = np.frombuffer(d[:len(d) // 4 * 4], dtype='<u4'); hits = []
    for i in range(len(w) - 3):
        v = int(w[i])
        if (v >> 24 & 0x7f) == 0x6c and (v >> 16 & 0xff) >= 16:
            n = v >> 16 & 0xff; j = i + 1 + n * 4
            if j < len(w) and (int(w[j]) >> 24 & 0x7f) == 0x6a and (int(w[j]) >> 16 & 0xff) == n: hits.append((i * 4, n))
    g = [[hits[0]]] if hits else []
    for h in hits[1:]:
        if h[0] - g[-1][-1][0] > gap: g.append([h])
        else: g[-1].append(h)
    return g
if __name__ == '__main__':
    d = open(sys.argv[1], 'rb').read()
    for g in groups(d):
        n = sum(k for _, k in g); V = np.vstack([np.frombuffer(d[o + 4:o + 4 + 16 * k], dtype='<f4').reshape(k, 4)[:, :3] for o, k in g])
        print(f'0x{g[0][0]:x}..0x{g[-1][0]:x} paquetes={len(g)} vértices={n} min={V.min(0).round(2)} max={V.max(0).round(2)}')
