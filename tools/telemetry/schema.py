# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Esquema y formato de archivo de la telemetría (DHTEL1). Solo biblioteca estándar. / Telemetry schema and file format (DHTEL1). Standard library only.

Qué se graba (ver README.md): ventanas de la RAM del juego del piloto del jugador (entradas, controlador de conducción, banderas, módulo de impactos), versiones reducidas de los demás pilotos,
el gestor de carrera (puertas, tiempos) y unas globales. NO se graba el teclado, ni capturas de pantalla, ni nada de tu sistema. Direcciones de SLES-52202 (Downhill Domination, PAL).
Cada ventana tiene longitud múltiplo de 8 (Read64 de PINE).

Archivo ticks.dhtel(.gz):  'DHTEL1\\0\\0' | u32 nregiones | u32 periodo_lento | por región: u32 dirección, u32 longitud, 24 B nombre ASCII, u8 tipo, 3 B relleno
                           | por muestra: f64 tiempo(s), u32 banderas, u32 longitud del bloque, bloque = regiones tipo 0 (cada muestra), tipo 1 (nodos, dirección dinámica) y,
                             si banderas & 2, tipo 2 (canal lento: estructuras grandes, una de cada `periodo_lento` muestras).
banderas: 1 = muestra forzada por tiempo (la moto no cambió), 2 = trae el canal lento. Si el nombre acaba en .gz el archivo va comprimido con gzip.
"""
import gzip, struct

MAGIC = b'DHTEL1\0\0'
SCHEMA = 3
GAME_IDS = ('SLES-52202',)          # única versión soportada (las direcciones son de ese ejecutable)

RIDER_BASE, RIDER_STRIDE, RIDER_COUNT_ADDR = 0x2D94C0, 0x7BD0, 0x329668     # DAT_002d94c0 + i*0x7bd0; DAT_00329668 = nº de pilotos
MAX_RIDERS = 10
RB = RIDER_BASE                     # piloto 0 = el jugador
CTRL = RB + 0x40C0                  # cuerpo de conducción del jugador (FUN_00136330)
RACE_MGR = 0x4691D0                 # gestor de carrera (FUN_001A33D0/001A3480): planos de puertas (+0x230), contadores

def rb(i): return RIDER_BASE + i * RIDER_STRIDE

# Jugador (piloto 0): (nombre, dirección, longitud)
PLAYER = [
    ('rider_input', RB + 0x1180, 0x1F0),    # palabras de entrada: giro +0x1190/94, cabeceo +0x1198/9C, botones +0x11B8..., (FUN_0012DFF0/0012E110/0012E1B0)
    ('rider_sel',   RB + 0x13D0, 0x30),     # bytes que eligen la fuente de entrada (+0x13D7..)
    ('rider_flags', RB + 0x3F40, 0x40),     # +0x3F58, +0x3F60: factores del piloto
    ('rider_state', RB + 0x4090, 0x20),     # +0x4098: banderas (bit 2 = sin fuerzas de conducción, bit 3 = cuerpo)
    ('ctrl',        CTRL,        0x720),    # controlador de conducción completo: P, velocidad, fuerzas, estados de giro/cabeceo, parámetros
    ('rider_ctrlst', RB + 0x4EC8, 0x20),    # +0x4ECC puntero de estado
    ('jump_obj',    RB + 0x5E40, 0x80),     # objeto de carga del salto (FUN_0012E168 -> FUN_00176160)
    ('rider_misc',  RB + 0x7900, 0x300),    # +0x791C, +0x7914, +0x7A5C, +0x7A69/6A/70/78, +0x7AF0, +0x7B00, +0x7BA8..B1
    ('physmod',     RB + 0x6420, 0x1E0),    # módulo de cuerpo rígido (impactos)
    ('node_ptr',    RB + 0x7928, 8),        # puntero al nodo (matriz de orientación) del cuerpo
]
GLOBAL = [
    ('globals',     0x77A7D0, 0x40),        # G0 = 0x77A7D8 y vecinos
    ('rider_count', RIDER_COUNT_ADDR, 8),
    ('race_mgr',    RACE_MGR, 0x530),       # gestor de carrera: contadores y planos de puertas
    ('hits',        0x2DD600, 0x800),       # pila de la física con los registros de impacto del paso
]
NODE_LEN = 0x80                    # matriz 4x4 en node+0x10 ... y vecinos

def lite(i):
    """Otros pilotos (IA o rivales): versión reducida (el estado de conducción y las entradas que el juego les calcula)."""
    b = rb(i); p = f'r{i}_'
    return [(p + 'ctrl', b + 0x40C0, 0x520), (p + 'input', b + 0x1180, 0x100), (p + 'misc', b + 0x7900, 0x100), (p + 'state', b + 0x4090, 0x20), (p + 'node_ptr', b + 0x7928, 8)]

# Instantánea al empezar y al terminar (constantes, tablas y el piloto 0 entero)
STATIC = [
    ('surf_table', 0x77A3D8, 0x400),        # 32 clases de superficie x 8 floats
    ('consts_a',   0x2C5300, 0x180),        # constantes de FUN_00136A88/00136330/00137C48 (gp-relativas y DAT_002c53xx/54xx)
    ('consts_b',   0x2C5A00, 0x40),         # DAT_002c5a08 (bandera de giro de la velocidad)
    ('consts_c',   0x45F800, 0x20),         # DAT_0045f808
    ('consts_d',   0x2C85F0, 0x40),         # X = 50.0 (0x2C8624), dt (0x2C85FC)
    ('rider0_full', RB, RIDER_STRIDE),      # el piloto 0 entero (31 KB): parámetros de la moto, animación, etc.
    ('race_mgr_wide', RACE_MGR, 0x1000),
]

SYNC = (CTRL + 0x50, 0x40)         # ventana cuyo cambio marca un nuevo paso de física (posición y momento del cuerpo)
HDR_REGION = struct.Struct('<II24sB3x')   # dirección, longitud, nombre, tipo (0 cada muestra, 1 nodo dinámico, 2 canal lento)
SLOW_PERIOD = 25                           # canal lento cada 25 muestras (~2 Hz a 50/s)

class Table(list):
    slow_period = 0

def region_table(n_riders=1, slow=True):
    """Tabla completa por muestra, en orden de archivo: (nombre, dirección o 0 si dinámica, longitud, tipo). n_riders incluye al jugador.
    tipo 0 = cada muestra, 1 = nodo (dirección leída del puntero), 2 = canal lento (todos los pilotos enteros y el gestor de carrera ancho)."""
    t = Table((n, a, l, 0) for n, a, l in PLAYER + GLOBAL)
    for i in range(1, max(1, n_riders)): t += [(n, a, l, 0) for n, a, l in lite(i)]
    t.append(('node', 0, NODE_LEN, 1))
    for i in range(1, max(1, n_riders)): t.append((f'r{i}_node', 0, NODE_LEN, 1))
    if slow:
        for i in range(max(1, n_riders)): t.append((f'full_r{i}', rb(i), RIDER_STRIDE, 2))
        t.append(('race_mgr_wide', RACE_MGR, 0x1000, 2))
    t.slow_period = SLOW_PERIOD if slow else 0
    return t

def node_ptr_name(name): return 'node_ptr' if name == 'node' else name[:-5] + '_node_ptr'    # 'r3_node' -> 'r3_node_ptr'

def write_header(f, table=None):
    t = table or region_table(); f.write(MAGIC + struct.pack('<II', len(t), getattr(t, 'slow_period', 0)))
    for n, a, l, d in t: f.write(HDR_REGION.pack(a, l, n.encode('ascii'), d))

def sample_len(table=None, slow=False):
    """Longitud del bloque de una muestra: tipos 0 y 1, más el tipo 2 si slow."""
    return sum(l for _, _, l, k in (table or region_table()) if k < 2 or slow)

def write_sample(f, t, flags, blob):
    f.write(struct.pack('<dII', t, flags, len(blob)) + blob)

def open_out(path):
    return gzip.open(path, 'wb', compresslevel=3) if path.endswith('.gz') else open(path, 'wb')

def read_file(path):
    """-> (tabla, [(t, flags, {nombre: bytes})]). Acepta .gz. Lanza ValueError si el archivo no es válido. Un archivo cortado (cierre brusco) devuelve las muestras completas que haya."""
    try:
        d = (gzip.open(path, 'rb') if path.endswith('.gz') else open(path, 'rb')).read()
    except (EOFError, OSError, gzip.BadGzipFile) as e:
        d = _salvage(path)
        if not d: raise ValueError(f'no se puede leer {path}: {e}')
    if d[:8] != MAGIC: raise ValueError('no es un archivo DHTEL1')
    n, sp = struct.unpack_from('<II', d, 8)
    if not 1 <= n <= 512: raise ValueError('tabla de regiones inválida')
    p = 16; table = Table(); table.slow_period = sp
    for _ in range(n):
        a, l, nm, k = HDR_REGION.unpack_from(d, p); p += HDR_REGION.size; table.append((nm.rstrip(b'\0').decode('ascii', 'replace'), a, l, k))
    fast, full = sample_len(table), sample_len(table, True); out = []
    while p + 16 <= len(d):
        t, fl, ln = struct.unpack_from('<dII', d, p)
        if ln != (full if fl & 2 else fast) or p + 16 + ln > len(d): break       # última muestra incompleta: se ignora
        p += 16; q = p; parts = {}
        for nm, _, l, k in table:
            if k == 2 and not fl & 2: continue
            parts[nm] = d[q:q + l]; q += l
        out.append((t, fl, parts)); p += ln
    if not out and len(d) > p + 16: raise ValueError('muestras de tamaño inesperado')
    return table, out

def _salvage(path):
    """Lee un .gz truncado trozo a trozo hasta donde llegue."""
    import zlib
    raw = open(path, 'rb').read(); d = zlib.decompressobj(31); out = bytearray()
    try:
        for i in range(0, len(raw), 65536): out += d.decompress(raw[i:i + 65536])
    except zlib.error: pass
    return bytes(out)

def f32(b, o): return struct.unpack_from('<f', b, o)[0]
def u32(b, o): return struct.unpack_from('<I', b, o)[0]
