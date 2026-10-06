#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Comprueba y resume una grabación de telemetría (carpeta o .zip). Sale con 0 si parece buena y con 1 si no. / Checks and summarises a telemetry recording (folder or .zip). Exit 0 if it looks good.
Uso / Usage:  python3 tools/telemetry/check.py RUTA"""
import io, json, math, os, struct, sys, tempfile, zipfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import schema as S

def locate(path):
    """-> carpeta con ticks.dhtel (extrae el zip a un directorio temporal si hace falta)."""
    if os.path.isdir(path): return path
    if zipfile.is_zipfile(path):
        d = tempfile.mkdtemp(prefix='dhtel_'); zipfile.ZipFile(path).extractall(d)
        for root, _, files in os.walk(d):
            if 'ticks.dhtel.gz' in files or 'ticks.dhtel' in files: return root
    raise SystemExit('no encuentro ticks.dhtel(.gz) / cannot find ticks.dhtel(.gz)')

def analyse(folder):
    tp = os.path.join(folder, 'ticks.dhtel.gz')
    table, samples = S.read_file(tp if os.path.exists(tp) else os.path.join(folder, 'ticks.dhtel')); r = dict(samples=len(samples), problems=[], notes=[])
    if not samples: r['problems'].append('sin muestras / no samples'); return r
    ts = [t for t, _, _ in samples]; r['seconds'] = ts[-1] - ts[0]; r['rate'] = (len(ts) - 1) / r['seconds'] if r['seconds'] > 0 else 0.0
    gaps = [b - a for a, b in zip(ts, ts[1:])]; r['gaps_over_80ms'] = sum(1 for g in gaps if g > 0.08); r['forced'] = sum(1 for _, fl, _ in samples if fl & 1)
    speeds = []; air = 0; surf = {}; inp = dict(derecha_right=0, izquierda_left=0, cabeceo_arriba_leanup=0, cabeceo_abajo_leandown=0, boton_button=0, pedal_7A6A=0, analog_7A78=0)
    for t, fl, p in samples:
        c = p['ctrl']; vx, vy, vz = struct.unpack_from('<3f', c, 0xC0); v = math.sqrt(vx * vx + vy * vy + vz * vz)
        if math.isfinite(v): speeds.append(v)
        if struct.unpack_from('<h', c, 0x4A4)[0] >= 2: air += 1
        k = struct.unpack_from('<H', c, 0x396)[0] & 0x1F; surf[k] = surf.get(k, 0) + 1
        ri = p['rider_input']; rm = p['rider_misc']                         # rider+0x1180 base: +0x10 giro der., +0x14 izq., +0x18/+0x1C cabeceo, +0x38 botón; rider+0x7900 base: +0x6A pedal, +0x78 analógico
        if struct.unpack_from('<i', ri, 0x10)[0] != 0: inp['derecha_right'] += 1
        if struct.unpack_from('<i', ri, 0x14)[0] != 0: inp['izquierda_left'] += 1
        if struct.unpack_from('<i', ri, 0x18)[0] != 0: inp['cabeceo_arriba_leanup'] += 1
        if struct.unpack_from('<i', ri, 0x1C)[0] != 0: inp['cabeceo_abajo_leandown'] += 1
        if struct.unpack_from('<i', ri, 0x38)[0] != 0: inp['boton_button'] += 1
        if rm[0x6A] != 0: inp['pedal_7A6A'] += 1
        if struct.unpack_from('<f', rm, 0x78)[0] != 0.0: inp['analog_7A78'] += 1
    ai = {}
    for nm, _, _, _ in table:
        if nm.endswith('_ctrl') and nm.startswith('r'):
            v = [math.sqrt(sum(x * x for x in struct.unpack_from('<3f', p[nm], 0xC0))) for _, _, p in samples]; v = [x for x in v if math.isfinite(x)]
            if v: ai[nm[:-5]] = (round(max(v)), round(sum(v) / len(v)))
    snaps = sorted(os.listdir(os.path.join(folder, 'snapshots'))) if os.path.isdir(os.path.join(folder, 'snapshots')) else []
    r['snapshots'] = [(fn, os.path.getsize(os.path.join(folder, 'snapshots', fn))) for fn in snaps]
    r['others'] = ai; r.update(speed_max=max(speeds) if speeds else 0.0, speed_mean=sum(speeds) / len(speeds) if speeds else 0.0, air_fraction=air / len(samples), surfaces=surf, inputs=inp)
    if not snaps: r['problems'].append('sin fotos completas (snapshots/) / no full snapshots (snapshots/)')
    elif not any(fn.endswith('.ram.gz') and sz > 100000 for fn, sz in r['snapshots']): r['notes'].append('las fotos de RAM están vacías o no hay / RAM snapshots empty or missing')
    if r['seconds'] < 20: r['problems'].append('muy corta (<20 s) / too short (<20 s)')
    if r['rate'] < 30: r['problems'].append(f'frecuencia baja ({r["rate"]:.0f}/s; esperable ~50/s) / low sample rate')
    if r['speed_max'] < 5: r['problems'].append('la moto casi no se movió / the bike barely moved')
    if sum(inp.values()) == 0: r['problems'].append('no se ve ninguna entrada (¿jugaste con el mando de PCSX2?) / no input seen')
    if r['forced'] > 0.5 * len(samples): r['notes'].append('más de la mitad de las muestras son forzadas (moto parada o PCSX2 lento) / over half the samples are forced')
    return r

def main(argv=None):
    path = (argv or sys.argv[1:] or [''])[0]
    if not path: raise SystemExit(__doc__)
    folder = locate(path)
    try: meta = json.load(open(os.path.join(folder, 'meta.json')))
    except Exception: meta = {}
    try: r = analyse(folder)
    except ValueError as e: print('ARCHIVO INVÁLIDO / INVALID FILE:', e); return 1
    print(f'Grabación / Recording: {meta.get("label", "")!r}  juego/game {meta.get("game_id", "?")}  PCSX2 {meta.get("pcsx2_version", "?")}')
    print(f'  muestras / samples: {r["samples"]}   duración / duration: {r.get("seconds", 0):.1f} s   ritmo / rate: {r.get("rate", 0):.0f}/s   huecos >80 ms / gaps: {r.get("gaps_over_80ms", "-")}   forzadas / forced: {r.get("forced", "-")}')
    if r['samples']:
        print(f'  velocidad / speed: máx {r["speed_max"]:.0f}  media {r["speed_mean"]:.0f} u/s   en el aire / airborne: {100 * r["air_fraction"]:.0f}%   superficies / surfaces: {dict(sorted(r["surfaces"].items()))}')
        print('  entradas vistas (muestras) / inputs seen (samples):', r['inputs'])
        print('  fotos / snapshots:', [(fn, round(sz / 1e6, 1)) for fn, sz in r['snapshots']] or 'ninguna / none')
        if r['others']: print('  otros pilotos / other riders (vel. máx, media):', r['others'])
    for n in r['notes']: print('  nota / note:', n)
    for p in r['problems']: print('  PROBLEMA / PROBLEM:', p)
    ok = not r['problems']; print('RESULTADO / RESULT:', 'OK - envía el .zip / send the .zip' if ok else 'REVISAR / CHECK - vuelve a grabar / record again')
    return 0 if ok else 1

if __name__ == '__main__': sys.exit(main())
