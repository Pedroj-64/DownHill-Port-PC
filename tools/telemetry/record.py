#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Graba telemetría y fotos completas del juego mientras juegas en PCSX2 (Downhill Domination SLES-52202) y deja un .zip listo para enviar en la carpeta SEND/. Solo biblioteca estándar. Lee README.md.
Records telemetry and full game snapshots while you play in PCSX2 and leaves a ready-to-send .zip in the SEND/ folder. Standard library only. Read README.md.

Carrera / Race:   python3 tools/telemetry/record.py --level ALP2 --rider Cosmo              (Ctrl+C para terminar / to stop)
Recorrido / Tour: python3 tools/telemetry/record.py --tour --label menus                    (menús, carga, pausa, resultados: no necesita carrera / no race needed)
Mientras grabas escribe una nota + Enter ("derrape"). Escribe  snap menu_principal  + Enter para una FOTO COMPLETA del juego en ese momento. / Type a note + Enter; type  snap name  + Enter for a FULL snapshot.

Qué guarda / What it saves: (1) cada paso de física: tú y los demás pilotos (entradas, estado de la moto, nodos); (2) cada ~0,5 s: todos los pilotos enteros y el gestor de carrera;
(3) fotos completas (RAM del EE y, si se puede, savestate con VRAM y audio): al empezar, al terminar, periódicas, en cada cambio carrera/menú y con `snap`.
NO lee el teclado ni el mando del sistema, ni captura pantalla, ni mira nada de tu equipo: solo memoria del juego por PINE. Escribe solo en --out, nunca en el repo."""
import argparse, datetime, hashlib, json, os, platform, struct, sys, threading, time, zipfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import schema as S, snapshots as SN

TOOL_VERSION = '3.0'
ES = (os.environ.get('LC_ALL') or os.environ.get('LANG') or '').lower().startswith('es')
def tr(es, en): return es if ES else en
def say(es, en=None): print(es if ES or en is None else en, flush=True)

def in_git_repo(path):
    p = os.path.abspath(path)
    while True:
        if os.path.exists(os.path.join(p, '.git')): return True
        q = os.path.dirname(p)
        if q == p: return False
        p = q

def valid_ptr(v): return 0x100000 <= v < 0x2000000

def check_game(link, force=False):
    """Comprueba que el juego cargado es el soportado. -> (id, título). Lanza SystemExit con un mensaje claro si no."""
    gid = link.game_id(); title = link.title()
    if gid not in S.GAME_IDS and not force:
        raise SystemExit(tr(f'El juego cargado es "{gid}" ({title}); este script solo conoce {", ".join(S.GAME_IDS)} (Downhill Domination PAL). Usa --force bajo tu responsabilidad.',
                            f'The loaded game is "{gid}" ({title}); this script only knows {", ".join(S.GAME_IDS)} (Downhill Domination PAL). Use --force at your own risk.'))
    return gid, title

def race_state(link):
    """-> (nº de pilotos, puntero de nodo del jugador). Fuera de carrera: nº fuera de 1..10 o puntero inválido."""
    cnt, ptr = link.read_windows([(S.RIDER_COUNT_ADDR, 8), (S.RB + 0x7928, 8)])
    return struct.unpack_from('<I', cnt)[0], struct.unpack_from('<I', ptr, 4)[0]

def in_race(link):
    n, ptr = race_state(link); return 1 <= n <= S.MAX_RIDERS and valid_ptr(ptr)

def wait_for_race(link, timeout, now=time.monotonic, sleep=time.sleep):
    t0 = now(); shown = False
    while now() - t0 < timeout:
        n, ptr = race_state(link)
        if 1 <= n <= S.MAX_RIDERS and valid_ptr(ptr): return n
        if not shown: say('Esperando a que empieces una carrera (arranca un nivel y llega a la salida)...', 'Waiting for you to start a race (load a level and get to the start)...'); shown = True
        sleep(0.5)
    raise SystemExit(tr('No empezó ninguna carrera a tiempo.', 'No race started in time.'))

def snapshot_static(link, path):
    """Constantes, tablas y el piloto 0 entero (pequeño y rápido; se hace al empezar y al terminar)."""
    table = [(n, a, l, 0) for n, a, l in S.STATIC]
    with open(path, 'wb') as f:
        S.write_header(f, table); S.write_sample(f, 0.0, 0, b''.join(link.read_windows([(a, l) for _, a, l, _ in table])))

def record(link, f, seconds, n_riders=1, now=time.monotonic, sleep=time.sleep, stop=lambda: False, idle_force=0.06, lost_after=3.0, progress=None,
           require_race=True, min_interval=0.0, on_tick=None, slow=True):
    """Bucle de grabación. Escribe muestras en `f` (ya con cabecera de region_table(n_riders, slow)). -> estadísticas.
    Una muestra por paso de física (cambio de SYNC); si la moto no cambia, una forzada cada `idle_force` s. require_race=False (modo recorrido): graba aunque no haya carrera (a `min_interval`)."""
    table = S.region_table(n_riders, slow); fixed = [(n, a, l) for n, a, l, k in table if k == 0]; dyn = [n for n, _, _, k in table if k == 1]; slw = [(n, a, l) for n, a, l, k in table if k == 2]
    names = [n for n, _, _ in fixed]; wins = [(a, l) for _, a, l in fixed]; period = table.slow_period
    st = dict(samples=0, torn=0, forced=0, bad=0, slow=0, ended='time'); t0 = now(); end = t0 + seconds if seconds else float('inf')
    last_sync = None; last_emit = -1e9; bad_since = None; last_status = -1e9; paused = False; next_prog = t0 + 5; read_time = 0.0; idx = 0
    while now() < end and not stop():
        t = now()
        if on_tick: on_tick(t - t0)
        if t - last_status > 0.5:                                          # pausa/apagado: no grabar muestras repetidas
            last_status = t; paused = link.status() != 0
        if paused: sleep(0.05); continue
        s1 = link.read_windows([S.SYNC])[0]; changed = s1 != last_sync
        if (not changed and t - last_emit < idle_force) or t - last_emit < min_interval: sleep(0.001); continue
        data = link.read_windows(wins); regs = dict(zip(names, data))
        cnt = struct.unpack_from('<I', regs['rider_count'])[0]; ptr = struct.unpack_from('<I', regs['node_ptr'], 4)[0]
        if require_race and not (1 <= cnt <= S.MAX_RIDERS and valid_ptr(ptr)):
            st['bad'] += 1; bad_since = bad_since if bad_since is not None else t
            if t - bad_since > lost_after: st['ended'] = 'race-ended'; break
            sleep(0.02); continue
        bad_since = None
        ptrs = [struct.unpack_from('<I', regs[S.node_ptr_name(n)], 4)[0] for n in dyn]       # punteros de nodo (jugador primero)
        reqs = [(p, S.NODE_LEN) if valid_ptr(p) else None for p in ptrs]
        got = link.read_windows([r for r in reqs if r]); it = iter(got); nodes = [next(it) if r else bytes(S.NODE_LEN) for r in reqs]
        slowdata = b''; flags = 0 if changed else 1
        if period and idx % period == 0 and slw: slowdata = b''.join(link.read_windows([(a, l) for _, a, l in slw])); flags |= 2
        s2 = link.read_windows([S.SYNC])[0]
        if s2 != s1 and require_race: st['torn'] += 1; last_sync = s2; continue            # la RAM cambió mientras leíamos: se descarta
        S.write_sample(f, t - t0, flags, b''.join(regs[n] for n in names) + b''.join(nodes) + slowdata)
        st['samples'] += 1; st['forced'] += 0 if changed else 1; st['slow'] += 1 if flags & 2 else 0; last_sync = s1; last_emit = t; idx += 1; read_time += now() - t
        if progress and t >= next_prog: next_prog = t + 5; progress(st, t - t0)
    st['seconds'] = now() - t0; st['mean_sample_ms'] = 1000 * read_time / st['samples'] if st['samples'] else 0.0
    return st

class Notes:
    """Líneas escritas por teclado mientras se graba (hilo en segundo plano): notas, y 'snap nombre' = pedir una foto completa."""
    def __init__(self, enabled=True, now=time.monotonic):
        self.items = []; self.snaps = []; self.now = now; self.lock = threading.Lock()
        if enabled and sys.stdin and sys.stdin.isatty(): threading.Thread(target=self._run, daemon=True).start()
    def add(self, text):
        with self.lock:
            if text.lower().startswith('snap'): self.snaps.append(text[4:].strip() or 'snap')
            self.items.append((self.now(), text))
    def pop_snap(self):
        with self.lock: return self.snaps.pop(0) if self.snaps else None
    def _run(self):
        try:
            for line in sys.stdin:
                if line.strip(): self.add(line.strip())
        except Exception: pass

class Snapper:
    """Fotos completas del juego: RAM del EE (siempre que se pueda) y savestate de PCSX2 (RAM+VRAM+audio+IOP+pantalla). Se piden al empezar, al terminar, cada `every` s, en cada cambio carrera/no carrera y con `snap`."""
    def __init__(self, link, folder, every=60.0, max_snaps=40, ram=True, states=None, notes=None, log=None, ram_size=SN.EE_RAM):
        self.link, self.dir, self.every, self.max, self.ram, self.states, self.notes = link, os.path.join(folder, 'snapshots'), every, max_snaps, ram, states, notes
        self.n = 0; self.next = 0.0; self.last_race = None; self.last_check = -1e9; self.log = log if log is not None else []; self.ram_size = ram_size; os.makedirs(self.dir, exist_ok=True)
    def take(self, label, t=0.0):
        if self.n >= self.max: say(f'  (límite de {self.max} fotos alcanzado)', f'  (limit of {self.max} snapshots reached)'); return
        self.n += 1; tag = f'snap_{self.n:03d}_' + ''.join(c if c.isalnum() or c in '-_' else '_' for c in label)[:30]; rec = dict(n=self.n, label=label, t=round(t, 2), files=[])
        say(f'  Foto {self.n} ({label})... el juego puede quedarse quieto unos segundos.', f'  Snapshot {self.n} ({label})... the game may freeze for a few seconds.')
        try:
            if self.ram: p = os.path.join(self.dir, tag + '.ram.gz'); SN.ram_dump(self.link, p, self.ram_size); rec['files'].append(os.path.basename(p))
        except Exception as e: say(f'  (volcado de RAM falló: {e})', f'  (RAM dump failed: {e})')
        try:
            if self.states: p = os.path.join(self.dir, tag + '.p2s'); self.states.save(p); rec['files'].append(os.path.basename(p))
        except Exception as e: say(f'  (savestate falló: {e}; se sigue solo con RAM)', f'  (savestate failed: {e}; continuing with RAM only)'); self.states = None
        self.log.append(rec)
    def tick(self, t):
        """Llamado en cada vuelta del bucle: ejecuta las fotos pendientes (periódicas, por cambio de estado o pedidas por el usuario)."""
        lbl = self.notes.pop_snap() if self.notes else None
        if lbl: self.take(lbl, t); return
        if t - self.last_check > 1.0:
            self.last_check = t
            try: r = in_race(self.link)
            except Exception: r = self.last_race
            if self.last_race is not None and r != self.last_race:
                self.last_race = r; self.take('race-start' if r else 'race-end', t); self.next = t + self.every; return
            self.last_race = r
        if self.every and t >= self.next: self.next = t + self.every; self.take('auto', t)

def make_zip(folder, dest_dir):
    os.makedirs(dest_dir, exist_ok=True); z = os.path.join(dest_dir, os.path.basename(folder.rstrip('/\\')) + '.zip')
    with zipfile.ZipFile(z, 'w', zipfile.ZIP_STORED if False else zipfile.ZIP_DEFLATED, compresslevel=1, allowZip64=True) as zf:       # los .gz/.p2s ya vienen comprimidos: nivel bajo para no tardar
        for root, _, files in os.walk(folder):
            for fn in sorted(files): zf.write(os.path.join(root, fn), os.path.join(os.path.basename(folder.rstrip('/\\')), os.path.relpath(os.path.join(root, fn), folder)))
    return z

def sha256(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for b in iter(lambda: f.read(1 << 20), b''): h.update(b)
    return h.hexdigest()

SEND_README = ('ENVÍA ESTA CARPETA (o el .zip grande que crea bundle.py) / SEND THIS FOLDER (or the single .zip made by bundle.py)\n\n'
               'Aquí quedan los .zip de tus grabaciones de Downhill Domination: telemetría y fotos del juego (sin teclado, sin capturas de tu pantalla, sin datos personales).\n'
               'Para juntarlo todo en un solo archivo:  python3 tools/telemetry/bundle.py\n'
               'NO lo subas a ningún repositorio público ni lo publiques: contiene memoria del juego. Mándalo solo a quien te lo pidió.\n\n'
               'These are the .zip files of your Downhill Domination recordings: telemetry and game snapshots (no keyboard, no screenshots of your screen, no personal data).\n'
               'To merge everything into one file:  python3 tools/telemetry/bundle.py\n'
               'Do NOT upload it to a public repository or publish it: it contains game memory. Send it only to whoever asked for it.\n')

def main(argv=None):
    ap = argparse.ArgumentParser(description='Telemetría y fotos completas de Downhill Domination vía PINE / Telemetry and full snapshots via PINE')
    ap.add_argument('--out', default=os.path.join(os.path.expanduser('~'), 'dh-telemetry'), help='carpeta base de salida (fuera del repo) / base output folder (outside the repo)')
    ap.add_argument('--level', default='', help='nivel, p. ej. ALP2 / level, e.g. ALP2'); ap.add_argument('--rider', default='', help='piloto que usas / rider you play')
    ap.add_argument('--label', default='', help='texto libre / free text'); ap.add_argument('--seconds', type=float, default=0, help='duración máxima; 0 = hasta Ctrl+C / max duration; 0 = until Ctrl+C')
    ap.add_argument('--tour', action='store_true', help='modo recorrido: menús, carga, pausa, resultados (no exige carrera) / tour mode: menus, loading, pause, results (no race needed)')
    ap.add_argument('--riders', type=int, default=S.MAX_RIDERS, help='pilotos a grabar (jugador + IA); 1 = solo tú / riders to record; 1 = only you')
    ap.add_argument('--light', action='store_true', help='sin canal lento ni fotos periódicas (PC lento) / no slow channel nor periodic snapshots (slow PC)')
    ap.add_argument('--snap-every', type=float, default=None, help='segundos entre fotos automáticas (0 = ninguna; por defecto 60, 30 en --tour) / seconds between automatic snapshots')
    ap.add_argument('--max-snaps', type=int, default=40); ap.add_argument('--no-ram', action='store_true', help='sin volcados de RAM / no RAM dumps')
    ap.add_argument('--no-states', action='store_true', help='no usar savestates de PCSX2 (solo RAM) / do not use PCSX2 savestates (RAM only)')
    ap.add_argument('--state-slot', type=int, default=10, help='ranura de savestate que se usa (se respalda y se restaura la tuya) / savestate slot used (yours is backed up and restored)')
    ap.add_argument('--sstates', help='carpeta de savestates de PCSX2 si no se encuentra sola / PCSX2 savestate folder if not auto-detected')
    ap.add_argument('--tcp', help='HOST:PUERTO (Windows: 127.0.0.1:28011) / HOST:PORT'); ap.add_argument('--socket', help='ruta del socket Unix de PINE / PINE Unix socket path')
    ap.add_argument('--pine-slot', type=int, default=28011, help='ranura PINE / PINE slot'); ap.add_argument('--wait', type=float, default=900, help='segundos esperando una carrera / seconds to wait for a race')
    ap.add_argument('--ram-size', type=lambda x: int(x, 0), default=SN.EE_RAM, help=argparse.SUPPRESS)       # pruebas
    ap.add_argument('--force', action='store_true', help='no comprobar el ID del juego / skip the game ID check'); ap.add_argument('--no-zip', action='store_true'); ap.add_argument('--no-notes', action='store_true')
    a = ap.parse_args(argv)
    base = os.path.abspath(a.out)
    if in_git_repo(base): raise SystemExit(tr('La carpeta de salida está dentro de un repositorio git; elige otra (p. ej. ~/dh-telemetry). Estos datos NO deben acabar en el repo.',
                                            'The output folder is inside a git repository; pick another (e.g. ~/dh-telemetry). This data must NOT end up in the repo.'))
    import pinelink
    tcp = None
    if a.tcp: h, _, p = a.tcp.rpartition(':'); tcp = (h or '127.0.0.1', int(p))
    try: link = pinelink.Link(path=a.socket, tcp=tcp, slot=a.pine_slot)
    except (FileNotFoundError, ConnectionRefusedError, OSError) as e:
        raise SystemExit(tr(f'No puedo conectar con PCSX2 por PINE: {e}\nAbre PCSX2, activa PINE (Ajustes > Avanzado > Activar PINE) y deja el juego en marcha.',
                            f'Cannot connect to PCSX2 over PINE: {e}\nOpen PCSX2, enable PINE (Settings > Advanced > Enable PINE) and leave the game running.'))
    say(f'Conectado: {link.where}', f'Connected: {link.where}')
    gid, title = check_game(link, a.force); say(f'Juego: {gid} {title}', f'Game: {gid} {title}')
    try: pcsx2 = link.version()
    except Exception: pcsx2 = '?'
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%SZ')
    tag = '_'.join(''.join(c if c.isalnum() or c in '-' else '_' for c in x)[:24] for x in (('tour',) if a.tour else ()) + (a.level, a.rider, a.label) if x)
    folder = os.path.join(base, 'sessions', f'dhtel_{stamp}' + (f'_{tag}' if tag else '')); os.makedirs(folder, exist_ok=True); send_dir = os.path.join(base, 'SEND'); os.makedirs(send_dir, exist_ok=True)
    with open(os.path.join(send_dir, 'LEEME_PRIMERO_README_FIRST.txt'), 'w', encoding='utf-8') as f: f.write(SEND_README)
    riders = 0 if a.tour else wait_for_race(link, a.wait); n_rec = max(1, min(a.riders, riders or S.MAX_RIDERS))
    if a.tour:
        try: riders = race_state(link)[0]
        except Exception: pass
        n_rec = max(1, min(a.riders, 1 if not 1 <= riders <= S.MAX_RIDERS else riders))
    say(('Modo recorrido' if a.tour else f'Carrera detectada ({riders} piloto(s); se graban {n_rec})') + '. Grabando... Escribe una nota + Enter; "snap nombre" + Enter = foto completa; Ctrl+C para terminar.',
        ('Tour mode' if a.tour else f'Race detected ({riders} rider(s); recording {n_rec})') + '. Recording... Type a note + Enter; "snap name" + Enter = full snapshot; Ctrl+C to stop.')
    notes = Notes(not a.no_notes); saver = None; snaplog = []
    if not a.no_states:
        dirs = [a.sstates] if a.sstates else SN.sstates_dirs(); found = [d for d in dirs if d and os.path.isdir(d)]
        if found:
            try: saver = SN.StateSaver(link, gid, found[0], a.state_slot); say(f'Savestates: {found[0]} (ranura {a.state_slot}; tu contenido se respalda y se restaura)', f'Savestates: {found[0]} (slot {a.state_slot}; your content is backed up and restored)')
            except Exception as e: say(f'Savestates desactivados: {e}', f'Savestates disabled: {e}')
        else: say('No encuentro la carpeta de savestates de PCSX2; solo volcados de RAM (usa --sstates RUTA para activarlos).', 'PCSX2 savestate folder not found; RAM dumps only (use --sstates PATH to enable).')
    every = (0 if a.light else (a.snap_every if a.snap_every is not None else (30.0 if a.tour else 60.0)))
    snapper = Snapper(link, folder, every=every, max_snaps=a.max_snaps, ram=not a.no_ram, states=saver, notes=notes, log=snaplog, ram_size=a.ram_size)
    snapper.take('start', 0.0); snapper.next = every if every else 0.0; snapper.last_race = in_race(link)
    snapshot_static(link, os.path.join(folder, 'static.dhtel')); t_start = time.monotonic()
    tick_path = os.path.join(folder, 'ticks.dhtel.gz'); stats = None
    def prog(st, t): say(f'  {t:5.0f} s  {st["samples"]} muestras ({st["samples"] / max(t, 1e-9):.0f}/s)', f'  {t:5.0f} s  {st["samples"]} samples ({st["samples"] / max(t, 1e-9):.0f}/s)')
    tab = S.region_table(n_rec, not a.light)
    try:
        with S.open_out(tick_path) as f:
            S.write_header(f, tab)
            try: stats = record(link, f, a.seconds, n_rec, progress=prog, require_race=not a.tour, min_interval=0.2 if a.tour else 0.0, on_tick=snapper.tick, slow=not a.light)
            except KeyboardInterrupt: stats = dict(samples=0, ended='ctrl-c')
    except (ConnectionError, IOError, OSError) as e:
        say(f'Se perdió la conexión con PCSX2 ({e}); se guarda lo grabado.', f'Lost the connection to PCSX2 ({e}); saving what was recorded.'); stats = stats or dict(ended='connection-lost')
    try: snapper.take('end', time.monotonic() - t_start)
    except Exception: pass
    try: snapshot_static(link, os.path.join(folder, 'static_end.dhtel'))
    except Exception: pass
    if saver: saver.restore()
    try: _, samples = S.read_file(tick_path); n = len(samples); dur = samples[-1][0] if samples else 0.0
    except ValueError: n, dur = 0, 0.0
    with open(os.path.join(folder, 'notes.jsonl'), 'w', encoding='utf-8') as f:
        for t, txt in notes.items: f.write(json.dumps(dict(t=round(t - t_start, 2), note=txt), ensure_ascii=False) + '\n')
    meta = dict(schema=S.SCHEMA, tool_version=TOOL_VERSION, created_utc=stamp, mode='tour' if a.tour else 'race', level=a.level, rider=a.rider, label=a.label, game_id=gid, game_title=title, pcsx2_version=pcsx2,
                riders_in_race=riders, riders_recorded=n_rec, platform=dict(system=platform.system(), release=platform.release(), machine=platform.machine(), python=platform.python_version()),
                samples=n, seconds=round(dur, 2), ended=(stats or {}).get('ended'), torn=(stats or {}).get('torn'), forced=(stats or {}).get('forced'), slow_samples=(stats or {}).get('slow'),
                mean_sample_ms=round((stats or {}).get('mean_sample_ms', 0), 2), notes=len(notes.items), snapshots=snaplog, slow_period=tab.slow_period,
                regions=[dict(name=nm, addr=ad, len=ln, kind=k) for nm, ad, ln, k in tab], static=[dict(name=nm, addr=ad, len=ln) for nm, ad, ln in S.STATIC], ticks_sha256=sha256(tick_path))
    with open(os.path.join(folder, 'meta.json'), 'w', encoding='utf-8') as f: json.dump(meta, f, indent=2, ensure_ascii=False)
    say(f'\nListo: {n} muestras, {dur:.1f} s, {len(snaplog)} foto(s).', f'\nDone: {n} samples, {dur:.1f} s, {len(snaplog)} snapshot(s).')
    if (stats or {}).get('mean_sample_ms', 0) > 18 and not a.tour: say('Aviso: leer tantos pilotos va justo a 50/s; prueba --riders 1 o --light si el juego va a tirones.', 'Note: reading that many riders is tight at 50/s; try --riders 1 or --light if the game stutters.')
    if not a.no_zip:
        z = make_zip(folder, send_dir); say(f'\n>>> ENVÍA: {send_dir}\n>>> (este archivo: {z}, {os.path.getsize(z) / 1e6:.1f} MB)', f'\n>>> SEND: {send_dir}\n>>> (this file: {z}, {os.path.getsize(z) / 1e6:.1f} MB)')
    say(f'Comprueba la grabación:  python3 tools/telemetry/check.py "{folder}"', f'Check the recording:  python3 tools/telemetry/check.py "{folder}"')
    return 0

if __name__ == '__main__':
    try: sys.exit(main())
    except KeyboardInterrupt: sys.exit(130)
