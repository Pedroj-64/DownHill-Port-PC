#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Captura automática, sin jugar: arranca PCSX2 (Flatpak) con PINE activado, carga un savestate y muestrea la RAM del juego cada fotograma de física.
Uso: pine_capture.py --iso RUTA.iso --slot 1 --seconds 30 --out /tmp/dhcap/ejemplo.cap   [--no-launch]
El .cap (binario propio) y los savestates NO se versionan: --out debe quedar fuera del repo. Analiza con analyze_capture.py.
Requisitos: PCSX2 cerrado al empezar (se edita PCSX2.ini para EnablePINE = true; copia de seguridad en PCSX2.ini.bak) y la carpeta de la ISO accesible al Flatpak."""
import argparse, os, shutil, struct, subprocess, sys, time
sys.path.insert(0, os.path.dirname(__file__)); sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
import pine
from p2s import RIDER_BASE, RIDER_STRIDE, RIDER_COUNT_ADDR, PHYS_OFF

INI = os.path.expanduser('~/.var/app/net.pcsx2.PCSX2/config/PCSX2/inis/PCSX2.ini')
HITS = (0x2DD600, 0x800)            # pila de la física donde quedan los registros de impacto (savestate ALPINEMX; se confirma en el análisis)
MAGIC = b'DHCAP1\0\0'
MAX_RIDERS = 10                     # ALP2 trae 10 pilotos; fuera de 1..MAX_RIDERS el estado no es una carrera

def enable_pine():
    s = open(INI).read()
    if 'EnablePINE = true' in s: return False
    shutil.copy(INI, INI + '.bak'); open(INI, 'w').write(s.replace('EnablePINE = false', 'EnablePINE = true')); return True

def running(): return subprocess.run(['pgrep', '-x', 'pcsx2-qt'], capture_output=True).returncode == 0

def check_riders(n, limit):
    """Valida el nº de pilotos leído del estado (1..MAX_RIDERS) y devuelve cuántos capturar (<= limit)."""
    if not 1 <= n <= MAX_RIDERS: sys.exit('el savestate no es una carrera (nº de pilotos inválido)')
    return min(n, limit)

def wait_socket(timeout=90, resolve=None, poll=0.5):
    """Espera al socket PINE recalculando la ruta en cada vuelta (al lanzar, default_socket() cae a una ruta de respaldo hasta que el socket existe)."""
    resolve = resolve or pine.default_socket; t0 = time.time()
    while not os.path.exists(sock := resolve()):
        if time.time() - t0 > timeout: sys.exit(f'no aparece el socket PINE {sock}')
        time.sleep(poll)
    return sock

def record(p, out_path, riders, seconds, sync_rider=None, t0=None):
    """Muestrea la RAM de PCSX2 (ya con el estado cargado) cada paso de física y escribe un .cap. sync_rider: piloto cuya posición de cuerpo marca el paso (None = nodo del jugador, como antes)."""
    t0 = t0 or time.time()
    n = struct.unpack('<I', p.read_windows([(RIDER_COUNT_ADDR, 8)])[0][:4])[0]; print('pilotos:', n)
    n = check_riders(n, riders)
    # punteros fijos (nodo del cuerpo) una vez
    ptrs = p.read_windows([(RIDER_BASE + i * RIDER_STRIDE + 0x7928, 8) for i in range(n)])
    nodes = [struct.unpack_from('<I', b, 4)[0] for b in ptrs]
    windows = [(RIDER_COUNT_ADDR, 8)] + [(RIDER_BASE + i * RIDER_STRIDE + 0x7928, 8) for i in range(n)] + [(RIDER_BASE + i * RIDER_STRIDE + 0x7A58, 8) for i in range(n)] \
              + [(nd, 0x70) for nd in nodes] + [(RIDER_BASE + i * RIDER_STRIDE + PHYS_OFF, 0x1E0) for i in range(n)] + [HITS]
    out = open(out_path, 'wb'); out.write(MAGIC + struct.pack('<II', len(windows), 0)); [out.write(struct.pack('<II', w[0], w[1])) for w in windows]
    cw = (nodes[0] + 0x10, 16) if sync_rider is None else (RIDER_BASE + sync_rider * RIDER_STRIDE + PHYS_OFF + 0x60, 16)   # sincronía: nodo del jugador (por defecto) o cuerpo del piloto indicado (momento lineal P): cambia en cada paso de física
    last = None; frames = 0; t_end = time.time() + seconds; torn = 0
    while time.time() < t_end:
        c1 = p.read_windows([cw])[0]
        if c1 == last: time.sleep(0.001); continue                         # esperar un nuevo paso de física
        data = p.read_windows(windows); c2 = p.read_windows([cw])[0]
        if c2 != c1: torn += 1; last = c2; continue                        # la RAM cambió durante la lectura: descartar la muestra
        last = c1; out.write(struct.pack('<dI', time.time() - t0, sum(len(d) for d in data))); [out.write(d) for d in data]; frames += 1
    out.close(); print(f'{frames} muestras ({torn} descartadas por lectura partida) -> {out_path}')
    return frames

def main():
    ap = argparse.ArgumentParser(); ap.add_argument('--iso'); ap.add_argument('--slot', type=int, default=1); ap.add_argument('--seconds', type=float, default=30); ap.add_argument('--out', required=True)
    ap.add_argument('--no-launch', action='store_true', help='PCSX2 ya está abierto con PINE activo'); ap.add_argument('--riders', type=int, default=6); a = ap.parse_args()
    proc = None
    if not a.no_launch:
        if running(): sys.exit('PCSX2 está abierto: ciérralo (o usa --no-launch si ya tiene PINE activo).')
        print('PINE activado en el ini' if enable_pine() else 'PINE ya estaba activo')
        proc = subprocess.Popen(['flatpak', 'run', 'net.pcsx2.PCSX2', '-batch', '-fastboot', '--', a.iso], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    t0 = time.time(); p = pine.Pine(wait_socket())
    while time.time() - t0 < 120:                                        # esperar a que el juego corra
        try:
            if p.status() == 0 and p.game_id().startswith('SLES'): break
        except Exception: pass
        time.sleep(0.5)
    print('juego:', p.game_id(), p.title()); time.sleep(2); p.load_state(a.slot); print(f'savestate slot {a.slot} cargado'); time.sleep(1.5)
    record(p, a.out, a.riders, a.seconds, None, t0)
    if proc: subprocess.run(['flatpak', 'kill', 'net.pcsx2.PCSX2'])

if __name__ == '__main__': main()
