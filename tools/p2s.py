#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Lectura de savestates de PCSX2 (.p2s = zip; Python >= 3.14 lee el zstd) y extracción del estado de los pilotos del juego (SLES-52202 PAL).
Los savestates y los volcados NO van al repo: este módulo sólo lee la ruta que se le pasa. Estructura y citas del ELF: docs/p2s-savestates.md.
Uso: p2s.py estado.p2s   -> tabla de pilotos."""
import struct, sys, zipfile

RIDER_BASE, RIDER_STRIDE, RIDER_COUNT_ADDR = 0x2D94C0, 0x7BD0, 0x329668   # DAT_002d94c0 + i*0x7bd0, DAT_00329668 = nº de pilotos (FUN_0016cf70, FUN_001a2738)
PHYS_OFF = 0x6420                                                            # módulo de física de la bici: rider+0x6420 (FUN_00155cb8/FUN_00133c78); +4 = piloto (back-pointer)

class State:
    def __init__(self, path):
        z = zipfile.ZipFile(path); self.names = [(i.filename, i.file_size) for i in z.infolist()]
        self.ee = z.read('eeMemory.bin'); self.version = z.read('PCSX2 Savestate Version.id')[:16]
    @classmethod
    def from_windows(cls, windows, datas, size=0x800000):
        """Estado parcial a partir de ventanas de RAM [(dirección, longitud)] + bytes (capturas PINE): el resto de la RAM queda a cero."""
        s = cls.__new__(cls); s.names = []; s.version = b''; ee = bytearray(size)
        for (a, n), d in zip(windows, datas): ee[a:a + n] = d
        s.ee = bytes(ee); return s
    def u32(self, a): return struct.unpack_from('<I', self.ee, a)[0]
    def f32(self, a, n=1): return struct.unpack_from(f'<{n}f', self.ee, a)
    def ok(self, a): return 0x100000 <= a < 0x2000000

    def riders(self):
        """-> lista de dict por piloto: nodo (+0x792C), tipo (+0x7A5C: 1 = jugador), posición del nodo (nodo+0x10), matriz 3x3 (nodo+0x40, filas), cuerpo rígido y puntos de contacto en mundo."""
        out = []
        for i in range(self.u32(RIDER_COUNT_ADDR)):
            r = RIDER_BASE + i * RIDER_STRIDE; node = self.u32(r + 0x792C)
            if not self.ok(node): continue
            phys = r + PHYS_OFF; n = self.u32(phys + 0x12C)
            pos = self.f32(node + 0x10, 3); rows = [self.f32(node + 0x40 + 16 * k, 3) for k in range(3)]
            cps = []
            for k in range(min(n, 8)):
                loc = self.f32(phys + 0x150 + 16 * k, 3); rad = self.f32(phys + 0x15C + 16 * k)[0]
                # FUN_001348A0: mundo = loc.x*fila0 + loc.y*fila1 + loc.z*fila2 + pos_nodo
                w = tuple(sum(loc[j] * rows[j][c] for j in range(3)) + pos[c] for c in range(3)); cps.append((loc, w, rad))
            rb = phys + 0x10
            out.append(dict(index=i, type=self.u32(r + 0x7A5C), node=node, pos=pos, rows=rows, phys=phys, contacts=cps,
                            rigid_pos=self.f32(rb + 0x50, 3), rigid_vel=self.f32(rb + 0xC0, 3), air_counter=self.u32(phys + 0x1D0)))
        return out

def hit_records(st, lo=0x100000, hi=0x2000000):
    """Restos de registros de impacto de 0x30 B (FUN_00219970/FUN_002193C0) que quedan en la pila de la física tras el paso: u16 1 en +0, surface u16 en +6, pen f32 +8,
    punto f32x3 +0x10, fracción f32 +0x1C, normal f32x3 +0x20 (unitaria) y d = n.v1 en +0x2C. Heurística: se aceptan los que tienen flag 1, normal unitaria y d finito.
    -> lista de dict (dirección, surface, frac, punto, normal, d, pen). Son restos de pila: pueden estar obsoletos; se contrastan con la malla (p2s_check.py)."""
    out = []; m = st.ee
    for o in range(lo, min(hi, len(m)) - 0x30, 16):
        if m[o] != 1 or m[o + 1] != 0: continue
        n = struct.unpack_from('<4f', m, o + 0x20)
        if abs(n[0] ** 2 + n[1] ** 2 + n[2] ** 2 - 1) > 1e-3 or not (-1e6 < n[3] < 1e6): continue
        surf, = struct.unpack_from('<H', m, o + 6)
        pt = struct.unpack_from('<3f', m, o + 0x10)
        if surf == 0 or not all(abs(c) < 1e6 for c in pt): continue
        out.append(dict(addr=o, surface=surf, frac=struct.unpack_from('<f', m, o + 0x1C)[0], point=pt, normal=n[:3], d=n[3], pen=struct.unpack_from('<f', m, o + 8)[0]))
    return out

if __name__ == '__main__':
    s = State(sys.argv[1]); print(s.version, [n for n, _ in s.names][:4], 'ee', len(s.ee))
    for r in s.riders():
        print(r['index'], 'tipo', r['type'], 'pos', tuple(round(v, 2) for v in r['pos']), 'vel', tuple(round(v, 1) for v in r['rigid_vel']), 'contactos', len(r['contacts']))
