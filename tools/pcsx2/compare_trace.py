#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Compara un log tomado en PCSX2 (guía: tools/pcsx2/README.md) con Ground::sweep.
Uso: compare_trace.py trace.log nivel.col [--sweep-cli build/sweep_cli] [--tol-frac 0.002] [--tol-pos 0.5] [--tol-angle 3]
Formato del log (una línea por evento, espacios o comas, '#' = comentario; coordenadas del juego, Z arriba):
  SEG  n  x0 y0 z0  x1 y1 z1  r                       entrada de FUN_0021A908: un segmento por línea; los de una misma llamada comparten n
  HIT  n  frac  px py pz  nx ny nz  surface           retorno de FUN_00217450 con v0 != 0 (registro de 0x30 B: +0x1C frac, +0x10 punto, +0x20 normal, u16 en +6)
  NOHIT n                                              retorno de FUN_00217450 con v0 == 0
n = número de llamada: FUN_00217450 devuelve el MEJOR impacto de todos los segmentos de la llamada, así que se compara el mínimo de nuestros barridos (menor frac; empate: menor triángulo) con el HIT/NOHIT de ese n.
Informa por barrido: hit/no-hit, diferencia de fracción, de punto, ángulo de normal e igualdad de surface_id; y un resumen con la lista de discrepancias."""
import subprocess, sys, math, re, argparse

def parse(path):
    segs, res = [], []
    for ln in open(path):
        ln = ln.split('#')[0].strip()
        if not ln: continue
        t = re.split(r'[,\s]+', ln); k = t[0].upper(); v = t[1:]
        if k == 'SEG': segs.append((int(v[0]), [float(x) for x in v[1:8]]))
        elif k == 'HIT': res.append((int(v[0]), ('HIT', [float(x) for x in v[1:8]] + [int(v[8], 0)])))
        elif k == 'NOHIT': res.append((int(v[0]), ('NOHIT', None)))
    return segs, dict(res)

def run_sweeps(cli, col, segs):
    inp = ''.join(' '.join(repr(x) for x in s) + '\n' for _, s in segs)
    out = subprocess.run([cli, col], input=inp, capture_output=True, text=True, check=True).stdout.strip().splitlines()
    if len(out) != len(segs): raise SystemExit(f'sweep_cli devolvió {len(out)} líneas para {len(segs)} segmentos')
    return [('NOHIT', None) if o.startswith('NOHIT') else ('HIT', [float(x) for x in o.split()[1:10]]) for o in out]

def angle(a, b):
    d = sum(x * y for x, y in zip(a, b)); la = math.sqrt(sum(x * x for x in a)); lb = math.sqrt(sum(x * x for x in b))
    return math.degrees(math.acos(max(-1.0, min(1.0, d / (la * lb))))) if la and lb else 180.0

def best(results):
    """FUN_00217450: de todos los impactos de la llamada, el de menor fracción."""
    hits = [r for r in results if r[0] == 'HIT']
    return min(hits, key=lambda r: (r[1][0], r[1][8] if len(r[1]) > 8 else 0)) if hits else ('NOHIT', None)

def compare(segs, game, ours, tol_frac=0.002, tol_pos=0.5, tol_angle=3.0):
    """-> lista de (n, motivo) con las discrepancias (una comparación por llamada n)."""
    groups = {}
    for (n, s), o in zip(segs, ours): groups.setdefault(n, []).append((s, o))
    bad = []
    for n, items in groups.items():
        o = best([x[1] for x in items]); s = items[0][0]; g = game.get(n)
        if g is None: bad.append((n, 'sin resultado del juego para esta llamada')); continue
        if g[0] != o[0]: bad.append((n, f'juego={g[0]} nuestro={o[0]} primer segmento={s}')); continue
        if g[0] == 'NOHIT': continue
        gf, gp, gn, gs = g[1][0], g[1][1:4], g[1][4:7], int(g[1][7]); of, op, on, os_ = o[1][0], o[1][1:4], o[1][4:7], int(o[1][7])
        overlap = gf < -1e30 and of < -1e30
        if not overlap and abs(gf - of) > tol_frac: bad.append((n, f'frac juego={gf:.5f} nuestro={of:.5f}'))
        if (gf < -1e30) != (of < -1e30): bad.append((n, f'solape inicial distinto: juego frac={gf:g} nuestro={of:g}'))
        if math.dist(gp, op) > tol_pos: bad.append((n, f'punto juego={gp} nuestro={tuple(round(c,2) for c in op)}'))
        if angle(gn, on) > tol_angle: bad.append((n, f'normal juego={gn} nuestro={tuple(round(c,3) for c in on)}'))
        if gs != os_: bad.append((n, f'surface juego={gs:#x} nuestro={os_:#x}'))
    return bad

if __name__ == '__main__':
    ap = argparse.ArgumentParser(); ap.add_argument('log'); ap.add_argument('col'); ap.add_argument('--sweep-cli', default='build/sweep_cli')
    ap.add_argument('--tol-frac', type=float, default=0.002); ap.add_argument('--tol-pos', type=float, default=0.5); ap.add_argument('--tol-angle', type=float, default=3.0)
    a = ap.parse_args(); segs, game = parse(a.log); ours = run_sweeps(a.sweep_cli, a.col, segs)
    bad = compare(segs, game, ours, a.tol_frac, a.tol_pos, a.tol_angle)
    print(f'{len(segs)} segmentos en {len({n for n, _ in segs})} llamadas, {len({n for n, _ in bad})} con discrepancias ({len(bad)} diferencias)')
    for n, why in bad[:50]: print(f'  #{n}: {why}')
    sys.exit(1 if bad else 0)
