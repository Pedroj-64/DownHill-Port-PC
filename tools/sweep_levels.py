#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Barrido de todos los niveles SIN VENTANA: ejecuta `dhview` sobre cada nivel de tu imagen, lee su registro y saca una tabla por nivel
(¿cargó la colisión?, ¿hay rejilla de salida? sí/no, ¿cuántos rivales y peatones?, puertas del piloto automático, errores) para repetirla tras cada cambio.
Con --baseline compara con un barrido anterior (--json) y sale con 1 si hay regresiones. No guarda datos del juego ni rutas: solo contadores.
Uso: tools/sweep_levels.py imagen.iso [--dhview build/dhview] [--ticks 600] [--jobs 4] [--levels ALP2 MOAB ...] [--json salida.json] [--baseline anterior.json]
EN: headless sweep of every level; per-level table of collision / start grid / rivals / pedestrians / gates / errors; --baseline flags regressions (exit 1)."""
import argparse, json, os, re, subprocess, sys
from concurrent.futures import ThreadPoolExecutor

ERR = re.compile(r'(?i)(no se puede|inv[aá]lid|faltan |fall[oó]|carga nativa de .*:|modo jugable: hace falta|\bnan\b|\binf\b)')
ENV = dict(SDL_VIDEODRIVER='offscreen', SDL_AUDIODRIVER='dummy', DH_PLAY='1', DH_LOADHOLD='0', DH_MUSIC='OFF', DH_SFX='OFF', DH_AUTOPLAY='1', DH_DEBUG='1', DH_DT='0.0166667')

def list_levels(iso_check_output):
    """Niveles de carrera a partir de la salida de `iso_check imagen.iso` (líneas «tamaño crc ruta»); se omiten los podios POD*."""
    lv = sorted(set(re.findall(r'^\d+ [0-9a-f]+ LVL/([A-Z0-9_]+)\.NGP$', iso_check_output, re.M)))
    return [n for n in lv if not n.startswith('POD')]

def parse_log(text, rc=0, timed_out=False):
    """Registro de dhview -> dict de contadores (None = la línea no apareció)."""
    def first(p, g=1):
        m = re.search(p, text); return int(m.group(g)) if m else None
    gates = re.search(r'puertas=(\d+)/(\d+)', text)
    errs = sorted({l.strip()[:80] for l in text.splitlines() if ERR.search(l) and 'errores máx' not in l})
    r = dict(collision=first(r'colisión: (\d+) triángulos'), start_grid=first(r'startPts=(\d+)'), rivals=first(r'rivales: (\d+) modelos') or 0,
             pedestrians=first(r'peatones: (\d+) puntos') or 0, gates=f'{gates.group(1)}/{gates.group(2)}' if gates else None, errors=errs, rc=rc)
    r['status'] = 'TIMEOUT' if timed_out else 'ERROR' if rc != 0 or errs or not r['collision'] else 'OK'
    return r

def run_level(dhview, iso, level, ticks, timeout):
    env = {k: v for k, v in os.environ.items() if not k.startswith('DH_')}; env.update(ENV, DH_TICKS=str(ticks))
    try:
        p = subprocess.run([dhview, iso, level], env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=timeout)
        return parse_log(p.stdout.decode('utf-8', 'replace'), p.returncode)
    except subprocess.TimeoutExpired:
        return parse_log('', 0, True)

def table(res):
    rows = ['| nivel | colisión (tri.) | rejilla de salida | rivales | peatones | puertas | estado |', '|---|---|---|---|---|---|---|']
    for n, r in sorted(res.items()):
        rows.append(f"| {n} | {r['collision'] if r['collision'] is not None else '-'} | {'-' if r['start_grid'] is None else 'sí' if r['start_grid'] else 'NO'} | {r['rivals']} | {r['pedestrians']} | {r['gates'] or '-'} | {r['status']}{' (' + '; '.join(r['errors']) + ')' if r['errors'] else ''} |")
    return '\n'.join(rows)

def regressions(base, cur):
    """Diferencias que empeoran respecto al barrido anterior."""
    out = []
    for n, b in sorted(base.items()):
        c = cur.get(n)
        if c is None: out.append(f'{n}: falta en el barrido nuevo'); continue
        if (b['collision'] or 0) > 0 and not c['collision']: out.append(f'{n}: ya no carga la colisión')
        if b['start_grid'] and not c['start_grid']: out.append(f'{n}: ya no hay rejilla de salida')
        if c['rivals'] < b['rivals']: out.append(f"{n}: menos rivales ({b['rivals']} -> {c['rivals']})")
        if c['pedestrians'] < b['pedestrians']: out.append(f"{n}: menos peatones ({b['pedestrians']} -> {c['pedestrians']})")
        if b['status'] == 'OK' and c['status'] != 'OK': out.append(f"{n}: estado OK -> {c['status']}")
        for e in set(c['errors']) - set(b['errors']): out.append(f'{n}: error nuevo: {e}')
    return out

def main(argv=None):
    a = argparse.ArgumentParser(description=__doc__.split('\n')[0]); a.add_argument('iso'); a.add_argument('--dhview', default='build/dhview'); a.add_argument('--ticks', type=int, default=300)
    a.add_argument('--jobs', type=int, default=4); a.add_argument('--timeout', type=int, default=300); a.add_argument('--levels', nargs='*'); a.add_argument('--json'); a.add_argument('--baseline')
    o = a.parse_args(argv)
    levels = o.levels
    if not levels:
        chk = os.path.join(os.path.dirname(o.dhview) or '.', 'iso_check')
        levels = list_levels(subprocess.run([chk, o.iso], stdout=subprocess.PIPE, stderr=subprocess.STDOUT).stdout.decode('utf-8', 'replace'))
        if not levels: sys.exit('no se encontraron niveles (¿existe build/iso_check?; también vale --levels A B ...)')
    with ThreadPoolExecutor(max(1, o.jobs)) as ex: res = dict(zip(levels, ex.map(lambda n: run_level(o.dhview, o.iso, n, o.ticks, o.timeout), levels)))
    print(table(res)); bad = [n for n, r in res.items() if r['status'] != 'OK']; print(f'\n{len(res) - len(bad)}/{len(res)} niveles OK' + (f'; con problemas: {", ".join(bad)}' if bad else ''))
    if o.json: json.dump(res, open(o.json, 'w'), indent=1, sort_keys=True)
    if o.baseline:
        base = json.load(open(o.baseline)); base = {k: v for k, v in base.items() if k in res} if o.levels else base   # con --levels solo se comparan los niveles ejecutados
        reg = regressions(base, res)
        print('\nRegresiones:' if reg else '\nSin regresiones respecto a la línea base.'); [print(' -', r) for r in reg]
        return 1 if reg else 0
    return 0

if __name__ == '__main__': sys.exit(main())
