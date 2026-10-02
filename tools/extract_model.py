#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Extrae un modelo con texturas de un grupo NGP/PTR/TEX/RTX ya descomprimido -> .mdl
Uso: extract_model.py basepath(sin extensión) out.mdl [--variant N]
.mdl: 'DHM1', u32 ntex, u32 nv, u32 ni; por textura: u32 w,h + RGBA8; nv * 9 f32 (x y z u v r g b tex); ni * u32 índices.
Enlaces verificados: PTR lista (texid u16, TEX0) en el mismo orden; TEX0.CBP == id (lo>>16) del registro de paleta del RTX;
T8 (psm 0x13) = textura 2w x 2h ya con swizzle GS; la paleta CT32 de 256 entradas usa el intercambio de bits 3 y 4 (CSM1)."""
import struct, sys, math, os
sys.path.insert(0, os.path.dirname(__file__))
from vif import iter_vif
from gs import upload32, read8
base, out = sys.argv[1], sys.argv[2]
variant = int(sys.argv[sys.argv.index('--variant') + 1]) if '--variant' in sys.argv else 0
ngp, ptr, tex, rtx = (open(base + e, 'rb').read() for e in ('.NGP', '.PTR', '.TEX', '.RTX'))
u32 = lambda b, o: struct.unpack_from('<I', b, o)[0]

def records(b, first):
    off = first
    while off + 16 <= len(b):
        nxt, h, lo, hi = struct.unpack_from('<IIII', b, off)
        yield off, lo, hi
        if nxt == 0: break
        off += (nxt & ~3) * 4
texs = {}
for off, lo, hi in records(tex, u32(tex, 4) * 16):
    w, h = 1 << (hi >> 8 & 15), 1 << (hi >> 12 & 15)
    texs[lo & 0xffff] = (w, h, tex[off + 0x80: off + 0x80 + w * h * 4])
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

def make_texture(tid, psm, w, h, cbp):
    if psm != 0x13 or tid not in texs or cbp not in cluts: return None
    tw, th, data = texs[tid]
    idx = read8(upload32(data, tw, th), tw * 2, th * 2)
    pal = cluts[cbp]; sw = lambda i: (i & ~0x18) | ((i & 8) << 1) | ((i & 16) >> 1)
    rgba = bytearray()
    for p in idx:
        e = sw(p) * 4; r, g, b, a = pal[e:e + 4]; rgba += bytes((r, g, b, 255 if a >= 0x80 else a * 2))
    # el GS guarda la imagen invertida respecto al espacio UV del juego; se deja tal cual y se ajusta v en la malla
    return (tw * 2, th * 2, bytes(rgba))

textures = []; tindex = {}
def get_tex(m):
    key = (m[1], m[5])
    if key not in tindex:
        t = make_texture(m[1], m[2], m[3], m[4], m[5])
        tindex[key] = len(textures) if t else -1
        if t: textures.append(t)
    return tindex[key]

V, I = [], []; L = len(ngp); i = 0; prev_end = 0
while i < L - 16:
    w = u32(ngp, i); cmd, num = w >> 24, (w >> 16) & 0xff
    if cmd in (0x68, 0x78) and num >= 3 and i + 4 + 12 * num <= L:
        pos = struct.unpack_from(f'<{3*num}f', ngp, i + 4)
        if all(math.isfinite(f) and abs(f) < 2e5 for f in pos):
            group = [m for m in mats if prev_end <= m[0] < i]
            tid = get_tex(group[min(variant, len(group) - 1)]) if group else -1
            idx = hdr = None; flag = set(); uvs = None
            end = i + 4 + 12 * num
            for off, nm, imm, n2, sz in iter_vif(ngp, end, min(L, end + 0x4000)):
                if nm.startswith('?') or nm == 'UNPACK V3-32': break
                if nm == 'UNPACK V4-32': hdr = imm & 0x3ff; idx = None; flag = set(); uvs = None
                elif nm == 'UNPACK S-8' and n2 >= 3 and idx is None: idx = list(ngp[off + 4: off + 4 + n2])
                elif nm == 'UNPACK S-8' and n2 <= 3 and idx is not None and hdr is not None:
                    v0 = ((imm & 0x3ff) - (hdr + 3)) // 3; flag.update(range(v0, v0 + n2))
                elif nm == 'UNPACK V2-16' and idx is not None and n2 == len(idx):
                    uvs = [struct.unpack_from('<hh', ngp, off + 4 + 4 * j) for j in range(n2)]
                elif nm.startswith('MS') and idx is not None and max(idx) < num:
                    start = 0
                    for j in range(len(idx)):
                        if j in flag: start = j
                        if j - start < 2: continue
                        order = [j-2, j-1, j]
                        if (j - start) & 1: order = [j-2, j, j-1]
                        if len({idx[o] for o in order}) < 3: continue
                        for o in order:
                            u, v = (uvs[o][0] / 4096.0, uvs[o][1] / 4096.0) if uvs else (0, 0)
                            V.extend((*pos[3*idx[o]:3*idx[o]+3], u % 1.0, 1.0 - (v % 1.0), 1, 1, 1, tid)); I.append(len(I))
                    idx = hdr = None; flag = set(); uvs = None
                end = off + 4 + sz
            prev_end = end; i += 4 + 12 * num; continue
    i += 4
with open(out, 'wb') as f:
    f.write(b'DHM1' + struct.pack('<III', len(textures), len(V) // 9, len(I)))
    for w, h, px in textures: f.write(struct.pack('<II', w, h) + px)
    f.write(struct.pack(f'<{len(V)}f', *V)); f.write(struct.pack(f'<{len(I)}I', *I))
print(f'{len(textures)} texturas, {len(V)//9} vértices, {len(I)//3} triángulos')
