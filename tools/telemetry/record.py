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
import argparse, datetime, hashlib, json, os, platform, shutil, struct, sys, threading, time, zipfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import schema as S, snapshots as SN, detect as D, check as C
from ui import ES, tr, say

TOOL_VERSION = '4.0'

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
           require_race=True, min_interval=0.0, on_tick=None, slow=True, on_sample=None, stats=None):
    """Bucle de grabación. Escribe muestras en `f` (ya con cabecera de region_table(n_riders, slow)). -> estadísticas.
    Una muestra por paso de física (cambio de SYNC); si la moto no cambia, una forzada cada `idle_force` s. require_race=False (modo recorrido): graba aunque no haya carrera (a `min_interval`)."""
    table = S.region_table(n_riders, slow); fixed = [(n, a, l) for n, a, l, k in table if k == 0]; dyn = [n for n, _, _, k in table if k == 1]; slw = [(n, a, l) for n, a, l, k in table if k == 2]
    names = [n for n, _, _ in fixed]; wins = [(a, l) for _, a, l in fixed]; period = table.slow_period
    st = stats if stats is not None else {}; st.update(samples=0, torn=0, forced=0, bad=0, slow=0, ended='time'); t0 = now(); end = t0 + seconds if seconds else float('inf')      # `stats`: el llamador conserva las cifras aunque haya Ctrl+C
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
        if on_sample: on_sample(t - t0, regs, nodes)
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
    def __init__(self, link, folder, every=60.0, max_snaps=40, ram=True, states=None, notes=None, log=None, ram_size=SN.EE_RAM, watch=None):
        self.link, self.dir, self.every, self.max, self.ram, self.states, self.notes = link, os.path.join(folder, 'snapshots'), every, max_snaps, ram, states, notes
        self.n = 0; self.next = 0.0; self.last_race = None; self.last_check = -1e9; self.log = log if log is not None else []; self.ram_size = ram_size; self.watch = watch; self.capped = False; self.last_watch = -1e9; self.last_w = None; os.makedirs(self.dir, exist_ok=True)
    def take(self, label, t=0.0):
        if self.n >= self.max:
            if not self.capped: self.capped = True; say(f'  (límite de {self.max} fotos alcanzado; sigo grabando sin más fotos)', f'  (limit of {self.max} snapshots reached; still recording, no more snapshots)')
            return
        self.n += 1; tag = f'snap_{self.n:03d}_' + ''.join(c if c.isalnum() or c in '-_' else '_' for c in label)[:30]; rec = dict(n=self.n, label=label, t=round(t, 2), files=[])
        say(f'  Foto {self.n} ({label})... el juego puede quedarse quieto unos segundos.', f'  Snapshot {self.n} ({label})... the game may freeze for a few seconds.')
        state_ok = False
        try:
            if self.states: p = os.path.join(self.dir, tag + '.p2s'); self.states.save(p); rec['files'].append(os.path.basename(p)); state_ok = True
        except Exception as e: say(f'  (savestate falló: {e}; se sigue solo con RAM)', f'  (savestate failed: {e}; continuing with RAM only)'); self.states = None
        try:                                                                          # el savestate ya trae la RAM (y es consistente): en fotos periódicas no se repite el volcado lento por PINE
            if self.ram and not (state_ok and (label == 'auto' or label.startswith('cambio_'))): p = os.path.join(self.dir, tag + '.ram.gz'); SN.ram_dump(self.link, p, self.ram_size); rec['files'].append(os.path.basename(p))
        except Exception as e: say(f'  (volcado de RAM falló: {e})', f'  (RAM dump failed: {e})')
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
        if self.watch and t - self.last_watch > 1.0:                                  # cambio de pantalla/nivel (p. ej. carga): foto
            self.last_watch = t; w = self.watch()
            if w and self.last_w is not None and w != self.last_w: self.last_w = w; self.take('cambio_' + w, t); return
            if w: self.last_w = w
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

