# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Autodetección (nivel, piloto, lista de niveles) y entrenador en vivo. Solo biblioteca estándar. / Auto-detection (level, rider, level list) and live coach. Standard library only.

Direcciones de SLES-52202 (PAL) halladas en 10 savestates (2 niveles, 6 y 10 pilotos); el nº de entradas de la lista coincide con DAT_00329668 en los 10. **Hipótesis** hasta validarlas en más niveles:
  0x4FB0CC  nombre del nivel cargado, ASCII ('ALPINE', 'ALPINEMX'); le sigue la ruta '\\LVL\\<NIVEL>.NGP;1'. Es la carpeta/área: variantes como ALP2 (2.º recorrido de ALPINE) comparten nombre.
  0x7684A0  lista de pilotos de la carrera: nombre del nivel (12 B) y por piloto un código 'xNOS' + modelos de moto ('GTIDRV4', 'TRKLQ104', 'ROXPS100', ...). La 1.ª entrada = el jugador (hipótesis).
  0x2C5BF0  tabla de niveles del juego (cadenas ASCII de 8 B de paso: ALPINE, MOAB, JUNGLE, ..., sufijos 2 al final)."""
import collections, math, re, struct

LEVEL_WIN = (0x4FB0C8, 0x48)
ROSTER_WIN = (0x7684A0, 0x400)
LEVELS_WIN = (0x2C5BF0, 0x200)
_NAME = re.compile(rb'^[A-Z][A-Z0-9]{1,11}$')
_CODE = re.compile(rb'^[A-Z]NOS$')

def _tokens(b): return [t for t in b.split(b'\0') if t]

def parse_level(win):
    """Ventana LEVEL_WIN -> nombre del nivel o ''."""
    m = re.search(rb'\\LVL\\([A-Z0-9]{2,12})\.NGP', win)
    if m: return m.group(1).decode()
    t = win[4:16].split(b'\0')[0]
    return t.decode() if _NAME.match(t) else ''

def parse_roster(win):
    """Ventana ROSTER_WIN -> (nombre, [(código, [modelos])]). El nombre del nivel sale del primer token."""
    toks = _tokens(win)
    if not toks or not _NAME.match(toks[0]): return '', []
    out = []
    for t in toks[1:]:
        if _CODE.match(t): out.append((t.decode(), []))
        elif out and re.match(rb'^[A-Z][A-Z0-9]{2,11}$', t) and len(out[-1][1]) < 6: out[-1][1].append(t.decode())
        elif out and not re.match(rb'^[ -~]+$', t): break              # basura binaria: fin de la lista
    return toks[0].decode(), out

def parse_levels(win):
    """Tabla de niveles -> lista de nombres en orden (hasta la primera cadena de 3 letras = tabla de prefijos)."""
    out = []
    for t in _tokens(win):
        if not _NAME.match(t): continue
        if len(t) <= 3: break
        out.append(t.decode())
    return out

def session_info(link, n_riders=None):
    """Lee del juego (PINE) qué nivel y qué piloto hay. -> dict(level, path_ok, player, roster, consistent). Nunca lanza por datos raros: devuelve vacío."""
    try: lv, ro = link.read_windows([LEVEL_WIN, ROSTER_WIN])
    except Exception: return dict(level='', player='', roster=[], consistent=False)
    level = parse_level(lv); rname, roster = parse_roster(ro)
    ok = bool(level) and rname == level and bool(roster) and (n_riders is None or len(roster) == n_riders)
    return dict(level=level, player=roster[0][0] if roster else '', roster=roster, consistent=ok)

def level_table(link):
    try: return parse_levels(link.read_windows([LEVELS_WIN])[0])
    except Exception: return []

# ---------------------------------------------------------------- señales y entrenador
def signals(regs, node=None):
    """Muestra (regiones por nombre) -> dict de señales. Offsets de schema.py / check.py (entradas: rider+0x1180: +0x10 der., +0x14 izq., +0x18/+0x1C cabeceo, +0x38 botón; pedal rider+0x7900+0x6A)."""
    c, ri, rm = regs['ctrl'], regs['rider_input'], regs['rider_misc']
    vx, vy, vz = struct.unpack_from('<3f', c, 0xC0); v = math.sqrt(vx * vx + vy * vy + vz * vz)
    i = lambda o: struct.unpack_from('<i', ri, o)[0] != 0
    pos = struct.unpack_from('<3f', node, 0x10) if node and len(node) >= 0x1C else None
    return dict(speed=v if math.isfinite(v) else 0.0, air=struct.unpack_from('<h', c, 0x4A4)[0] >= 2, surf=struct.unpack_from('<H', c, 0x396)[0] & 0x1F,
                right=i(0x10), left=i(0x14), up=i(0x18), down=i(0x1C), btn=i(0x38), pedal=rm[0x6A] != 0, pos=pos)

ITEMS = [   # clave, objetivo, texto es, texto en
    ('pedal', 10.0, 'Pedalea a tope en línea recta durante 10 s', 'Pedal flat out in a straight line for 10 s'),
    ('right', 3.0, 'Gira a la derecha a fondo (3 s en total)', 'Steer hard right (3 s in total)'),
    ('left', 3.0, 'Gira a la izquierda a fondo (3 s en total)', 'Steer hard left (3 s in total)'),
    ('lean', 4, 'Inclínate hacia delante y hacia atrás varias veces (también en el aire)', 'Lean forward and back several times (also in the air)'),
    ('brake', 2, 'Frena dos veces (una frenada corta y otra larga)', 'Brake twice (one short, one long)'),
    ('jump_short', 1, 'Salta con una pulsación corta del botón de salto', 'Jump with a short tap of the jump button'),
    ('jump_long', 1, 'Salta con carga larga: mantén el botón de salto 1 s y suéltalo', 'Jump with a long charge: hold the jump button for 1 s and release'),
    ('air', 1, 'Consigue un vuelo largo (más de 1 s en el aire): rampa grande o barranco', 'Get a long flight (over 1 s in the air): big ramp or drop'),
    ('surfaces', 3, 'Pasa por 3 tipos de terreno distintos (tierra, hierba, roca, nieve, puente...)', 'Ride over 3 different ground types (dirt, grass, rock, snow, bridge...)'),
    ('crash', 1, 'Estréllate una vez a propósito (árbol, roca o caída) y deja que reaparezca', 'Crash once on purpose (tree, rock or fall) and let it respawn'),
    ('time', 180.0, 'Aguanta al menos 3 minutos de carrera, o llega a la meta', 'Stay in the race at least 3 minutes, or reach the finish'),
]
KEYS = [k for k, _, _, _ in ITEMS]

class Coach:
    """Ve las señales del jugador y marca qué partes del guion ya están cubiertas. update() -> lista de claves completadas ahora."""
    def __init__(self):
        self.acc = collections.OrderedDict((k, 0.0) for k in KEYS); self.done = set(); self.surf = set(); self.hist = collections.deque(); self.t_btn = None
        self.air_run = 0.0; self.air_counted = False; self.prev = {}; self.t_brake = -9.0; self.t_crash = -9.0; self.t = 0.0
    def _add(self, k, n=1.0):
        self.acc[k] += n; need = next(o for kk, o, _, _ in ITEMS if kk == k)
        if self.acc[k] >= need and k not in self.done: self.done.add(k); self.new.append(k)
    def update(self, sig, dt):
        self.new = []; dt = min(max(dt, 0.0), 0.2); self.t += dt; t = self.t; p = self.prev; v = sig['speed']
        if sig['pedal'] and v > 15 and not (sig['right'] or sig['left']) and not sig['air']: self._add('pedal', dt)
        if sig['right']: self._add('right', dt)
        if sig['left']: self._add('left', dt)
        for k in ('up', 'down'):
            if sig[k] and not p.get(k): self._add('lean')
        if sig['btn'] and not p.get('btn'): self.t_btn = t
        if not sig['btn'] and p.get('btn') and self.t_btn is not None:
            held = t - self.t_btn; self.t_btn = None
            if held < 0.4: self._add('jump_short')
            elif held >= 0.8: self._add('jump_long')
        if sig['air']:
            self.air_run += dt
            if self.air_run >= 1.0 and not self.air_counted: self.air_counted = True; self._add('air')
        else: self.air_run = 0.0; self.air_counted = False; self.surf.add(sig['surf']); self.acc['surfaces'] = float(len(self.surf)); self._add('surfaces', 0.0)
        self.hist.append((t, v))
        while self.hist and t - self.hist[0][0] > 1.0: self.hist.popleft()
        top = max((s for _, s in self.hist), default=0.0)
        if self.hist and t - self.hist[0][0] > 0.3 and top >= 25 and v <= 0.3 * top and t - self.t_crash > 3.0: self.t_crash = t; self._add('crash')
        elif top - v >= 15 and v >= 3 and not sig['air'] and t - self.t_brake > 2.0 and t - self.t_crash > 1.5: self.t_brake = t; self._add('brake')
        if sig['pos'] and p.get('pos') and math.dist(sig['pos'], p['pos']) > 60 and t - self.t_crash > 3.0: self.t_crash = t; self._add('crash')       # salto de posición = reaparición
        self._add('time', dt); self.prev = dict(sig, pos=sig['pos'])
        return self.new
    def pending(self): return [(k, es, en) for k, _, es, en in ITEMS if k not in self.done]
    def coverage(self): return {k: dict(done=k in self.done, value=round(self.acc[k], 1)) for k in KEYS}
    def hint(self):
        p = self.pending(); return p[0] if p else None

TOUR_SCREENS = [   # lo que el proyecto aún no tiene de las pantallas del juego (docs/formats/game-layer.md, ui-reference.md)
    ('Menú principal y sus submenús (opciones, vídeo, control: "SELECT CONTROL OPTIONS", idioma)', 'Main menu and its submenus (options, video, controls: "SELECT CONTROL OPTIONS", language)'),
    ('Taller de bicis y selección de piloto', 'Bike shop and rider selection'),
    ('Selección de modo (carrera, freestyle, slalom dual, freeride, contrarreloj, combate, super salto) y de nivel', 'Mode selection (race, freestyle, dual slalom, freeride, time trial, combat, super jump) and level selection'),
    ('PANTALLA DE CARGA de varios niveles distintos (elige un nivel y espera sin tocar nada)', 'LOADING SCREEN of several different levels (pick a level and wait without touching anything)'),
    ('Cuenta atrás de salida y la primera pantalla de carrera', 'Start countdown and the first race screen'),
    ('Menú de pausa en carrera, cada una de sus pestañas (póster, música, cámara, reinicio)', 'In-race pause menu, each of its tabs (poster, music, camera, restart)'),
    ('Pantalla de resultados y cartel de meta, de cada modo que puedas', 'Results screen and finish banner, of every mode you can'),
    ('Repetición (replay), con su indicador, y créditos', 'Replay, with its indicator, and credits'),
    ('Si puedes: repite el HUD en otro idioma (Opciones > idioma)', 'If you can: repeat the HUD in another language (Options > language)'),
]
