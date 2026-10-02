#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Prepara y lanza el prototipo jugable desde TU imagen de disco (Linux y Windows). Nada del juego sale de tu máquina ni entra al repo.
EN: set up and launch the playable prototype from YOUR disc image. No game data leaves your machine or enters the repo.

  python3 tools/dh.py doctor                         comprueba dependencias y estado / checks dependencies and state
  python3 tools/dh.py setup "Downhill Domination.iso" [--levels ALP2,ALPINEMX | --all] [--jobs N] [--rebuild]
  python3 tools/dh.py play [NIVEL]                   por defecto ALP2 / defaults to ALP2
  python3 tools/dh.py levels                         niveles exportados / exported levels

setup: extrae la ISO (iso_extract/), descomprime los contenedores IE (unpacked/), exporta los niveles pedidos (out/maps/) y arma la bici (out/play/bike1.mdl).
Todas esas carpetas están en .gitignore. Por defecto sólo se exportan ALP2 y ALPINEMX (~150 MB); --all exporta los 54 niveles (~4 GB, varios minutos)."""
import argparse, concurrent.futures as cf, os, pathlib, platform, shutil, subprocess, sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
TOOLS, ISO_DIR, UNPACKED, MAPS, PLAY = ROOT / 'tools', ROOT / 'iso_extract', ROOT / 'unpacked', ROOT / 'out' / 'maps', ROOT / 'out' / 'play'
BIKE = PLAY / 'bike1.mdl'
DEFAULT_LEVELS = ['ALP2', 'ALPINEMX']
BIKE_PARTS = ('CANFIELD', 'MAN100', 'WHBRIAN')   # cuadro, manillar+horquilla, rueda (BIKE/); la elección es estética, tools/assemble_bike.py acepta cualquier combinación
EXE = 'dhview.exe' if platform.system() == 'Windows' else 'dhview'

def say(msg): print(f'[dh] {msg}', flush=True)
def fail(msg): print(f'[dh] ERROR: {msg}', file=sys.stderr); sys.exit(1)
def py(script, *args, quiet=True):
    r = subprocess.run([sys.executable, str(TOOLS / script), *map(str, args)], cwd=ROOT, capture_output=quiet, text=True)
    if r.returncode: fail(f'{script} {" ".join(map(str, args))} falló / failed:\n{(r.stdout or "")[-600:]}{(r.stderr or "")[-600:]}')
    return r.stdout
def viewer():
    for p in (ROOT / 'build' / EXE, ROOT / 'build' / 'Release' / EXE, ROOT / 'build' / 'Debug' / EXE):
        if p.exists(): return p
def extractor():
    for n in ('7z', '7zz', '7za'):
        if shutil.which(n): return [n, 'x', '-y']
    if shutil.which('bsdtar'): return ['bsdtar', '-xf']
def levels_in(d): return sorted(p.stem for p in d.glob('*.NGP')) if d.exists() else []
def exported(): return sorted(p.stem for p in MAPS.glob('*.mdl') if not p.stem.endswith(('.sky', '.dome')) and (MAPS / f'{p.stem}.col').exists()) if MAPS.exists() else []

def doctor(_a=None):
    ok = True
    def row(name, good, hint=''):
        nonlocal ok; ok &= bool(good); print(f'  {"OK " if good else "FALTA/MISSING"} {name}' + ('' if good else f'  -> {hint}'))
    say('dependencias / dependencies')
    row('Python >= 3.8', sys.version_info >= (3, 8));
    for mod in ('PIL', 'numpy'):
        try: __import__(mod); row(f'python: {mod}', True)
        except ImportError: row(f'python: {mod}', False, f'pip install {"pillow" if mod == "PIL" else mod}')
    row('extractor de ISO (7z/7zz/7za/bsdtar)', extractor(), 'instala p7zip / install p7zip')
    row('cmake', shutil.which('cmake'), 'instala cmake / install cmake'); row('ninja o make', shutil.which('ninja') or shutil.which('make'), 'instala ninja / install ninja')
    say('estado / state')
    row(f'visor / viewer ({EXE})', viewer(), 'se compila en setup / built by setup')
    row('ISO extraída / extracted ISO (iso_extract/)', ISO_DIR.exists(), 'python3 tools/dh.py setup TU.iso')
    row('contenedores IE descomprimidos / unpacked (unpacked/)', (UNPACKED / 'LVL').exists(), 'python3 tools/dh.py setup TU.iso')
    ex = exported(); row(f'niveles exportados / exported levels: {", ".join(ex[:6]) or "ninguno / none"}{"..." if len(ex) > 6 else ""}', ex, 'python3 tools/dh.py setup TU.iso')
    row('bici ensamblada / assembled bike', BIKE.exists(), 'python3 tools/dh.py setup TU.iso')
    return 0 if ok else 1

def build(rebuild=False):
    if viewer() and not rebuild: say(f'visor ya compilado / viewer already built: {viewer()}'); return
    if not shutil.which('cmake'): fail('falta cmake / cmake missing (ver `doctor`)')
    gen = ['-G', 'Ninja'] if shutil.which('ninja') else []
    say('compilando el visor (C++20, SDL3 + OpenGL) / building the viewer')
    for cmd in (['cmake', '-S', '.', '-B', 'build', *gen, '-DCMAKE_BUILD_TYPE=Release'], ['cmake', '--build', 'build', '--config', 'Release', '-j']):
        r = subprocess.run(cmd, cwd=ROOT)
        if r.returncode: fail('la compilación falló: faltan SDL3/OpenGL de desarrollo? / build failed: SDL3/OpenGL dev files missing?')
    if not viewer(): fail('no se encontró el ejecutable tras compilar / executable not found after build')

def export_level(n):
    py('extract_model.py', UNPACKED / 'LVL' / n, MAPS / f'{n}.mdl')
    py('collision.py', UNPACKED / 'LVL' / f'{n}.NGP', MAPS / f'{n}.col', '--instances')
    py('markers.py', UNPACKED / 'LVL' / f'{n}.NGP', 'x', MAPS / f'{n}.start.pts', MAPS / f'{n}.gates')
    return n

def make_bike():
    if BIKE.exists(): say('bici ya ensamblada / bike already assembled'); return
    PLAY.mkdir(parents=True, exist_ok=True); parts = []
    for p in BIKE_PARTS:
        out = PLAY / f'{p}.part.mdl'; py('extract_model.py', UNPACKED / 'BIKE' / p, out); parts.append(out)
    py('assemble_bike.py', BIKE, *parts)
    for p in parts: p.unlink(missing_ok=True)

def setup(a):
    iso = pathlib.Path(a.iso).expanduser()
    if not iso.is_file() and not ISO_DIR.exists(): fail(f'no existe la ISO / ISO not found: {iso}')
    for m in ('PIL', 'numpy'):
        try: __import__(m)
        except ImportError: fail(f'falta el módulo de Python {m}: pip install {"pillow" if m == "PIL" else m}')
    if ISO_DIR.exists() and any(ISO_DIR.iterdir()): say('ISO ya extraída / ISO already extracted (iso_extract/)')
    else:
        ex = extractor() or fail('falta un extractor de ISO: instala p7zip (7z) / no ISO extractor: install p7zip (7z)')
        say('extrayendo la ISO / extracting the ISO (~2.6 GB)'); ISO_DIR.mkdir(parents=True, exist_ok=True)
        cmd = ex + ([f'-o{ISO_DIR}', str(iso)] if ex[0].startswith('7z') else [str(iso), '-C', str(ISO_DIR)])
        if subprocess.run(cmd, cwd=ROOT, stdout=subprocess.DEVNULL).returncode: fail('la extracción falló / extraction failed')
    if not (ROOT / 'iso_extract' / 'SLES_522.02').exists(): say('aviso / warning: no se encuentra SLES_522.02; esta versión sólo se ha probado con la PAL SLES_522.02 / only the PAL build SLES_522.02 has been tested')
    if (UNPACKED / 'LVL').exists(): say('contenedores IE ya descomprimidos / IE containers already unpacked')
    else: say('descomprimiendo los contenedores IE / unpacking IE containers (~3 GB)'); py('unpack_ie.py', ISO_DIR, UNPACKED)
    avail = levels_in(UNPACKED / 'LVL')
    if not avail: fail('no se encontraron niveles en unpacked/LVL: ¿es la ISO correcta? / no levels found: wrong ISO?')
    want = avail if a.all else [x.strip().upper() for x in a.levels.split(',') if x.strip()]
    bad = [x for x in want if x not in avail]
    if bad: fail(f'niveles desconocidos / unknown levels: {", ".join(bad)}. Disponibles / available: {", ".join(avail)}')
    todo = [x for x in want if not ((MAPS / f'{x}.mdl').exists() and (MAPS / f'{x}.gates').exists())]
    MAPS.mkdir(parents=True, exist_ok=True)
    if todo:
        say(f'exportando {len(todo)} nivel(es) / exporting {len(todo)} level(s): {", ".join(todo[:8])}{"..." if len(todo) > 8 else ""}')
        with cf.ThreadPoolExecutor(max_workers=a.jobs) as ex_:
            for i, n in enumerate(ex_.map(export_level, todo), 1): say(f'  [{i}/{len(todo)}] {n}')
    make_bike(); build(a.rebuild)
    say(f'listo / done. Juega con / play with:  python3 tools/dh.py play {want[0]}')
    return 0

def play(a):
    exe = viewer() or fail('el visor no está compilado; ejecuta `setup` / viewer not built; run `setup`')
    n = a.level.upper(); mdl = MAPS / f'{n}.mdl'
    if not mdl.exists(): fail(f'el nivel {n} no está exportado / level {n} not exported. Exportados / exported: {", ".join(exported()) or "ninguno / none"}. Usa `setup --levels {n}`')
    env = dict(os.environ, DH_PLAY='1');
    if BIKE.exists(): env['DH_BIKE'] = str(BIKE)
    say('W acelerar/accelerate · S frenar/brake · A/D girar/steer · Q/E inclinar/lean · Espacio saltar/jump · Enter reiniciar/respawn · T salida/start · Esc salir/quit')
    return subprocess.run([str(exe), str(mdl)], cwd=ROOT, env=env).returncode

def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0]); sub = ap.add_subparsers(dest='cmd', required=True)
    sub.add_parser('doctor').set_defaults(f=doctor); sub.add_parser('levels').set_defaults(f=lambda a: print('\n'.join(exported())) or 0)
    s = sub.add_parser('setup'); s.add_argument('iso'); s.add_argument('--levels', default=','.join(DEFAULT_LEVELS)); s.add_argument('--all', action='store_true')
    s.add_argument('--jobs', type=int, default=max(1, min(4, (os.cpu_count() or 2) // 2))); s.add_argument('--rebuild', action='store_true'); s.set_defaults(f=setup)
    p = sub.add_parser('play'); p.add_argument('level', nargs='?', default='ALP2'); p.set_defaults(f=play)
    a = ap.parse_args(); sys.exit(a.f(a))
if __name__ == '__main__': main()