def connect(a, now=time.monotonic, sleep=time.sleep):
    """Conecta con PCSX2 por PINE y ESPERA (sin que hagas nada) a que PCSX2 y el juego estén listos. -> (enlace, id del juego, título)."""
    import pinelink
    tcp = None
    if a.tcp: h, _, p = a.tcp.rpartition(':'); tcp = (h or '127.0.0.1', int(p))
    t0 = now(); shown = set()
    def hint(key, es, en):
        if key not in shown: shown.add(key); say(es, en)
    while True:
        try:
            link = pinelink.Link(path=a.socket, tcp=tcp, slot=a.pine_slot)
        except (FileNotFoundError, ConnectionRefusedError, OSError):
            hint('pine', 'Esperando a PCSX2... Ábrelo y activa PINE (Ajustes > Avanzado > Activar PINE, y reinicia PCSX2 una vez). Yo sigo esperando.',
                         'Waiting for PCSX2... Open it and enable PINE (Settings > Advanced > Enable PINE, then restart PCSX2 once). I keep waiting.')
        else:
            try:
                gid = (link.game_id() or '').strip()
                if gid: return link, gid, link.title()
            except (OSError, ConnectionError): pass
            hint('game', 'PCSX2 está abierto: arranca ahora Downhill Domination (disco PAL). Yo sigo esperando.', 'PCSX2 is open: now start Downhill Domination (PAL disc). I keep waiting.')
        if now() - t0 > a.wait: raise SystemExit(tr('No encontré PCSX2 con el juego a tiempo. Ábrelos y vuelve a lanzar este programa.', 'PCSX2 with the game was not found in time. Open them and run this program again.'))
        sleep(2)

def calibrate(link, riders, now=time.monotonic, budget_ms=12.0):
    """Mide cuánto cuesta leer una muestra y elige cuántos pilotos grabar para no frenar el juego. -> (pilotos, modo_ligero, ms medidos)."""
    ms = 0.0
    for r in dict.fromkeys((riders, min(riders, 3), 1)):
        wins = [(a, l) for _, a, l, k in S.region_table(r, False) if k == 0]; link.read_windows(wins); t0 = now()
        for _ in range(5): link.read_windows(wins)
        ms = 1000 * (now() - t0) / 5
        if ms <= budget_ms: return r, False, ms
    return 1, True, ms

class RaceWatch:
    """True cuando hay carrera de verdad durante 2 comprobaciones seguidas (separadas ~1 s). Sirve de `stop` del modo recorrido automático."""
    def __init__(self, link, now=time.monotonic): self.link, self.now, self.t, self.hits = link, now, -9.0, 0
    def __call__(self):
        t = self.now()
        if t - self.t >= 1.0:
            self.t = t
            try: self.hits = self.hits + 1 if in_race(self.link) else 0
            except Exception: self.hits = 0
        return self.hits >= 2

def load_progress(base):
    try: return json.load(open(os.path.join(base, 'progress.json'), encoding='utf-8'))
    except Exception: return []

def save_progress(base, prog):
    try: json.dump(prog, open(os.path.join(base, 'progress.json'), 'w', encoding='utf-8'), indent=1, ensure_ascii=False)
    except OSError: pass

def suggest(prog, levels, roster):
    """Qué jugar la próxima vez: niveles de la tabla del juego aún no grabados y pilotos aún no usados. -> (es, en) o None."""
    seen = {r.get('level') for r in prog}; used = {r.get('rider') for r in prog}
    todo = [l for l in levels if l not in seen and not l.endswith('2') and len(l) > 3][:3]; riders = [c for c, _ in roster if c not in used][:3]
    if not todo and not riders: return None
    return (f'Para la próxima: nivel {", ".join(todo) or "(el que quieras)"}' + (f' y piloto {", ".join(riders)}' if riders else '') + ' (aún sin grabar).',
            f'Next time: level {", ".join(todo) or "(any)"}' + (f' and rider {", ".join(riders)}' if riders else '') + ' (not recorded yet).')

