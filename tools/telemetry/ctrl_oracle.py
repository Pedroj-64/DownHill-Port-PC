# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Oráculo del controlador de giro y cabeceo (ctrl+0x248 / +0x240) con telemetría DHTEL1: pares de ticks consecutivos -> residuo del modelo. Solo biblioteca estándar. / Steering and pitch controller oracle on DHTEL1 telemetry: consecutive tick pairs -> model residual. Standard library only.

Uso / Usage:  [AI=1] python3 tools/telemetry/ctrl_oracle.py <carpeta o ticks.dhtel[.gz]> [...]
Modelo = FUN_00137C48 (guiñada) y FUN_00137E68 (cabeceo), leídos de la decompilación y comprobados contra los ticks (docs/telemetry-awiiwa.md):
  u = entrada (rider_input +0x10 der. - +0x14 izq.): analógico/128 o ±1 si es digital; suelo: acc = ctrl+0x43C * rider+0x7918 (0.75) * tabla_superficie[6]/100, tope ctrl+0x438 * 0.75;
  aire: acc = ctrl+0x45C * ctrl+0x49C, tope ctrl+0x458 * ctrl+0x49C. Nuevo = yaw + acc*u; si |nuevo| > tope -> tope*u. Sin entrada: suelo -> 0, aire -> yaw*0.675 (a 0 si < 0.00014544411).
  ctrl+0x49C = 2.8 con el truco (izquierda+derecha mantenidas al cargar el salto, jump_obj+0x28 = 0x01010000); vuelve a 1.0 en cuanto no hay entrada de giro.
Un par vale si |ΔP| = 1 paso físico (0.02 s). Sale el % de pares que el modelo predice a <= TOL."""
import math, os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import schema as S

TOL = 2e-6                    # rad/tick (yaw es float32)
SNAP = 0.00014544411          # |valor| < SNAP -> 0 (constante literal de FUN_00137C48/00137E68, 1/120 de grado en rad... 0.00833°)
GROUND_K = 0.75               # rider+0x7918 (un float del piloto, constante en las grabaciones)
PAR = dict(gacc=0.008726646, gmax=0.033161256, aacc=0.005235988, amax=0.046076693, adec=0.675)   # ctrl+0x43C, 0x438, 0x45C, 0x458, 0x46C
PITCH = dict(acc=0.008726646, max=0.046076693, dec=0.8)                                              # ctrl+0x454, 0x450, 0x470

def _sample_stream(path):
    if os.path.isdir(path):
        for r, _, fs in os.walk(path):
            for f in fs:
                if f.startswith('ticks.dhtel'): path = os.path.join(r, f)
    return S.read_file(path)[1]

def surf_col6(path):
    """Columna 6 de la tabla de superficies (agarre de giro) desde static.dhtel de la grabación, o 100 para todas si no está."""
    d = os.path.dirname(path) if not os.path.isdir(path) else path
    for r, _, fs in os.walk(d):
        if 'static.dhtel' in fs:
            try: s = S.read_file(os.path.join(r, 'static.dhtel'))[1][0][2]['surf_table']; return [struct.unpack_from('<f', s, i * 32 + 24)[0] for i in range(32)]
            except Exception: pass
    return [100.0] * 32

def drive(path, rider=0):
    """Ticks del piloto `rider` (0 = jugador, 1..9 = IA): (yaw, inp, aire, un_paso, cabeceo, lean, k49c, superficie)."""
    out = []; prev = None; col6 = surf_col6(path)
    for _, _, p in _sample_stream(path):
        pre = f'r{rider}_' if rider else ''; c = p[pre + 'ctrl']; ri = p[pre + 'input'] if rider else p['rider_input']
        yaw = S.f32(c, 0x248); air = struct.unpack_from('<h', c, 0x4A4)[0]
        P = struct.unpack_from('<3f', c, 0x50); v = math.sqrt(sum(x * x for x in struct.unpack_from('<3f', c, 0xC0)))
        inp = struct.unpack_from('<i', ri, 0x10)[0] - struct.unpack_from('<i', ri, 0x14)[0]
        lean = (struct.unpack_from('<i', ri, 0x1C)[0] != 0) - (struct.unpack_from('<i', ri, 0x18)[0] != 0)      # +0x1C (abajo) +1, +0x18 (arriba) -1
        one = prev is not None and v > 1e-3 and abs(math.dist(P, prev) / v - 0.02) < 0.004
        grip = col6[struct.unpack_from('<H', c, 0x206)[0] & 0x1F]
        out.append((yaw, inp, air, one, S.f32(c, 0x240), lean, S.f32(c, 0x49C), grip)); prev = P
    return out

def u_of(inp):
    """Entrada normalizada: analógico inp/128 (inp=±127), digital (±1) = ±1."""
    return 0.0 if inp == 0 else float(inp) if abs(inp) == 1 else inp / 128

def _step(x, acc, mx, u, dec):
    """Regla común de FUN_00137C48/00137E68: sin entrada decae (a 0 bajo SNAP); con entrada suma acc*u y, si pasa el tope, vale tope*u."""
    if u == 0:
        x *= dec; return 0.0 if abs(x) < SNAP else x
    x += acc * u
    return mx * u if abs(x) > mx else x

def model(yaw, inp, ground, par, k49c=1.0, grip=100.0):
    u = u_of(inp)
    if ground: return _step(yaw, par['gacc'] * GROUND_K * grip * 0.01, par['gmax'] * GROUND_K, u, 0.0)
    return _step(yaw, par['aacc'] * k49c, par['amax'] * k49c, u, par['adec'])

def pitch_model(p, lean, air, k498=1.0):
    """Cabeceo: en suelo 0; en aire cualquier pulsación vale +-1 (FUN_0012E110 devuelve el signo)."""
    if not air: return 0.0
    return _step(p, PITCH['acc'] * k498, PITCH['max'] * k498, float(lean), PITCH['dec'])

def _pairs(t):
    for a, b in zip(t, t[1:]):
        if not b[3]: continue
        ground = a[2] == 0 and b[2] == 0; air = a[2] >= 2 and b[2] >= 2
        if ground or air: yield a, b, ground

def check_pitch(path, rider=0):
    ok = n = 0
    for a, b, ground in _pairs(drive(path, rider)):
        n += 1; ok += abs(pitch_model(a[4], b[5], not ground) - b[4]) <= TOL
    return ok, n

def check(path, rider=0):
    res = {'suelo': [0, 0], 'aire': [0, 0]}
    for a, b, ground in _pairs(drive(path, rider)):
        k = 'suelo' if ground else 'aire'; res[k][1] += 1
        if abs(model(a[0], b[1], ground, PAR, b[6], b[7]) - b[0]) <= TOL: res[k][0] += 1
    return res

if __name__ == '__main__':
    for p in sys.argv[1:]:
        po, pn = check_pitch(p); print(p, f'cabeceo {po}/{pn} ({100 * po / max(1, pn):.1f} %)')
        r = check(p); print(p, {k: f'{v[0]}/{v[1]} ({100 * v[0] / max(1, v[1]):.1f} %)' for k, v in r.items()})
        if os.environ.get('AI'):
            for i in range(1, 10):
                try: r = check(p, i)
                except KeyError: break
                print(f'   IA r{i}', {k: f'{100 * v[0] / max(1, v[1]):.1f} % de {v[1]}' for k, v in r.items()})
