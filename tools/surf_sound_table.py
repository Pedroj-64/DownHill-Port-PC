#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Genera src/surfsound.hpp: superficie del suelo -> material de sonido (docs/formats/audio.md).
Generates src/surfsound.hpp: ground surface -> sound material.

ES. Entradas: el ELF PAL (SLES_522.02 sacado de la ISO con build/iso_cat) y el mapa USA->PAL de
tools/usa_pal_map.py. Las tablas RIDE (PEDAL/COAST) se leen del propio ELF PAL: la función
(USA func_001CA768 -> PAL 0x1CE5F8) es un `switch (kind)` con tabla de saltos de 316 entradas y cada caso
carga el nombre "B_<MAT>_PEDAL"/"B_<MAT>_COAST" como u128; aquí se emulan lui/ori/dsll/pcpyld/sq.
La tabla LAND (aterrizaje, USA func_001CCFB8 -> PAL 0x1D0E50) también es una tabla de saltos, pero allí
los nombres se pasan en registros: el material de cada caso se toma de la decompilación USA
(github.com/skuza-oss/downhill-revived, GPL-3.0-or-later, `--revived`) y se COMPRUEBA contra la PAL: los
316 `kind` agrupados por destino de salto en la PAL tienen que dar exactamente un material cada grupo.
EN. Inputs: the PAL ELF and the USA->PAL map. RIDE tables come straight from the PAL jump table; the LAND
material per case comes from the USA decompilation and is checked against the PAL jump-target partition.
"""
import argparse, json, re, struct, sys
from collections import defaultdict

ap = argparse.ArgumentParser()
ap.add_argument('--elf', required=True)
ap.add_argument('--revived', required=True, help='carpeta del clon de downhill-revived / revived clone dir')
ap.add_argument('--map', required=True, help='JSON de tools/usa_pal_map.py')
ap.add_argument('--out', default='src/surfsound.hpp')
a = ap.parse_args()

d = open(a.elf, 'rb').read()
phoff = struct.unpack_from('<I', d, 0x1C)[0]
_, po, pv, _, pfs, *_ = struct.unpack_from('<8I', d, phoff)
def w(addr): return struct.unpack_from('<I', d, po + addr - pv)[0]
def sx(x): return x - 0x10000 if x & 0x8000 else x
M64 = (1 << 64) - 1
usa_pal = {int(k, 16): int(v, 16) for k, v in json.load(open(a.map)).items()}

def run(start, maxn=200):
    """Emula el bloque de un caso hasta el salto: devuelve los u128 guardados con sq."""
    lo = [0] * 32; hi = [0] * 32; names = []; p = start
    for _ in range(maxn):
        x = w(p); op = x >> 26; rs = (x >> 21) & 31; rt = (x >> 16) & 31; rd = (x >> 11) & 31; sa = (x >> 6) & 31; fn = x & 63; im = x & 0xFFFF
        p += 4
        if op == 0x0F: lo[rt] = (im << 16) & M64 if not im & 0x8000 else ((im << 16) | (M64 ^ 0xFFFFFFFF)) & M64
        elif op == 0x0D: lo[rt] = lo[rs] | im
        elif op == 0x09: lo[rt] = (lo[rs] + sx(im)) & M64
        elif op == 0 and fn == 0x38: lo[rd] = (lo[rt] << sa) & M64
        elif op == 0 and fn == 0x3C: lo[rd] = (lo[rt] << (sa + 32)) & M64
        elif op == 0 and fn in (0x25, 0x2D): lo[rd] = lo[rs] | lo[rt]
        elif op == 0x1C and fn == 0x09 and sa == 0x0E: hi[rd], lo[rd] = lo[rs], lo[rt]
        elif op == 0x1F: names.append((hi[rt] << 64) | lo[rt])
        if op == 2 or (op == 4 and rs == 0 and rt == 0):
            y = w(p)
            if (y >> 26) == 0x1F: names.append((hi[(y >> 16) & 31] << 64) | lo[(y >> 16) & 31])
            break
        if x == 0x03E00008: break
    return [n.to_bytes(16, 'little').rstrip(b'\0').decode('latin1') for n in names]

def jump_table(fa, n=1600):
    """(límite, dirección) de la tabla de saltos de 316 entradas: tras `sltiu 316` hay un lui/addiu cuya
    tabla apunta (las 316 entradas) a código dentro de la propia función."""
    for i in range(n):
        x = w(fa + 4 * i)
        if (x >> 26) == 0x0B and (x & 0xFFFF) == 316:
            for j in range(i - 8, i + 40):
                y = w(fa + 4 * j)
                if (y >> 26) != 0x0F: continue
                base = (y & 0xFFFF) << 16
                for k in range(j + 1, j + 8):
                    z = w(fa + 4 * k)
                    if (z >> 26) == 0x09 and ((z >> 21) & 31) == ((y >> 16) & 31):
                        tab = base + sx(z & 0xFFFF)
                        if all(fa <= w(tab + 4 * e) < fa + 0x3000 for e in range(316)): return 316, tab
    raise SystemExit('sin tabla de saltos en %#x' % fa)

MATS = ['', 'BRDG', 'CONC', 'DIRT', 'GRAS', 'H2O', 'ICE', 'LAVA', 'MUD', 'ROCK', 'ROOF', 'SAND', 'SNOW', 'TRAK', 'FENCE']
mi = {m: i for i, m in enumerate(MATS)}

# --- RIDE: PEDAL/COAST desde la PAL ---
lim, tab = jump_table(usa_pal[0x1CA768]); assert lim == 316
ride = []
for k in range(lim):
    ns = run(w(tab + 4 * k))
    if not ns: ride.append(0); continue
    m = re.match(r'B_(\w+?)_(PEDAL|COAST)$', ns[0])
    ride.append(mi[m.group(1)] if m else mi['FENCE'] if ns[0] == 'B_SCR_FENCE_L' else None)
assert None not in ride

# --- LAND: material por caso de la decompilación USA, verificado contra la partición de la PAL ---
src = open(a.revived + '/src/unsorted/001C5040.c').read().split('\n')
st = next(i for i, l in enumerate(src) if l.startswith('void func_001CCFB8'))
usa_land = {}; pend = []
for l in src[st:st + 700]:
    m = re.match(r'\s*case (0x[0-9A-Fa-f]+|\d+):', l)
    if m: pend.append(int(m.group(1), 0)); continue
    m = re.search(r'SFX_B_([A-Z0-9]+)_(SUSP|BUMP|LAND_SM|LAND_BG)', l)
    if m and pend:
        for c in pend: usa_land[c] = m.group(1)
        pend = []
    if l.startswith('}'): break
lim2, tab2 = jump_table(usa_pal[0x1CCFB8]); assert lim2 == 316
groups = defaultdict(list)
for k in range(lim2): groups[w(tab2 + 4 * k)].append(k)
for t, ks in groups.items():
    ms = {usa_land.get(k) for k in ks} - {None}
    assert len(ms) <= 1, (hex(t), ks, ms)   # un destino de salto = un material
land = [mi[usa_land[k]] if k in usa_land else 0 for k in range(316)]

def arr(name, v):
    rows = [', '.join(str(x) for x in v[i:i + 32]) for i in range(0, len(v), 32)]
    return 'inline constexpr uint8_t %s[316] = {\n    %s\n};\n' % (name, ',\n    '.join(rows))

out = '''// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// GENERADO por tools/surf_sound_table.py (no editar a mano) / GENERATED, do not edit.
// Superficie del suelo -> material de sonido. VALIDADO contra el ejecutable PAL (docs/formats/audio.md,
// «superficie → material, resuelto»): `kind = grupo(nivel) * 32 + bits 11-15 del u16 de superficie`
// (USA func_001C32D0 suma 0x20 por grupo de niveles; PAL idéntico), y de `kind` salen los nombres de los
// bancos B_<MAT>_{PEDAL,COAST,LAND_SM,...}. Material 0 = sin sonido. Referencia cruzada: decompilación USA
// de github.com/skuza-oss/downhill-revived (GPL-3.0-or-later), solo para el material de LAND (ver el script).
#pragma once
#include <cstdint>
#include <string>
namespace surfsound {
inline constexpr const char* kMat[] = {@MATS@};
@RIDE@@LAND@
// Grupo de niveles (mismos grupos que el juego usa para sumar 0x20 al kind). Desconocido -> 0 (como el `default` del juego).
inline int levelGroup(std::string n) {
    for (char& c : n) c = (char)std::toupper((unsigned char)c);
    struct P { const char* pre; int g; };
    static const P ps[] = {{"ALP", 0}, {"MOA", 1}, {"JUN", 2}, {"PER", 3}, {"GLA", 4}, {"AUB", 5}, {"FUJ", 6}, {"CIT", 7}, {"BC", 8}, {"MOS", 9}};
    for (const P& p : ps) if (n.rfind(p.pre, 0) == 0) return p.g;
    return 0;
}
inline int kind(int group, uint16_t surface) { return group * 32 + ((surface >> 11) & 31); }
// Nombre completo del sonido de rodadura ("B_ROCK_PEDAL" / "B_ROCK_COAST"); vacío si la superficie no suena.
inline std::string rideName(int kind, bool pedalling) {
    const uint8_t m = kRide[kind % 316]; if (!m) return {};
    if (kMat[m][0] == 'F') return "B_SCR_FENCE_L";
    return std::string("B_") + kMat[m] + (pedalling ? "_PEDAL" : "_COAST");
}
// Material de aterrizaje/choque ("ROCK", "SNOW", ...); nulo si el juego usa el caso por defecto.
inline const char* landMat(int kind) { const uint8_t m = kLand[kind % 316]; return m ? kMat[m] : nullptr; }
}  // namespace surfsound
'''
out = out.replace('@MATS@', ', '.join('"%s"' % m for m in MATS)).replace('@RIDE@', arr('kRide', ride)).replace('@LAND@', arr('kLand', land))
open(a.out, 'w').write(out)
print('escrito', a.out, 'ride', sum(1 for x in ride if x), 'land', sum(1 for x in land if x))