def run_session(link, a, ctx, tour):
    """Una grabación completa (carrera o recorrido): espera, graba, fotos, comprueba y empaqueta. -> dict(ended, zip, folder, seconds, ok, discarded)."""
    base, send_dir, gid, title = ctx['base'], ctx['send'], ctx['gid'], ctx['title']
    riders = 0 if tour else wait_for_race(link, a.wait)
    if tour:
        try: riders = race_state(link)[0]
        except Exception: pass
    info = D.session_info(link, riders if 1 <= riders <= S.MAX_RIDERS else None); level = a.level or info['level']; rider = a.rider or info['player']
    cap = a.riders if a.riders else ctx['riders_auto']; n_rec = max(1, min(cap, riders if 1 <= riders <= S.MAX_RIDERS else 1))
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%SZ')
    tag = '_'.join(''.join(c if c.isalnum() or c in '-' else '_' for c in x)[:24] for x in (('tour', a.label) if tour else (level, rider, a.label)) if x)
    folder = os.path.join(base, 'sessions', f'dhtel_{stamp}' + (f'_{tag}' if tag else '')); os.makedirs(folder, exist_ok=True)
    if tour:
        say('\n== Menús y pantallas (no hace falta carrera). Recórrelas sin prisa, ~5 s en cada una; yo hago fotos solo. Pantallas que nos faltan: ==', '\n== Menus and screens (no race needed). Visit them calmly, ~5 s on each; I take snapshots by myself. Screens we are missing: ==')
        for es, en in D.TOUR_SCREENS: say(f'   - {es}', f'   - {en}')
    else:
        say(f'\n== Carrera detectada: nivel {level or "?"}, tu piloto {rider or "?"} ({riders} pilotos; grabo {n_rec}). Juega con normalidad. ==', f'\n== Race detected: level {level or "?"}, your rider {rider or "?"} ({riders} riders; recording {n_rec}). Play normally. ==')
        if not info['consistent']: say('   (no pude confirmar el nivel/piloto leyendo el juego; no pasa nada, lo resolvemos al analizar)', '   (could not confirm level/rider from the game; no problem, it is resolved at analysis time)')
    coach = None if tour or a.no_coach else D.Coach(); last = [0.0]
    if coach:
        say('Guion: el programa marca [OK] cada cosa que hagas; tú solo juega e intenta cubrirlas (en cualquier orden):', 'Script: the program ticks [OK] for each thing you do; just play and try to cover them (any order):')
        for k, _, es, en in D.ITEMS: say(f'   - {es}', f'   - {en}')
    def on_sample(t, regs, nodes):
        try: ev = coach.update(D.signals(regs, nodes[0] if nodes else None), t - last[0]); last[0] = t
        except Exception: return
        for k in ev: es, en = next((e, n) for kk, _, e, n in D.ITEMS if kk == k); say(f'  [OK] {es}', f'  [OK] {en}')
    def prog(st, t):
        say(f'  {t:5.0f} s  {st["samples"]} muestras ({st["samples"] / max(t, 1e-9):.0f}/s)', f'  {t:5.0f} s  {st["samples"]} samples ({st["samples"] / max(t, 1e-9):.0f}/s)')
        h = coach.hint() if coach else None
        if h and int(t) % 30 < 5: say(f'  Te falta: {h[1]}', f'  Still missing: {h[2]}')
    every = 0 if a.light or ctx['light'] else (a.snap_every if a.snap_every is not None else (15.0 if tour else 60.0))
    snaplog = []; snapper = Snapper(link, folder, every=every, max_snaps=ctx['max_snaps'], ram=not a.no_ram, states=ctx['saver'], notes=ctx['notes'], log=snaplog, ram_size=a.ram_size,
                                    watch=(lambda: D.session_info(link)['level'] or None) if tour else None)
    snapper.take('start', 0.0); snapper.next = every if every else 0.0; snapper.last_race = in_race(link)
    snapshot_static(link, os.path.join(folder, 'static.dhtel')); t_start = time.monotonic(); tick_path = os.path.join(folder, 'ticks.dhtel.gz')
    slow = not (a.light or ctx['light']); tab = S.region_table(n_rec, slow); stats = {}; ended = None
    stop = (lambda: False) if a.tour or not ctx['auto'] else RaceWatch(link)
    try:
        with S.open_out(tick_path) as f:
            S.write_header(f, tab)
            try: record(link, f, a.seconds, n_rec, progress=prog, require_race=not tour, min_interval=0.2 if tour else 0.0, on_tick=snapper.tick, slow=slow, on_sample=on_sample if coach else None,
                        stats=stats, stop=stop if tour else (lambda: False))
            except KeyboardInterrupt: stats['ended'] = 'ctrl-c'
    except (ConnectionError, IOError, OSError) as e:
        say(f'Se perdió la conexión con PCSX2 ({e}); se guarda lo grabado.', f'Lost the connection to PCSX2 ({e}); saving what was recorded.'); stats['ended'] = 'connection-lost'
    ended = stats.get('ended')
    try: snapper.take('end', time.monotonic() - t_start)
    except Exception: pass
    try: snapshot_static(link, os.path.join(folder, 'static_end.dhtel'))
    except Exception: pass
    try: _, samples = S.read_file(tick_path); n = len(samples); dur = samples[-1][0] if samples else 0.0
    except ValueError: n, dur = 0, 0.0
    if tour and not a.tour and dur < 10 and ended != 'ctrl-c':                      # tramo de menús demasiado corto (la carrera empezó enseguida): no vale la pena enviarlo
        shutil.rmtree(folder, ignore_errors=True); return dict(ended=ended, discarded=True, folder=None, zip=None, seconds=dur, ok=True)
    with open(os.path.join(folder, 'notes.jsonl'), 'w', encoding='utf-8') as f:
        for t, txt in ctx['notes'].items: f.write(json.dumps(dict(t=round(t - t_start, 2), note=txt), ensure_ascii=False) + '\n')
    nnotes = len(ctx['notes'].items); ctx['notes'].items.clear()
    meta = dict(schema=S.SCHEMA, tool_version=TOOL_VERSION, created_utc=stamp, mode='tour' if tour else 'race', level=level, rider=rider, label=a.label, game_id=gid, game_title=title, pcsx2_version=ctx['pcsx2'],
                detected=dict(level=info['level'], player=info['player'], roster=info['roster'], consistent=info['consistent'], user_override=bool(a.level or a.rider)), coverage=coach.coverage() if coach else None,
                riders_in_race=riders, riders_recorded=n_rec, riders_auto=ctx['riders_auto'], calibration_ms=round(ctx['calib_ms'], 2), light=not slow, platform=dict(system=platform.system(), release=platform.release(), machine=platform.machine(), python=platform.python_version()),
                samples=n, seconds=round(dur, 2), ended=ended, torn=stats.get('torn'), forced=stats.get('forced'), slow_samples=stats.get('slow'),
                mean_sample_ms=round(stats.get('mean_sample_ms', 0), 2), notes=nnotes, snapshots=snaplog, slow_period=tab.slow_period,
                regions=[dict(name=nm, addr=ad, len=ln, kind=k) for nm, ad, ln, k in tab], static=[dict(name=nm, addr=ad, len=ln) for nm, ad, ln in S.STATIC], ticks_sha256=sha256(tick_path))
    with open(os.path.join(folder, 'meta.json'), 'w', encoding='utf-8') as f: json.dump(meta, f, indent=2, ensure_ascii=False)
    say(f'\nListo: {n} muestras, {dur:.1f} s, {len(snaplog)} foto(s).', f'\nDone: {n} samples, {dur:.1f} s, {len(snaplog)} snapshot(s).')
    if stats.get('mean_sample_ms', 0) > 18 and not tour: say('Aviso: tu PC va justo leyendo tantos pilotos; la próxima vez lo ajusto solo (--riders 1 si sigue a tirones).', 'Note: your PC is tight reading that many riders; I will adjust next time (--riders 1 if it still stutters).')
    z = None
    if not a.no_zip: z = make_zip(folder, send_dir); say(f'>>> Guardado para enviar: {z} ({os.path.getsize(z) / 1e6:.1f} MB)', f'>>> Saved to send: {z} ({os.path.getsize(z) / 1e6:.1f} MB)')
    ok = C.main([folder]) == 0
    if coach and n > 0:
        miss = [es for _, es, _ in coach.pending()]
        if miss: say('Cosas del guion que aún no salieron (opcional, para otra vez): ' + '; '.join(miss), 'Script items not seen yet (optional, next time): ' + '; '.join(en for _, _, en in coach.pending()))
        ctx['progress'].append(dict(level=level, rider=rider, utc=stamp, seconds=round(dur, 1), done=sorted(coach.done))); save_progress(base, ctx['progress'])
        sg = suggest(ctx['progress'], ctx['levels'], info['roster'])
        if sg: say(*sg)
    return dict(ended=ended, zip=z, folder=folder, seconds=dur, ok=ok, discarded=False)

