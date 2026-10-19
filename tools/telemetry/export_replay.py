#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Exporta una telemetría DHTEL1 a un binario plano por paso de física para `tests/replay_demo.cpp` (oráculo por repetición del port, hito M1).
Solo biblioteca estándar. La salida (`*.replay`) contiene datos del juego: NO va al repo (regla 4); escribe fuera, p. ej. ~/dh-states/replay/.
/ Exports a DHTEL1 telemetry to a flat per-physics-step binary for `tests/replay_demo.cpp` (the port's replay oracle, milestone M1). Standard library only.
The output (`*.replay`) holds game data: it does NOT go in the repo (rule 4); write it outside, e.g. ~/dh-states/replay/.

Uso / Usage:  python3 tools/telemetry/export_replay.py <carpeta o ticks.dhtel[.gz]> -o salida.replay

Formato / Format:  'DHRPL1\\0\\0' | u32 nfloats por registro | u32 nregistros | registros de f32.
Registro / record (NF = 40 floats; 32-39 = ctrl+0xE0 fuerza, ctrl+0x250 contacto, ctrl+0x430, masa; 40-42 = fila 0 del nodo (lateral), 43 = ctrl+0x48C rampa, 44/45 = rueda 0/1 en contacto, 46 = coef0 de la clase), coordenadas del port (Y arriba: x, z, -y del espacio NGP):
  0 t(s)  1-3 P  4-6 v  7-9 adelante / forward (matriz del nodo, fila +0x50)  10-12 arriba / up (fila +0x60); ctrl+0x190/0x170 es el marco de CONTACTO y no cambia en el aire  13 giro / steer (+ = derecha, der.-izq. normalizado, FUN_0012DFF0)
  14 cabeceo / lean (+1 = palabra arriba rider+0x1198 = morro arriba, -1 = abajo +0x119C)  15 pedal / pedal (rider+0x7A78, float 0..1; FUN_00136A88)  16 carga del salto (ticks, jump_obj+0x24)  17 suelta el salto (jump_obj+0x0C == 2)
  18 aire (ctrl+0x4A4 >= 2)  19 superficie (ctrl+0x206 & 0x1F)  20 ctrl+0x49C (truco)  21 esfuerzo extra / sprint (rider+0x7A69)  22 rider+0x796A (complementario del pedal; ¿freno?)  23 columna 2 de la tabla de superficies de la clase actual (rodadura, FUN_001380E0; x0.025 = 1/s)
  24 ctrl+0x490 (k del arrastre cuadrático del tick, = peso/vterm²)  25 ctrl+0x428 (parámetro de la velocidad terminal)  26 rider+0x7964 (temporizador de esfuerzo extra, s16)  27 rider+0x796C (índice de la tabla de esfuerzo, s32)  28 rider+0x4098 (banderas del piloto, u32 bajo: el bit 2 anula el pedal en FUN_00136A88)  29-31 normal de contacto del suelo ctrl+0x220 (x, y, z NGP -> coordenadas del port)"""
import argparse, math, os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import schema as S

NF = 47
MAGIC = b'DHRPL1\0\0'

def v3(b, o):
    x, y, z = struct.unpack_from('<3f', b, o)
    return (x, z, -y)                  # NGP (Z arriba) -> port (Y arriba), como tests/ride_demo.cpp

def steer_of(inp):
    return 0.0 if inp == 0 else float(inp) if abs(inp) == 1 else inp / 128.0   # igual que ctrl_oracle.u_of

def surf_col(path, col):
    """Columna `col` (0 = coeficiente de rozamiento de rueda, FUN_00132FD8) de la tabla de superficies de esa grabación (static.dhtel), 32 valores."""
    d = path if os.path.isdir(path) else os.path.dirname(path)
    for r, _, fs in os.walk(d):
        if 'static.dhtel' in fs:
            tb = S.read_file(os.path.join(r, 'static.dhtel'))[1][0][2]['surf_table']
            return [struct.unpack_from('<f', tb, k * 32 + 4 * col)[0] for k in range(32)]
    return [0.0] * 32

def surf_roll(path):
    """Columna 2 (rodadura, FUN_001380E0) de la tabla de superficies de esa grabación (static.dhtel), 32 valores; la tabla cambia de un nivel a otro."""
    d = path if os.path.isdir(path) else os.path.dirname(path)
    for r, _, fs in os.walk(d):
        if 'static.dhtel' in fs:
            tb = S.read_file(os.path.join(r, 'static.dhtel'))[1][0][2]['surf_table']
            return [struct.unpack_from('<f', tb, k * 32 + 8)[0] for k in range(32)]
    return [0.0] * 32

def export(path):
    roll = surf_roll(path); coef0 = surf_col(path, 0)
    if os.path.isdir(path):
        for r, _, fs in os.walk(path):
            for f in fs:
                if f.startswith('ticks.dhtel'): path = os.path.join(r, f)
    recs, prevP = [], None
    for t, _, p in S.read_file(path)[1]:
        c, ri, jo, misc, nd = p['ctrl'], p['rider_input'], p['jump_obj'], p['rider_misc'], p['node']
        P = v3(c, 0x50)
        if P == prevP: continue                      # una muestra por paso de física
        prevP = P
        inp = struct.unpack_from('<i', ri, 0x10)[0] - struct.unpack_from('<i', ri, 0x14)[0]
        lean = (struct.unpack_from('<i', ri, 0x18)[0] != 0) - (struct.unpack_from('<i', ri, 0x1C)[0] != 0)      # +1 = palabra «arriba» (rider+0x1198) = morro arriba en el port; validado con el cabeceo del nodo (error 0.58 -> 0.07)
        air = struct.unpack_from('<h', c, 0x4A4)[0] >= 2
        rec = [t, *P, *v3(c, 0xC0), *v3(nd, 0x50), *v3(nd, 0x60), steer_of(inp), float(lean), S.f32(misc, 0x178),
               float(S.u32(jo, 0x24)), float(S.u32(jo, 0x0C) == 2), float(air), float(struct.unpack_from('<H', c, 0x206)[0] & 0x1F), S.f32(c, 0x49C), float(misc[0x169] != 0), float(misc[0x6A] != 0), roll[struct.unpack_from('<H', c, 0x206)[0] & 0x1F],
               S.f32(c, 0x490), S.f32(c, 0x428), float(struct.unpack_from('<h', misc, 0x64)[0]), float(struct.unpack_from('<i', misc, 0x6C)[0]),
               float(struct.unpack_from('<I', p['rider_state'], 8)[0]), *v3(c, 0x220), *v3(c, 0xE0), *v3(c, 0x250), S.f32(c, 0x430), S.f32(c, 0x150),
               *v3(nd, 0x40), S.f32(c, 0x48C), float((struct.unpack_from('<I', c, 0x354)[0] & 0xFF) == 1), float((struct.unpack_from('<I', c, 0x3D4)[0] & 0xFF) == 1), coef0[struct.unpack_from('<H', c, 0x206)[0] & 0x1F]]
        recs.append(rec + [0.0] * (NF - len(rec)))
    return recs

def main():
    ap = argparse.ArgumentParser(); ap.add_argument('path'); ap.add_argument('-o', '--out', required=True); a = ap.parse_args()
    recs = export(a.path)
    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    with open(a.out, 'wb') as f:
        f.write(MAGIC + struct.pack('<II', NF, len(recs)))
        for r in recs: f.write(struct.pack(f'<{NF}f', *r))
    print(f'{a.out}: {len(recs)} pasos de física, {recs[-1][0] - recs[0][0]:.1f} s' if recs else 'sin datos', file=sys.stderr)

if __name__ == '__main__':
    main()
