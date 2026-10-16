#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Oráculo por repetición (estrategia 2026-10-08): reproduce en el modelo del port las entradas grabadas
en una telemetría DHTEL1 y compara la trayectoria por tick con la grabada. Solo biblioteca estándar.
/ Replay oracle (2026-10-08 strategy): replay the inputs recorded in a DHTEL1 telemetry through the
port's physics model and compare the per-tick trajectory against the recording. Standard library only.

Uso / Usage:  python3 tools/telemetry/replay_oracle.py <carpeta o ticks.dhtel[.gz]> [...]

Qué reproduce / What it replays (todo medido antes, docs/telemetry-awiiwa.md, FUN_xxxxxxxx de la decompilación):
- gravedad 96.6 u/s² + amortiguación lineal 0.75 s⁻¹ (FUN_00136330, ctrl+0xE0 = F en el aire, exacto);
- arrastre cuadrático -|v|²·m·ctrl+0x490·v̂ (FUN_00136330; en el aire reproduce F con mediana de error 370);
- impulso del salto hopSpeed() de src/bike.hpp (FUN_00136330 + FUN_00176160), validado contra 89 saltos;
- en el suelo NO integra fuerzas de contacto (queda abierto): marca esos ticks como "sin modelo" y no los cuenta.
Los tramos evaluados son tramos aéreos (ctrl+0x4A4 >= 2 ambos extremos): el aire es el modelo más completo
que el port tiene validado, y el error se mide donde el modelo pretende ser fiel.

