# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Oráculo del controlador de giro (ctrl+0x248) con telemetría DHTEL1: pares de ticks consecutivos -> residuo del modelo. Solo biblioteca estándar. / Steering-controller oracle (ctrl+0x248) on DHTEL1 telemetry: consecutive tick pairs -> model residual. Standard library only.

Uso / Usage:  python3 tools/telemetry/ctrl_oracle.py <carpeta o ticks.dhtel[.gz]> [...]
Modelo (hipótesis de docs/telemetry-awiiwa.md): u = entrada (rider_input +0x10 der. - +0x14 izq.) analógica/128, o ±1 si es digital; suelo y aire en `model()`.
Un par vale si |ΔP| = 1 paso físico (0.02 s). Sale el % de pares que el modelo predice a <= TOL."""
import math, os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import schema as S

TOL = 2e-6                    # rad/tick (yaw es float32, ~1e-9 de resolución a estos valores)
K = 1.5 / 256                 # escala de la entrada analógica

def drive(path, rider=0):
    """Ticks del piloto `rider` (0 = jugador, 1..9 = IA; sus regiones son r<i>_ctrl / r<i>_input): (yaw, entrada, aire(contador), pasos físicos desde el anterior = 1, v) o None."""
    if os.path.isdir(path):
        for r, _, fs in os.walk(path):
            for f in fs:
                if f.startswith('ticks.dhtel'): path = os.path.join(r, f)
    _, samples = S.read_file(path); out = []; prev = None
    for _, _, p in samples:
        pre = f'r{rider}_' if rider else ''; c = p[pre + 'ctrl']; yaw = S.f32(c, 0x248); air = struct.unpack_from('<h', c, 0x4A4)[0]
        P = struct.unpack_from('<3f', c, 0x50); v = math.sqrt(sum(x * x for x in struct.unpack_from('<3f', c, 0xC0)))
        inp = struct.unpack_from('<i', p[pre + 'input'] if rider else p['rider_input'], 0x10)[0] - struct.unpack_from('<i', p[pre + 'input'] if rider else p['rider_input'], 0x14)[0]
        one = prev is not None and v > 1e-3 and abs(math.dist(P, prev) / v - 0.02) < 0.004
        ri = p[pre + 'input'] if rider else p['rider_input']; lean = (struct.unpack_from('<i', ri, 0x1C)[0] != 0) - (struct.unpack_from('<i', ri, 0x18)[0] != 0)      # +0x1C (abajo) +1, +0x18 (arriba) -1
        out.append((yaw, inp, air, one, S.f32(c, 0x240), lean)); prev = P
    return out

def u_of(inp):
    """Entrada normalizada: analógico inp/128 (inp=±127), digital (±1) = ±1."""
    return 0.0 if inp == 0 else float(inp) if abs(inp) == 1 else inp / 128

PAR = dict(gacc=0.008726646, gmax=0.033161256, gk=0.75, aacc=0.005235988, amax=0.046076693, adec=0.675, snap=0.0002)   # ctrl+0x43C, 0x438, factor de suelo, 0x45C, 0x450, 0x46C

def model(yaw, inp, ground, par):
    """Siguiente yaw (ctrl+0x248). Suelo: acc*u*0.75, tope max*|u|*0.75, sin entrada -> 0. Aire: acc*u, tope amax*|u|, sin entrada -> yaw*decay (a 0 si |yaw| < snap)."""
    u = u_of(inp)
    if ground:
        if u == 0: return 0.0
        m = par['gmax'] * abs(u) * par['gk']; return max(-m, min(m, yaw + par['gacc'] * u * par['gk']))
    if u == 0: return 0.0 if abs(yaw) < par['snap'] else yaw * par['adec']
    m = par['amax'] * abs(u); return max(-m, min(m, yaw + par['aacc'] * u))

PITCH = dict(acc=0.008726646, max=0.046076693, dec=0.8)         # ctrl+0x454, 0x458, 0x470

def pitch_model(p, lean, air):
    """Siguiente cabeceo (ctrl+0x240): en suelo 0; en aire +-acc por tick con cualquier pulsación (no depende de la magnitud), tope max, sin entrada *dec."""
    if not air: return 0.0
    if lean == 0: return p * PITCH['dec']
    return max(-PITCH['max'], min(PITCH['max'], p + PITCH['acc'] * lean))

def check_pitch(path, rider=0):
    t = drive(path, rider); ok = n = 0
    for a, b in zip(t, t[1:]):
        if not b[3] or not ((a[2] >= 2 and b[2] >= 2) or (a[2] == 0 and b[2] == 0)): continue
        n += 1; ok += abs(pitch_model(a[4], b[5], b[2] >= 2) - b[4]) <= TOL
    return ok, n

def check(path, rider=0):
    t = drive(path, rider); res = {'suelo': [0, 0], 'aire': [0, 0]}
    for a, b in zip(t, t[1:]):
        if not b[3]: continue
        ground = a[2] == 0 and b[2] == 0; air = a[2] >= 2 and b[2] >= 2
        if not (ground or air): continue
        k = 'suelo' if ground else 'aire'; res[k][1] += 1
        if abs(model(a[0], b[1], ground, PAR) - b[0]) <= TOL: res[k][0] += 1
    return res

if __name__ == '__main__':
    for p in sys.argv[1:]:
        po, pn = check_pitch(p); print(p, f'cabeceo {po}/{pn} ({100 * po / max(1, pn):.1f} %)'); r = check(p); print(p, {k: f'{v[0]}/{v[1]} ({100 * v[0] / max(1, v[1]):.1f} %)' for k, v in r.items()})
        if os.environ.get('AI'):
            for i in range(1, 10):
                try: r = check(p, i)
                except KeyError: break
                print(f'   IA r{i}', {k: f'{100 * v[0] / max(1, v[1]):.1f} % de {v[1]}' for k, v in r.items()})