def main(argv=None):
    if sys.version_info < (3, 8): raise SystemExit('Necesitas Python 3.8 o superior / You need Python 3.8 or newer: https://www.python.org/downloads/')
    try: sys.stdout.reconfigure(errors='replace')
    except Exception: pass
    ap = argparse.ArgumentParser(description='Telemetría y fotos completas de Downhill Domination vía PINE; sin argumentos graba solo todo / Telemetry and full snapshots via PINE; with no arguments it records everything by itself')
    ap.add_argument('--out', default=os.path.join(os.path.expanduser('~'), 'dh-telemetry'), help='carpeta base de salida (fuera del repo) / base output folder (outside the repo)')
    ap.add_argument('--level', default='', help='(opcional, se detecta solo) nivel / (optional, auto-detected) level'); ap.add_argument('--rider', default='', help='(opcional, se detecta solo) piloto / (optional, auto-detected) rider')
    ap.add_argument('--label', default='', help='texto libre / free text'); ap.add_argument('--seconds', type=float, default=0, help='duración máxima de UNA grabación (0 = automático); con valor graba una sola vez / max duration of ONE recording (0 = automatic); with a value records once')
    ap.add_argument('--tour', action='store_true', help='solo menús y pantallas, sin esperar carrera / menus and screens only, no race needed')
    ap.add_argument('--once', action='store_true', help='una sola carrera y salir (por defecto graba todo lo que juegues hasta Ctrl+C) / one race then exit (by default records everything you play until Ctrl+C)')
    ap.add_argument('--no-coach', action='store_true', help='sin el guion en vivo / no live script')
    ap.add_argument('--riders', type=int, default=0, help='pilotos a grabar; por defecto se elige solo según tu PC / riders to record; auto by default according to your PC')
    ap.add_argument('--light', action='store_true', help='sin canal lento ni fotos periódicas (PC lento; se activa solo si hace falta) / no slow channel nor periodic snapshots (slow PC; auto-enabled when needed)')
    ap.add_argument('--snap-every', type=float, default=None, help='segundos entre fotos automáticas (0 = ninguna; por defecto 60, 30 en menús) / seconds between automatic snapshots')
    ap.add_argument('--max-snaps', type=int, default=40); ap.add_argument('--no-ram', action='store_true', help='sin volcados de RAM / no RAM dumps')
    ap.add_argument('--no-states', action='store_true', help='no usar savestates de PCSX2 (solo RAM) / do not use PCSX2 savestates (RAM only)')
    ap.add_argument('--state-slot', type=int, default=10, help='ranura de savestate que se usa (se respalda y se restaura la tuya) / savestate slot used (yours is backed up and restored)')
    ap.add_argument('--sstates', help='carpeta de savestates de PCSX2 si no se encuentra sola / PCSX2 savestate folder if not auto-detected')
    ap.add_argument('--tcp', help='HOST:PUERTO (Windows: 127.0.0.1:28011) / HOST:PORT'); ap.add_argument('--socket', help='ruta del socket Unix de PINE / PINE Unix socket path')
    ap.add_argument('--pine-slot', type=int, default=28011, help='ranura PINE / PINE slot'); ap.add_argument('--wait', type=float, default=900, help='segundos esperando a PCSX2 / una carrera / seconds to wait for PCSX2 / a race')
    ap.add_argument('--ram-size', type=lambda x: int(x, 0), default=SN.EE_RAM, help=argparse.SUPPRESS)       # pruebas
    ap.add_argument('--force', action='store_true', help='no comprobar el ID del juego / skip the game ID check'); ap.add_argument('--no-zip', action='store_true'); ap.add_argument('--no-notes', action='store_true')
    a = ap.parse_args(argv)
    base = os.path.abspath(a.out)
    if in_git_repo(base): raise SystemExit(tr('La carpeta de salida está dentro de un repositorio git; elige otra (p. ej. ~/dh-telemetry). Estos datos NO deben acabar en el repo.',
                                            'The output folder is inside a git repository; pick another (e.g. ~/dh-telemetry). This data must NOT end up in the repo.'))
    os.makedirs(base, exist_ok=True); free = shutil.disk_usage(base).free; max_snaps = a.max_snaps
    if free < 0.5e9: raise SystemExit(tr(f'Casi no queda disco libre ({free / 1e9:.1f} GB) en {base}. Libera al menos 2 GB o usa --out OTRA_CARPETA.', f'Almost no free disk ({free / 1e9:.1f} GB) in {base}. Free at least 2 GB or use --out ANOTHER_FOLDER.'))
    if free < 3e9: max_snaps = min(max_snaps, 8); say(f'Aviso: queda poco disco ({free / 1e9:.1f} GB); limito las fotos a {max_snaps} por grabación.', f'Note: low disk space ({free / 1e9:.1f} GB); limiting snapshots to {max_snaps} per recording.')
    say('Downhill Domination - grabador de telemetría. Solo tienes que jugar; yo detecto el nivel y el piloto, te guío y empaqueto todo.', 'Downhill Domination telemetry recorder. Just play; I detect the level and rider, guide you and pack everything.')
    link, gid, title = connect(a); say(f'Conectado: {link.where}', f'Connected: {link.where}')
    saver = None
    try:
        check_game(link, a.force); say(f'Juego: {gid} {title}', f'Game: {gid} {title}')
        try: pcsx2 = link.version()
        except Exception: pcsx2 = '?'
        send_dir = os.path.join(base, 'SEND'); os.makedirs(send_dir, exist_ok=True)
        with open(os.path.join(send_dir, 'LEEME_PRIMERO_README_FIRST.txt'), 'w', encoding='utf-8') as f: f.write(SEND_README)
        riders_auto, light, calib_ms = calibrate(link, S.MAX_RIDERS)
        if light: say('Tu PC va justo: grabo en modo ligero para no frenar el juego.', 'Your PC is tight: recording in light mode so the game is not slowed down.')
        if not a.no_states:
            dirs = [a.sstates] if a.sstates else SN.sstates_dirs(); found = [d for d in dirs if d and os.path.isdir(d)]
            if found:
                try: saver = SN.StateSaver(link, gid, found[0], a.state_slot); say(f'Savestates: {found[0]} (ranura {a.state_slot}; tu contenido se respalda y se restaura)', f'Savestates: {found[0]} (slot {a.state_slot}; your content is backed up and restored)')
                except Exception as e: say(f'Savestates desactivados: {e}', f'Savestates disabled: {e}')
            else: say('No encuentro la carpeta de savestates de PCSX2; solo volcados de RAM (usa --sstates RUTA para activarlos).', 'PCSX2 savestate folder not found; RAM dumps only (use --sstates PATH to enable).')
        notes = Notes(not a.no_notes); single = bool(a.seconds) or a.once
        ctx = dict(base=base, send=send_dir, gid=gid, title=title, pcsx2=pcsx2, notes=notes, saver=saver, riders_auto=riders_auto, light=light, calib_ms=calib_ms, max_snaps=max_snaps, auto=not single and not a.tour,
                   progress=load_progress(base), levels=D.level_table(link))
        say('Termina con Ctrl+C cuando quieras (guardo y empaqueto todo). Opcional: escribe "snap nombre" + Enter para una foto completa.', 'Stop with Ctrl+C any time (I save and pack everything). Optional: type "snap name" + Enter for a full snapshot.')
        results = []
        try:
            if a.tour or single: results.append(run_session(link, a, ctx, a.tour))
            else:
                while True:                                              # modo automático: menús -> carrera -> menús -> carrera...
                    r = run_session(link, a, ctx, tour=not in_race(link)); results.append(r)
                    if r['ended'] in ('ctrl-c', 'connection-lost'): break
        except KeyboardInterrupt: pass
        except (ConnectionError, OSError) as e: say(f'Se cerró PCSX2 o se perdió la conexión ({e}). Guardo lo que hay.', f'PCSX2 closed or the connection was lost ({e}). Saving what exists.')
        zips = [r['zip'] for r in results if r.get('zip')]
        if len(zips) > 1:
            import bundle; bundle.main(['--out', base])
        elif zips: say(f'\n>>> ENVÍA ESTE ARCHIVO: {zips[0]}', f'\n>>> SEND THIS FILE: {zips[0]}')
        else: say('\nNo se guardó ninguna grabación.', '\nNo recording was saved.')
    finally:
        if saver: saver.restore()
        try: link.close()
        except Exception: pass
    return 0

if __name__ == '__main__':
    try: sys.exit(main())
    except KeyboardInterrupt: sys.exit(130)