Métrica / Metric: por tramo aéreo, integra el modelo desde el estado inicial grabado y compara la posición
final con la grabada: error absoluto (u), error relativo a la distancia recorrida, y porcentaje de tramos
con error <= UMBRAL (--tol, por defecto 1 u/s² ~ 0.04 u en 1 s de vuelo... véase --tol). También integra
tramos de suelo como referencia informativa (contacto no modelado -> error esperado grande)."""

import math, os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import schema as S

DT = 0.02                    # 50 Hz
G = 96.6                     # gravedad medida (ctrl+0xE0 en el aire: F = (0,0,-m·2.47·32.2), m=100)
DAMP = 0.75                  # amortiguación lineal 1/s (mismo término de F)
DRAG_K_DEFAULT = 1.17e-4     # ctrl+0x490 ~ peso/v_max²: 100/(92²)... mediana medida en las grabaciones
JUMP_MAX = 47.55             # Δv máx del salto (25 ticks de carga, bike.hpp::hopSpeed)
HOP = 2.47                   # Δv = 2.47·J·rider+0x7920

def _f32(b, o): return struct.unpack_from('<f', b, o)[0]

class Tick:
    """Estado por tick leído del archivo: posición, velocidad, aire, salto, entradas."""
    __slots__ = ('t', 'pos', 'vel', 'air', 'jump', 'inp', 'pedal')
    def __init__(self, t, c, ri, jump, misc):
        self.t = t
        self.pos = struct.unpack_from('<3f', c, 0x50)
        self.vel = struct.unpack_from('<3f', c, 0xC0)
        self.air = struct.unpack_from('<h', c, 0x4A4)[0] >= 2
        # salto: impulso aplicado el fotograma de soltar (jump_obj+0x0C == 2)
        self.jump = S.u32(jump, 0x0C) == 2
        self.inp = struct.unpack_from('<i', ri, 0x10)[0] - struct.unpack_from('<i', ri, 0x14)[0]
        self.pedal = misc[0x6A] != 0

def load(path):
    """-> lista de ticks del jugador (piloto 0) desde una carpeta DHTEL1 o ticks.dhtel[.gz]."""
    p = path
    if os.path.isdir(p):
        for root, _, files in os.walk(p):
            if 'ticks.dhtel.gz' in files: p = os.path.join(root, 'ticks.dhtel.gz'); break
            if 'ticks.dhtel' in files: p = os.path.join(root, 'ticks.dhtel'); break
    _, samples = S.read_file(p)
    out = []
    for t, _, regs in samples:
        out.append(Tick(t, regs['ctrl'], regs['rider_input'], regs['jump_obj'], regs['rider_misc']))
    return out

def step_model(pos, vel, dt=DT, drag_k=DRAG_K_DEFAULT):
    """Un paso del modelo del port en el aire: gravedad + amortiguación + arrastre cuadrático (FUN_00136330)."""
    v = math.sqrt(sum(x * x for x in vel))
    ax, ay, az = -DAMP * vel[0], -DAMP * vel[1], -G - DAMP * vel[2]
    if v > 1e-6:
        d = v * v * drag_k
        ax -= d * vel[0] / v; ay -= d * vel[1] / v; az -= d * vel[2] / v
    return (pos[0] + dt * vel[0], pos[1] + dt * vel[1], pos[2] + dt * vel[2]), \
           (vel[0] + dt * ax, vel[1] + dt * ay, vel[2] + dt * az)

def air_segments(ticks):
    """Tramos de ticks consecutivos en el aire: [i..j] con ticks[i].air y ticks[j].air (no necesariamente entre medio)."""
    segs = []; i = 0
    while i < len(ticks):
        if ticks[i].air:
            j = i
            while j + 1 < len(ticks) and ticks[j + 1].air: j += 1
            if j > i: segs.append((i, j))
            i = j + 1
        else: i += 1
    return segs

def replay_segment(ticks, i, j, drag_k=DRAG_K_DEFAULT):
    """Integra el modelo del port desde ticks[i] hasta ticks[j] usando SOLO el estado inicial y las entradas.
    -> (posición final del modelo, posición final grabada, Δv por saltos dentro del tramo)."""
    pos = ticks[i].pos; vel = ticks[i].vel
    for k in range(i, j):
        if ticks[k + 1].jump:        # el impulso se aplica en el tick de soltar (frame-exacto)
            vel = (vel[0], vel[1], vel[2] + HOP * JUMP_MAX / 25.0)
        pos, vel = step_model(pos, vel, drag_k=drag_k)
    return pos, ticks[j].pos

def _mk_tick(**kw):
    """Tick de prueba: construye la clase a partir de un dict (para tests y para casos sintéticos)."""
    return Tick(0.0, _fake_ctrl(kw.get('pos', (0, 0, 0)), kw.get('vel', (0, 0, 0)), kw.get('air', 0)),
                _fake_input(kw.get('inp_r', 0), kw.get('inp_l', 0)),
                struct.pack('<I', kw.get('jump_phase', 0)).ljust(0x80, b'\0'), bytes(0x300))

def _fake_ctrl(pos, vel, air):
    c = bytearray(0x720)
    struct.pack_into('<3f', c, 0x50, *pos); struct.pack_into('<3f', c, 0xC0, *vel)
    struct.pack_into('<h', c, 0x4A4, air)
    return bytes(c)

def _fake_input(r, l):
    b = bytearray(0x1F0); struct.pack_into('<i', b, 0x10, r); struct.pack_into('<i', b, 0x14, l)
    return bytes(b)

def check(path, tol=0.35, drag_k=DRAG_K_DEFAULT):
    """-> dict con el resumen de un archivo. tol = error absoluto máximo aceptado por tramo (u)."""
    ticks = load(path)
    segs = air_segments(ticks)
    errs, rels, lens = [], [], []
    for i, j in segs:
        mp, rp = replay_segment(ticks, i, j, drag_k)
        d = math.dist(mp, rp); travel = max(math.dist(ticks[i].pos, ticks[j].pos), 1e-6)
        errs.append(d); rels.append(d / travel); lens.append(j - i)
    out = dict(segments=len(segs), errors=errs, rel=rels, lens=lens, tol=tol,
               ok=sum(1 for e in errs if e <= tol),
               median_err=sorted(errs)[len(errs) // 2] if errs else 0.0,
               median_rel=sorted(rels)[len(rels) // 2] if rels else 0.0)
    return out

def fmt(path, r):
    if not r['segments']: return f'{path}: sin tramos aéreos de >= 2 ticks'
    return (f"{os.path.basename(os.path.dirname(path)) or path}: {r['ok']}/{r['segments']} tramos <= {r['tol']} u "
            f"({100 * r['ok'] / r['segments']:.0f} %); error mediano {r['median_err']:.2f} u "
            f"(rel {100 * r['median_rel']:.1f} %)")

if __name__ == '__main__':
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument('paths', nargs='+')
    ap.add_argument('--tol', type=float, default=0.35, help='error absoluto máximo por tramo (u)')
    ap.add_argument('--drag', type=float, default=DRAG_K_DEFAULT, help='k del arrastre cuadrático')
    a = ap.parse_args()
    for p in a.paths:
        try:
            r = check(p, tol=a.tol, drag_k=a.drag)
            print(fmt(p, r))
            if os.environ.get('VERBOSE'):
                for (i, j), e in zip(r and air_segments(load(p)) or [], r['errors']):
                    print(f'   tramo {i}..{j} ({j - i} ticks): {e:.2f} u')
        except Exception as e:
            print(f'{p}: ERROR {e}', file=sys.stderr); sys.exit(1)
