#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Extrae un modelo con texturas de un grupo NGP/PTR/TEX/RTX ya descomprimido -> .mdl
Uso: extract_model.py basepath(sin extensión) out.mdl [--variant N]
.mdl: 'DHM2', u32 ntex, u32 nv, u32 ni; por textura: u32 w,h + RGBA8; nv * 10 f32 (x y z u v r g b a tex); ni * u32 índices.
Enlaces verificados: PTR lista (texid u16, TEX0) en el mismo orden; TEX0.CBP == id (lo>>16) del registro de paleta del RTX;
T8 (psm 0x13) = textura 2w x 2h ya con swizzle GS; la paleta CT32 de 256 entradas usa el intercambio de bits 3 y 4 (CSM1)."""
import struct, sys, math, os
sys.path.insert(0, os.path.dirname(__file__))
from vif import iter_vif
from gs import upload32, read8, read4, read8h
from scene import Scene, Owners
base, out = sys.argv[1], sys.argv[2]
variant = int(sys.argv[sys.argv.index('--variant') + 1]) if '--variant' in sys.argv else 0
ngp, ptr, tex, rtx = (open(base + e, 'rb').read() for e in ('.NGP', '.PTR', '.TEX', '.RTX'))
u32 = lambda b, o: struct.unpack_from('<I', b, o)[0]
# niveles (LVL/*): sólo detalle fino + matrices del grafo; modelos sueltos (BIKE...) sin grafo útil -> todo, sin transformar. DH_SUB como en extract_mesh.py
_sub = os.environ.get('DH_SUB')
try: OW = Owners(Scene(ngp), [int(x, 0) for x in _sub.split(',')] if _sub and _sub not in ('all', 'fine') else _sub) if '/LVL/' in os.path.abspath(base) or _sub else None
except Exception: OW = None
def split_type25(OW, S):
    """Nodo tipo 25 (FUN_0020effc caso 0x19, docs/formats/ngp-nodes-25-11.md): contenedor con u32 n @+0x28 y n punteros a cadenas VIF @+0x2c. Esas cadenas pueden estar ANTES del nodo
    (Glacier: cielo con n=4), y Owners.owner() las daría a la hoja anterior; cada una pasa a ser un inicio propio de las instancias del nodo. / each chain becomes its own start (hipótesis: layout de +0x28/+0x2c verificado sólo con los datos)."""
    for k, d in enumerate(OW.inst):
        p = d['ptr']
        if S.u32(p) & 0x3f != 25: continue
        for i in range(min(S.u32(p + 0x28), 16)):
            q = S.ptr(p + 0x2c + 4*i)
            if q is None or q == p: continue
            if k not in OW.by_ptr.setdefault(q, []): OW.by_ptr[q].append(k)
            if OW.allowed is not None and p in OW.allowed: OW.allowed.add(q)
    OW.starts = sorted(OW.by_ptr)
if OW: split_type25(OW, Scene(ngp))

def records(b, first):
    off = first
    while off + 16 <= len(b):
        nxt, h, lo, hi = struct.unpack_from('<IIII', b, off)
        yield off, lo, hi
        if nxt == 0: break
        off += (nxt & ~3) * 4
texs = {}   # id -> (w, h, fmt de subida, bytes). fmt 0 = subida CT32 (datos con swizzle GS: T8 2w x 2h, T4, 8H); 19 = PSMT8 lineal w x h; 20 = PSMT4 lineal (2 píxeles/byte, bajo primero)
_recs = list(records(tex, u32(tex, 4) * 16)) + [(len(tex), 0, 0)]
for (off, lo, hi), (nxt_off, _, _) in zip(_recs, _recs[1:]):
    w, h, fmt = 1 << (hi >> 8 & 15), 1 << (hi >> 12 & 15), hi & 0x3f
    size = {19: w * h, 20: w * h // 2}.get(fmt, w * h * 4)
    texs[lo & 0xffff] = (w, h, fmt, tex[off + 0x80: off + 0x80 + min(size, max(0, nxt_off - off - 0x80))])
cluts = {}
for off, lo, hi in records(rtx, 0):
    w, h = 1 << (hi >> 8 & 15), 1 << (hi >> 12 & 15)
    if w * h in (256, 16): cluts[lo >> 16] = rtx[off + 16: off + 16 + w * h * 4]
n0 = u32(ptr, 0); q = 4 + 4 * n0; nt = u32(ptr, q); texref = struct.unpack_from(f'<{nt}I', ptr, q + 4)
q2 = q + 4 + 4 * nt; nc = u32(ptr, q2); tex0o = struct.unpack_from(f'<{nc}I', ptr, q2 + 4)
mats = []   # (offset del TEX0, id textura, psm, w, h, cbp)
for to, co in zip(texref, tex0o):
    v = struct.unpack_from('<Q', ngp, co)[0]
    mats.append((co, struct.unpack_from('<H', ngp, to)[0], v >> 20 & 63, 1 << (v >> 26 & 15), 1 << (v >> 30 & 15), v >> 37 & 0x3fff))

BADTEX = []
def make_texture(tid, psm, w, h, cbp):
    try: return _make_texture(tid, psm, w, h, cbp)
    except IndexError:                              # datos más cortos que lo que declara el TEX0: se registra y la textura queda sin resolver
        BADTEX.append((tid, hex(psm), w, h, texs[tid][:3])); return None

def _make_texture(tid, psm, w, h, cbp):
    """w,h = dimensiones reales de la textura (del TEX0). Paleta CLUT; el formato de los datos depende del fmt de subida del registro .TEX."""
    if psm not in (0x13, 0x14, 0x1b) or tid not in texs or cbp not in cluts: return None
    tw, th, fmt, data = texs[tid]; pal = cluts[cbp]; swap = lambda i: (i & ~0x18) | ((i & 8) << 1) | ((i & 16) >> 1)
    if fmt == 19:                                   # PSMT8 lineal
        w, h = tw, th; idx = data.ljust(w * h, b'\0'); sw = swap
    elif fmt == 20:                                 # PSMT4 lineal
        w, h = tw, th; idx = bytes(n for b in data.ljust(w * h // 2, b'\0') for n in (b & 15, b >> 4)); sw = lambda i: i
    else:
        mem = upload32(data.ljust(tw * th * 4, b'\0'), tw, th)
        if psm == 0x1b: idx = read8h(mem, w, h); sw = swap
        elif psm == 0x13: idx = read8(mem, w, h); sw = swap
        else: idx = read4(mem, w, h); sw = lambda i: i
    rgba = bytearray()
    for p in idx:
        e = sw(p) * 4; r, g, b, a = pal[e:e + 4]; rgba += bytes((r, g, b, 255 if a >= 0x80 else a * 2))
    return (w, h, bytes(rgba))

textures = []; tindex = {}
def get_tex(m):
    key = (m[1], m[5])
    if key not in tindex:
        t = make_texture(m[1], m[2], m[3], m[4], m[5])
        tindex[key] = len(textures) if t else -1
        if t: textures.append(t)
    return tindex[key]

RNG = [int(x, 0) for x in os.environ['DH_RANGE'].split(',')] if os.environ.get('DH_RANGE') else None   # depuración: "lo,hi" = sólo las cadenas VIF cuyo offset cae ahí (p. ej. las partes de piloto, que el grafo no recorre), sin instancias
last_tid = -1; V, I, VO, SK = [], [], [], []; L = len(ngp); i = 0; prev_end = 0
while i < L - 16:
    w = u32(ngp, i); cmd, num = w >> 24, (w >> 16) & 0xff
    rider = RNG is not None and cmd in (0x6c, 0x7c) and num >= 16 and i + 8 + 16 * num <= L and u32(ngp, i + 4 + 16 * num) >> 24 in (0x6a, 0x7a) and (u32(ngp, i + 4 + 16 * num) >> 16 & 0xff) == num   # sólo con DH_RANGE (los pilotos están en el NGP del nivel pero no se exportan con el mapa); piloto (FUN_00195b80, hipótesis): V4-32 (x, y, z, peso) + V3-8 (normales) en vez de V3-32; pose de referencia
    psz = 16 * num if rider else 12 * num
    if (cmd in (0x68, 0x78) and num >= 3 or rider) and i + 4 + psz <= L:
        pos = (tuple(x for k, x in enumerate(struct.unpack_from(f'<{4*num}f', ngp, i + 4)) if k & 3 != 3) if rider else struct.unpack_from(f'<{3*num}f', ngp, i + 4))
        if all(math.isfinite(f) and abs(f) < 2e5 for f in pos) :
            if (RNG and not RNG[0] <= i < RNG[1]) or (not RNG and OW and not OW.ok(i)): prev_end = i + 4 + psz; i = prev_end; continue   # fuera del detalle fino: salta, pero consume sus materiales
            group = [m for m in mats if prev_end <= m[0] < i]
            slot = min(variant, max(len(group) - 1, 0)); tid = get_tex(group[slot]) if group else last_tid   # sin material propio: hereda el estado GS anterior
            idx = hdr = None; flag = set(); uvs = cols = None
            end = i + 4 + psz
            skin = None
            if rider:   # piel (VU1, microprograma en .vutext 0x0c8d..0x0cc4, hipótesis leída del desensamblado): los 7 bits bajos de los u32 de x, y, z = id de hueso (múltiplo de 4 = índice de paleta*4); w = peso1, frac(w*2048) = peso2, 1-peso1-peso2 = peso3
                skin = []
                for k in range(num):
                    xi, yi, zi = struct.unpack_from('<3I', ngp, i + 4 + 16 * k); wf = struct.unpack_from('<f', ngp, i + 4 + 16 * k + 12)[0]; w2 = (wf * 2048) % 1.0
                    skin.append(((xi & 127) >> 2, (yi & 127) >> 2, (zi & 127) >> 2, wf, w2, 1.0 - wf - w2))
            insts = [(k, OW.apply(k, pos)) for k in OW.ids(i)] if OW and not RNG else []   # una copia por instancia del grafo (árboles, banderas…); sin dueño: tal cual
            if not insts: insts = [(None, pos)]
            for off, nm, imm, n2, sz in iter_vif(ngp, end, min(L, end + 0x4000)):
                if nm.startswith('?') or nm == 'UNPACK V3-32' or (RNG and nm == 'UNPACK V4-32' and n2 >= 16): break   # >= 16 vectores = el siguiente bloque de vértices (piloto)
                if nm == 'UNPACK V4-32':
                    hdr = imm & 0x3ff; idx = None; flag = set(); uvs = cols = None
                    sel = u32(ngp, off + 16)          # palabra 3 de la cabecera: bit 2k = pasar al material k (se mantiene hasta el próximo cambio)
                    if sel and group:
                        slot = min(((sel & -sel).bit_length() - 1) // 2, len(group) - 1); tid = get_tex(group[slot])
                elif nm == 'UNPACK S-8' and n2 >= 3 and idx is None: idx = list(ngp[off + 4: off + 4 + n2])
                elif nm == 'UNPACK S-8' and n2 <= 3 and idx is not None and hdr is not None:
                    v0 = ((imm & 0x3ff) - (hdr + 3)) // 3; flag.update(range(v0, v0 + n2))
                elif nm == 'UNPACK V4-8' and idx is not None and n2 == len(idx):      # color RGBA8 por vértice; 0x80 = 1.0 (modulación PS2)
                    cols = [tuple(c / 128.0 for c in ngp[off + 4 + 4 * j: off + 8 + 4 * j]) for j in range(n2)]   # (r, g, b, a)
                elif nm == 'UNPACK V4-5' and idx is not None and n2 == len(idx):      # RGBA 5551: cada canal de 5 bits se expande <<3
                    cols = []
                    for j in range(n2):
                        c = struct.unpack_from('<H', ngp, off + 4 + 2 * j)[0]; cols.append(((c & 31) * 8 / 128.0, (c >> 5 & 31) * 8 / 128.0, (c >> 10 & 31) * 8 / 128.0, 1.0))
                elif nm == 'UNPACK V2-16' and idx is not None and n2 == len(idx):
                    uvs = [struct.unpack_from('<hh', ngp, off + 4 + 4 * j) for j in range(n2)]
                elif nm.startswith('MS') and idx is not None and max(idx) < num:
                    for j in range(2, len(idx)):
                        if j in flag: continue          # ADC: el vértice j no dispara triángulo, la tira sigue (la paridad cuenta desde el inicio del lote)
                        order = [j-2, j-1, j]
                        if j & 1: order = [j-2, j, j-1]
                        if len({idx[o] for o in order}) < 3: continue
                        for k_inst, wpos in insts:
                            for o in order:
                                u, v = (uvs[o][0] / 4096.0, uvs[o][1] / 4096.0) if uvs else (0, 0)
                                V.extend((*wpos[3*idx[o]:3*idx[o]+3], u, 1.0 - v, *(cols[o] if cols else (1, 1, 1, 1)), tid)); I.append(len(I)); VO.append(k_inst)
                                if skin: SK.append(skin[idx[o]])
                    idx = hdr = None; flag = set(); uvs = cols = None
                end = off + 4 + sz
            prev_end = end; last_tid = tid; i = end if rider else i + 4 + psz; continue   # piloto: salta toda la cadena (sus S-8/V2-16 darían falsos 0x68/0x78)
    i += 4
def save(path, groups, chunks=False):
    """Escribe un .mdl con `groups` = [(dueño, [triángulos])], en ese orden, recompactando texturas. Con chunks=True escribe DHM3: tras los índices,
    u32 nchunks y por chunk: u32 primer_índice, u32 nº índices, u32 nsel, nsel * (cx, cy, cz, radio, dist2_max) f32 = rangos de visibilidad (nodos tipo 2 del grafo)."""
    remap, tx, vs, tab = {}, [], [], []
    for own, tris in groups:
        first = len(vs) // 10
        for t in tris:
            for k in range(3):
                row = list(V[(3*t+k)*10:(3*t+k)*10+10]); tid = int(row[9])
                if tid >= 0:
                    if tid not in remap: remap[tid] = len(tx); tx.append(textures[tid])
                    row[9] = remap[tid]
                vs += row
        tab.append((own, first * 1, len(vs) // 10 - first))
    nvv = len(vs) // 10
    with open(path, 'wb') as f:
        f.write((b'DHM3' if chunks else b'DHM2') + struct.pack('<III', len(tx), nvv, nvv))
        for w, h, px in tx: f.write(struct.pack('<II', w, h) + px)
        f.write(struct.pack(f'<{len(vs)}f', *vs)); f.write(struct.pack(f'<{nvv}I', *range(nvv)))
        if chunks:
            f.write(struct.pack('<I', len(tab)))
            for own, first, n in tab:
                sels = OW.info[own]['sels'] if OW and own in OW.info else ()
                f.write(struct.pack('<III', first, n, len(sels)))
                for sl in sels: f.write(struct.pack('<5f', *sl))
    return len(tx), nvv // 3

ntri = len(I) // 3; sky_path = out[:-4] + '.sky.mdl' if out.endswith('.mdl') else out + '.sky.mdl'
byown = {}
for t in range(ntri): byown.setdefault(VO[3*t], []).append(t)
sky_owners = {o for o in byown if o is not None and OW and OW.layer.get(o) == OW.BACKDROP} if not os.environ.get('DH_NOSKY') else set()   # telón de fondo/cielo: capa 2 del grafo
# panorama del horizonte (degradado de cielo, anillo de montañas, nubes): hojas de capa 1 de la primera raíz del grafo cuando ésta es pequeña (<=12 dueños) y de radio grande; se dibuja centrado en la cámara
dome_path = out[:-4] + '.dome.mdl' if out.endswith('.mdl') else out + '.dome'
dome_owners = set()
if OW and not os.environ.get('DH_NODOME') and OW.info:
    roots = {}
    for o in OW.info: roots.setdefault(OW.info[o]['root'], []).append(o)
    for R in (3000, 2000):    # radio mínimo; si ninguna raíz cumple con 3000 se prueba 2000 (PODARC, PODSUP: una sola hoja de 10 326 triángulos y radio 2360; PODSPE ya tiene domo con 3000 y no cambia)
        for r, grp in roots.items():   # raíz pequeña, de radio enorme, sin celdas de terreno (capa 3) y con todas las hojas en el origen (traslación de la matriz ~0)
            if len(grp) > 16 or max(OW.info[o]['rad'] for o in grp) <= R or any(OW.layer.get(o) == 3 for o in grp): continue
            if all(max(abs(x) for x in OW.info[o]['m'][12:15]) < 1500 for o in grp): dome_owners |= {o for o in grp if o in byown}
        if dome_owners: break
if os.path.exists(dome_path): os.remove(dome_path)
if dome_owners:
    save(dome_path, [(None, sorted(t for o in sorted(dome_owners) for t in byown[o]))])
    print(f'panorama: {len(dome_owners)} hojas, {sum(len(byown[o]) for o in dome_owners)} triángulos -> {os.path.basename(dome_path)}')
sky_owners -= dome_owners
if os.path.exists(sky_path): os.remove(sky_path)
if sky_owners:
    save(sky_path, [(None, sorted(t for o in sky_owners for t in byown[o]))])
    print(f'cielo: hojas {[hex(o) for o in sorted(sky_owners)]}, {sum(len(byown[o]) for o in sky_owners)} triángulos -> {os.path.basename(sky_path)}')
main = [(o, ts) for o, ts in byown.items() if o not in sky_owners and o not in dome_owners]; ntri = sum(len(ts) for _, ts in main)
save(out, main, chunks=bool(OW))
if SK and os.environ.get('DH_SKIN') and len(main) == 1 and len(SK) == len(I):   # sidecar .skin: 'DHSK', u32 n, n * (3 x u8 hueso paleta, pad, 3 x f32 pesos) en el MISMO orden que los vértices del .mdl (un solo dueño)
    with open(os.environ['DH_SKIN'], 'wb') as f:
        f.write(b'DHSK' + struct.pack('<I', len(SK)))
        for b0, b1, b2, w0, w1, w2 in SK: f.write(struct.pack('<4B3f', b0, b1, b2, 0, w0, w1, w2))
print(f'{len(textures)} texturas, {ntri * 3} vértices, {ntri} triángulos, {len(main)} chunks')
if BADTEX: print('texturas no decodificables (id, psm, w, h, (subida w,h,fmt)):', BADTEX[:8])
