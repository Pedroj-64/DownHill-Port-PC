#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Lee un .PTS de nivel (grafo de puntos de la pista) y exporta sus puntos. Uso: pts_path.py NIVEL.PTS out.pts [--chain N] [--stats]
Registro: 8 B de cabecera + 2048 * 32 B = f32 x,y,z; i16 a0,a1,a2,a3(=0),a4,a5,a6,a7; 4 B de relleno.
  a6 = índice del registro SIGUIENTE (verificado: mediana de distancia 38 u; forma cadenas/carriles que confluyen), a0/a1 = contadores crecientes de progreso (sin confirmar su unidad).
Sin --chain exporta todos los puntos; con --chain N sigue los enlaces a6 desde el registro N hasta repetir (carril principal)."""
import struct, sys, math, collections
d = open(sys.argv[1], 'rb').read(); N = (len(d) - 8) // 32
R = [struct.unpack_from('<3f8h4x', d, 8 + 32 * i) for i in range(N)]
idx = list(range(N))
if '--chain' in sys.argv:
    i, seen = int(sys.argv[sys.argv.index('--chain') + 1]), []
    while 0 <= i < N and i not in seen: seen.append(i); i = R[i][9]
    idx = seen
if '--stats' in sys.argv:
    heads = [i for i in range(N) if i not in {r[9] for r in R}]
    print(f'{N} registros, {len(heads)} cabezas de cadena; longitud de la selección: {sum(math.dist(R[a][:3], R[b][:3]) for a, b in zip(idx, idx[1:])):.0f} u')
open(sys.argv[2], 'wb').write(b''.join(struct.pack('<3f', *R[i][:3]) for i in idx))
print(f'{len(idx)} puntos -> {sys.argv[2]}')
