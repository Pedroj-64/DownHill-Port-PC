#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Contenedor .NGA (animaciones): tabla de clips, cabecera de clip y pistas (curvas) con su evaluador. Layout y citas del ELF: docs/formats/nga.md.
Uso: nga.py ARCHIVO.NGA   -> resumen. EN: .NGA animation container: clip table, clip header, track decoding and sampling."""
import struct, sys, math

def parse_nga(d):
    """-> (magic, clips). Cabecera según FUN_0020CEE0: u16 @0 (desconocido), u16 @2 = nº de entradas; entradas de 8 B desde +4: u16 id, u16 relleno, u32 offset
    (el motor hace item[id].clip = base + offset). Los ids NO son consecutivos (BANIM acaba en 196, 197, 202). Cada clip llega hasta el offset superior siguiente.
    clip = dict(id, off, size, hdr, cid, dur, tracks, counts, param)."""
    magic, = struct.unpack_from('<I', d); n = magic >> 16
    ent = [struct.unpack_from('<HHI', d, 4 + 8 * i) for i in range(n)]
    offs = sorted({o for _, _, o in ent}); nxt = {o: (offs[i + 1] if i + 1 < len(offs) else len(d)) for i, o in enumerate(offs)}
    clips = []
    for cid_, _, o in ent:
        e = nxt[o]; hdr, cid, z, dur, nt = struct.unpack_from('<2HIfI', d, o)   # z: 0 casi siempre; en 4 clips de RANIM es un f32 (10.0…), sin descifrar
        counts = struct.unpack_from(f'<{nt}H', d, o + 0x12) if o + 0x12 + 2 * nt <= e else None
        clips.append(dict(id=cid_, index=cid_, off=o, size=e - o, hdr=hdr, cid=cid, dur=dur, tracks=nt, counts=counts, param=z))
    return magic, clips

# --- pistas (FUN_0020A258 recorre; FUN_0020C5E8 despacha por (flags&7, flags>>3&7) -> tabla FUN_0020CDD0 @0x4FAFB8) ---
# Las pistas de un clip están ANTES de su cabecera: pista_k = pista_{k-1} - 4*counts[k] (pista_-1 = cabecera). Registro: u16 flags, u16 canal, u16 n, datos.
# Las pistas "cuantizadas" leen una cabecera de 16 B en pista-0x10 (f32 t0, dt, v0, dv) que cae dentro de la región de la pista SIGUIENTE (en orden de proceso).
HAS_HDR = {(1, 1), (1, 2), (2, 0), (2, 1), (2, 2), (4, 1), (4, 2)}
STRIDE = {(0, 0): (8, 20), (1, 0): (8, 16), (1, 1): (6, 8), (1, 2): (6, 6), (2, 0): (8, 4), (2, 1): (6, 2), (2, 2): (6, 1), (3, 0): (8, 0), (4, 0): (8, 12), (4, 2): (6, 4)}

def _tan(x):
    """Tangente de 16 bits (FUN_00268EF8 y vecinas): |x| < 16384 -> x/16384; códigos 01 / 10 en los 2 bits altos -> 16384/(±32768 - x)."""
    sx = x - 65536 if x >= 32768 else x; code = (sx << 16 >> 16) >> 14
    if code == -2: return 16384.0 / (-32768.0 - sx)
    if code == 1: return 16384.0 / (32768.0 - sx)
    return sx * 6.1035156e-05

def parse_track(d, pos, prev_quant_hdr):
    """-> dict(type, mode, channel, n, hdr=(t0,dt,v0,dv)|None, keys). Sólo las combinaciones del motor con evaluador conocido."""
    fl, ch, n = struct.unpack_from('<3H', d, pos); typ, mode = fl & 7, fl >> 3 & 7
    if (typ, mode) not in STRIDE: raise ValueError(f'pista ({typ},{mode}) sin evaluador conocido')
    hdr = struct.unpack_from('<4f', d, pos - 16) if (typ, mode) in HAS_HDR else None
    off, st = STRIDE[(typ, mode)]; keys = []
    if typ == 3: return dict(type=3, mode=mode, channel=ch, n=1, hdr=None, keys=[(0.0, struct.unpack_from('<f', d, pos + 4)[0])], size=8)   # constante: el u16 n es la mitad alta del f32
    for i in range(n):
        o = pos + off + st * i
        if (typ, mode) == (0, 0): keys.append(struct.unpack_from('<5f', d, o))                  # t, v, c3, c2, c1 (polinomio en t - t_k)
        elif (typ, mode) == (1, 0): keys.append(struct.unpack_from('<4f', d, o))                # t, v, tanA, tanB
        elif (typ, mode) == (1, 1): tq, vq, a, b = struct.unpack_from('<4H', d, o); keys.append((tq * hdr[1] + hdr[0], vq * hdr[3] + hdr[2], _tan(a), _tan(b)))
        elif (typ, mode) == (1, 2): tq, vq, a, b = struct.unpack_from('<2B2H', d, o); keys.append((tq * hdr[1] + hdr[0], vq * hdr[3] + hdr[2], _tan(a), _tan(b)))
        elif (typ, mode) == (2, 0): keys.append((hdr[0] + i * hdr[1], struct.unpack_from('<f', d, o)[0]))
        elif (typ, mode) == (2, 1): keys.append((hdr[0] + i * hdr[1], struct.unpack_from('<H', d, o)[0] * hdr[3] + hdr[2]))
        elif (typ, mode) == (2, 2): keys.append((hdr[0] + i * hdr[1], d[o] * hdr[3] + hdr[2]))
        elif (typ, mode) == (4, 0): keys.append(struct.unpack_from('<3f', d, o))                # t, v, tan
        elif (typ, mode) == (4, 2): tq, vq, a = struct.unpack_from('<2BH', d, o); keys.append((tq * hdr[1] + hdr[0], vq * hdr[3] + hdr[2], _tan(a)))
    return dict(type=typ, mode=mode, channel=ch, n=n, hdr=hdr, keys=keys, size=off + st * n)

def _herm(u, p0, p1, m0, m1):
    d = p1 - p0
    return u * (u * (u * ((m0 + m1) - 2 * d) + (3 * d - (2 * m0 + m1))) + m0) + p0

def sample_track(tr, t):
    """Valor de la pista en el tiempo t (mismas fórmulas que los evaluadores del ELF: FUN_00268D10 / 00268DE8 / 00268EF8 / 002691A8 / 00268968 / 00268A68 / 00269438 / 002694E8 / 00269600 y el stub 0020C5E0)."""
    k, ty, md = tr['keys'], tr['type'], tr['mode']
    if ty == 3: return k[0][1]
    if t <= k[0][0]: return k[0][1]
    if t >= k[-1][0]: return k[-1][1]
    i = max(j for j in range(len(k) - 1) if k[j][0] <= t)
    if ty == 2:
        u = (t - k[i][0]) / (k[i + 1][0] - k[i][0]); return k[i][1] + (k[i + 1][1] - k[i][1]) * u
    if ty == 0:
        x = t - k[i][0]; _, v, c3, c2, c1 = k[i]; return x * (x * (x * c3 + c2) + c1) + v
    dt = k[i + 1][0] - k[i][0]; u = (t - k[i][0]) / dt
    if ty == 1: return _herm(u, k[i][1], k[i + 1][1], k[i][2], k[i][3])
    m0 = k[i][2] * dt if md != 0 else k[i][2] * dt; m1 = k[i + 1][2] * dt
    return _herm(u, k[i][1], k[i + 1][1], m0, m1)

def clip_tracks(d, clip):
    """Pistas de un clip en orden de proceso: [(dict, región (inicio, tamaño))]. La región incluye la cabecera cuantizada de la pista anterior (en orden de proceso)."""
    pos = clip['off']; out = []
    for cnt in clip['counts']:
        pos -= 4 * cnt; out.append((parse_track(d, pos, None), (pos, 4 * cnt)))
    return out

if __name__ == '__main__':
    m, C = parse_nga(open(sys.argv[1], 'rb').read())
    print(f'magic {m:#x}, {len(C)} clips; pistas {min((c["tracks"] for c in C), default=0)}..{max((c["tracks"] for c in C), default=0)}')
