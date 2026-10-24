#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Lector de volcados GS de PCSX2 (.gs / .gs.zst, formato v2) y reproductor de listas de dibujo.
Layout verificado con un volcado real (ranura 1 de ALPINEMX): u32 0xFFFFFFFF, u32 tamaño de cabecera; cabecera = 9 u32 (versión, tamaño del estado,
offset y tamaño del serial, crc, ancho y alto de la captura, offset y tamaño de la captura) + serial + captura RGBA; después el ESTADO
(`state_size` bytes: registros ~509 B + VRAM de 4 MiB), 0x2000 B de registros privilegiados y la cola de paquetes
(0 = transferencia {u8 ruta, u32 tamaño, datos}, 1 = vsync {u8}, 2 = readfifo {u32 tamaño, datos}, 3 = registros 0x2000 B); la suma cuadra con el fin del archivo.
Los volcados NO van al repo (regla 4). Uso: gsdump.py volcado.gs[.zst] [--draws] [--png dir]   (resumen / lista de dibujos / texturas)"""
import struct, subprocess, sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gs as gsm

def load(path):
    if path.endswith('.zst'): return subprocess.run(['zstd', '-d', '-q', '-c', path], capture_output=True, check=True).stdout
    return open(path, 'rb').read()

class Dump:
    def __init__(self, path):
        d = self.raw = load(path)
        if struct.unpack_from('<I', d, 0)[0] != 0xFFFFFFFF: raise ValueError('formato v1 sin soporte')
        hs = struct.unpack_from('<I', d, 4)[0]
        (self.version, self.state_size, so, sl, self.crc, self.sw, self.sh, sso, ssl) = struct.unpack_from('<9I', d, 8)
        self.serial = d[8 + so:8 + so + sl].decode(errors='replace'); self.screenshot = d[8 + sso:8 + sso + ssl]
        self.state_off = 8 + hs; o = self.state_off + self.state_size
        self.priv_regs = d[o:o + 0x2000]; self.packets = []; o += 0x2000
        while o < len(d):
            t = d[o]; o += 1
            if t == 0: path_, sz = d[o], struct.unpack_from('<I', d, o + 1)[0]; self.packets.append((0, path_, o + 5, sz)); o += 5 + sz
            elif t == 1: self.packets.append((1, d[o], 0, 0)); o += 1
            elif t == 2: sz = struct.unpack_from('<I', d, o)[0]; self.packets.append((2, 0, o + 4, sz)); o += 4 + sz
            elif t == 3: self.packets.append((3, 0, o, 0x2000)); o += 0x2000
            else: raise ValueError(f'paquete desconocido {t} en {o - 1}')
        if o != len(d): raise ValueError('el flujo no cuadra con el fin del archivo')
        # la VRAM (4 MiB) ocupa el principio o el final del estado (el resto son registros): se decide en vram()
    def vram(self, at_end=True):
        s = self.state_off; n = 4 << 20
        return bytearray(self.raw[s + self.state_size - n:s + self.state_size] if at_end else self.raw[s:s + n])

REG = {0x00: 'PRIM', 0x01: 'RGBAQ', 0x02: 'ST', 0x03: 'UV', 0x04: 'XYZF2', 0x05: 'XYZ2', 0x06: 'TEX0_1', 0x07: 'TEX0_2', 0x08: 'CLAMP_1', 0x09: 'CLAMP_2',
       0x0A: 'FOG', 0x0C: 'XYZF3', 0x0D: 'XYZ3', 0x14: 'TEX1_1', 0x15: 'TEX1_2', 0x18: 'XYOFFSET_1', 0x19: 'XYOFFSET_2', 0x1A: 'PRMODECONT', 0x1B: 'PRMODE',
       0x1C: 'TEXCLUT', 0x22: 'SCANMSK', 0x34: 'MIPTBP1_1', 0x35: 'MIPTBP1_2', 0x36: 'MIPTBP2_1', 0x37: 'MIPTBP2_2', 0x3B: 'TEXA', 0x3D: 'FOGCOL',
       0x3F: 'TEXFLUSH', 0x40: 'SCISSOR_1', 0x41: 'SCISSOR_2', 0x42: 'ALPHA_1', 0x43: 'ALPHA_2', 0x44: 'DIMX', 0x45: 'DTHE', 0x46: 'COLCLAMP', 0x47: 'TEST_1',
       0x48: 'TEST_2', 0x49: 'PABE', 0x4A: 'FBA_1', 0x4B: 'FBA_2', 0x4C: 'FRAME_1', 0x4D: 'FRAME_2', 0x4E: 'ZBUF_1', 0x4F: 'ZBUF_2', 0x50: 'BITBLTBUF', 0x51: 'TRXPOS',
       0x52: 'TRXREG', 0x53: 'TRXDIR', 0x54: 'HWREG', 0x60: 'SIGNAL', 0x61: 'FINISH', 0x62: 'LABEL'}
PRIM_VERTS = {0: 1, 1: 2, 2: 2, 3: 3, 4: 3, 5: 3, 6: 2}   # punto, línea, tira de líneas, triángulo, tira, abanico, sprite (vértices por kick)

class Replay:
    """Interpreta la cola GIF: devuelve la lista de dibujos con el estado vigente y mantiene la VRAM local (transferencias host->local)."""
    def __init__(self, dump, vram_at_end=True):
        self.d = dump; self.vram = dump.vram(vram_at_end); self.r = {}; self.draws = []; self.xfer_log = []
        self.v = []; self.rgba = (128, 128, 128, 128); self.st = (0., 0.); self.uv = (0, 0); self.q = 1.; self.fog = 0; self.stream = {}
        self.trx = None; self.hw = bytearray(); self.vs = 0
    # --- registros ---
    def write(self, reg, val):
        name = REG.get(reg)
        if name is None: return
        r = self.r
        if name == 'RGBAQ': self.rgba = (val & 255, val >> 8 & 255, val >> 16 & 255, val >> 24 & 255); self.q = struct.unpack('<f', struct.pack('<I', val >> 32 & 0xFFFFFFFF))[0]; return
        if name == 'ST': self.st = struct.unpack('<2f', struct.pack('<2I', val & 0xFFFFFFFF, val >> 32 & 0xFFFFFFFF)); return
        if name == 'UV': self.uv = (val & 0x3FFF, val >> 16 & 0x3FFF); return
        if name == 'FOG': self.fog = val >> 56 & 255; return
        if name in ('XYZ2', 'XYZF2', 'XYZ3', 'XYZF3'):
            x, y = val & 0xFFFF, val >> 16 & 0xFFFF
            if name.startswith('XYZF'): z, f = val >> 32 & 0xFFFFFF, val >> 56 & 255
            else: z, f = val >> 32 & 0xFFFFFFFF, self.fog
            self.v.append((x, y, z, f, self.rgba, self.st, self.q, self.uv))
            if name in ('XYZ2', 'XYZF2'): self.kick()
            return
        if name == 'HWREG': return
        if name == 'PRIM': self.v = []
        r[name] = val
        if name == 'TRXDIR': self.start_xfer(val & 3)
    def ctx(self): return 1 if (self.r.get('PRIM', 0) >> 9) & 1 else 0
    def kick(self):
        prim = self.r.get('PRIM', 0); t = prim & 7; n = PRIM_VERTS.get(t)
        if n is None or len(self.v) < n: return
        c = (prim >> 9 & 1) + 1; g = lambda k: self.r.get(f'{k}_{c}', 0)
        vs = self.v[-n:]
        if t in (4, 5) and len(self.v) >= 3: vs = self.v[-3:] if t == 4 else [self.v[0], self.v[-2], self.v[-1]]
        self.draws.append(dict(vs=self.vs, prim=prim, verts=list(vs), xyoff=g('XYOFFSET'), tex0=g('TEX0'), tex1=g('TEX1'), clamp=g('CLAMP'), alpha=g('ALPHA'), test=g('TEST'),
                               frame=g('FRAME'), zbuf=g('ZBUF'), scissor=g('SCISSOR'), texa=self.r.get('TEXA', 0), fogcol=self.r.get('FOGCOL', 0), pabe=self.r.get('PABE', 0),
                               colclamp=self.r.get('COLCLAMP', 1), dthe=self.r.get('DTHE', 0)))
        if t in (0, 1, 3, 6): self.v = []
        elif t == 2: self.v = self.v[-1:]
        else: self.v = self.v[-2:] if t == 4 else [self.v[0], self.v[-1]]
    # --- transferencias ---
    def start_xfer(self, d):
        if d != 0: self.trx = None; return        # sólo host->local (0); local->host / local->local no cambian lo que se dibuja aquí
        b = self.r.get('BITBLTBUF', 0); p = self.r.get('TRXPOS', 0); rg = self.r.get('TRXREG', 0)
        self.trx = dict(dbp=b >> 32 & 0x3FFF, dbw=b >> 48 & 63, dpsm=b >> 56 & 63, dx=p >> 32 & 0x7FF, dy=p >> 48 & 0x7FF, w=rg & 0xFFF, h=rg >> 32 & 0xFFF, pos=0, buf=bytearray())
    def feed_image(self, data):
        t = self.trx
        if t is None: return
        t['buf'] += data
        if len(t['buf']) >= self.xfer_bytes(t): self.finish_xfer()
    def xfer_bytes(self, t): return {0: 4, 1: 3, 2: 2, 0x0A: 2, 0x13: 1, 0x14: 0.5, 0x1B: 1, 0x24: 4, 0x2C: 4, 0x30: 4, 0x31: 4, 0x32: 2, 0x3A: 2}.get(t['dpsm'], 4) * t['w'] * t['h']
    def finish_xfer(self):
        t = self.trx; self.trx = None; p = t['dpsm']; w, h, buf = t['w'], t['h'], t['buf']; base = t['dbp'] * 256; bw = t['dbw'] * 64; self.xfer_log.append((t['dbp'], p, w, h))
        if p in (0, 1, 0x30, 0x31, 0x32):   # CT32/CT24/Z: palabras de 32 bits (CT24 usa los 3 primeros bytes; aquí se transfiere con relleno de 4 por simplicidad)
            if p == 1: buf = b''.join(buf[i:i + 3] + b'\x00' for i in range(0, w * h * 3, 3))
            for y in range(h):
                for x in range(w):
                    a = base + gsm.addr32(t['dx'] + x, t['dy'] + y, bw) * 4; i = (y * w + x) * 4; self.vram[a:a + 4] = buf[i:i + 4]
        elif p == 0x13:   # PSMT8
            for y in range(h):
                for x in range(w): self.vram[base + gsm.addr8(t['dx'] + x, t['dy'] + y, bw)] = buf[y * w + x]
        elif p == 0x14:   # PSMT4
            for y in range(h):
                for x in range(w):
                    a = base * 2 + gsm.addr4(t['dx'] + x, t['dy'] + y, bw); b = buf[(y * w + x) >> 1]; nib = (b >> (4 * (x & 1))) & 15
                    o = a >> 1; self.vram[o] = (self.vram[o] & ~(15 << (4 * (a & 1)))) | (nib << (4 * (a & 1)))
        elif p in (2, 0x0A):   # CT16/CT16S: se guarda como CT16 lineal por páginas (aprox.; ver tex_at)
            self.vram16 = getattr(self, 'vram16', {}); self.vram16[(t['dbp'], bw)] = (t['dx'], t['dy'], w, h, bytes(buf))
    # --- GIF ---
    def run(self, upto_vsync=None):
        vs = 0; stream = bytearray()
        for kind, a, o, sz in self.d.packets:
            if kind == 1:
                vs += 1; self.vs = vs
                if upto_vsync is not None and vs >= upto_vsync: break
            elif kind == 0: self.gif(self.d.raw[o:o + sz], a)
        return self.draws
    def packed(self, reg, a, b):
        """Un qword PACKED (descriptor `reg`) -> escrituras equivalentes a registros."""
        if reg == 0: self.write(0x00, a & 0x7FF)
        elif reg == 1: self.write(0x01, (a & 255) | (a >> 32 & 255) << 8 | (b & 255) << 16 | (b >> 32 & 255) << 24 | struct.unpack('<I', struct.pack('<f', self.q))[0] << 32)
        elif reg == 2: self.q = struct.unpack('<f', struct.pack('<I', b & 0xFFFFFFFF))[0]; self.write(0x02, (a & 0xFFFFFFFF) | (a >> 32) << 32)
        elif reg == 3: self.write(0x03, (a & 0x3FFF) | (a >> 32 & 0x3FFF) << 16)
        elif reg in (4, 5, 0xC, 0xD):
            x, y, adc = a & 0xFFFF, a >> 32 & 0xFFFF, b >> 47 & 1
            if reg in (4, 0xC):
                val = x | y << 16 | (b >> 4 & 0xFFFFFF) << 32 | (b >> 36 & 255) << 56; reg = 0xC if (adc or reg == 0xC) else 4
            else: val = x | y << 16 | (b & 0xFFFFFFFF) << 32; reg = 0xD if (adc or reg == 0xD) else 5
            self.write(reg, val)
        elif reg == 0xA: self.write(0x0A, (b >> 36 & 255) << 56)
        elif reg == 0xE: self.write(b & 0xFF, a)
    def gif(self, data, path):
        data = self.stream.get(path, b'') + data; i = 0; n = len(data)
        while i + 16 <= n:
            lo, hi = struct.unpack_from('<2Q', data, i)
            nloop, pre, prim, flg, nreg = lo & 0x7FFF, lo >> 46 & 1, lo >> 47 & 0x7FF, lo >> 58 & 3, lo >> 60 & 15 or 16
            regs = [hi >> (4 * k) & 15 for k in range(nreg)]
            body = nloop * nreg * 16 if flg == 0 else ((nloop * nreg + 1) // 2 * 16 if flg == 1 else nloop * 16)
            if i + 16 + body > n: break
            if pre and flg != 2: self.write(0x00, prim)
            j = i + 16
            if flg == 0:
                for _ in range(nloop):
                    for rg in regs: a, b = struct.unpack_from('<2Q', data, j); j += 16; self.packed(rg, a, b)
            elif flg == 1:
                for k in range(nloop * nreg): self.write(regs[k % nreg], struct.unpack_from('<Q', data, j + 8 * k)[0])
            else: self.feed_image(data[j:j + body])
            i += 16 + body
        self.stream[path] = data[i:]
