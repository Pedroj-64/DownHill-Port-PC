#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Mini rasterizador del GS (numpy) sobre la lista de dibujos de gsdump.py: reproduce un fotograma de un volcado de PCSX2 para compararlo con la captura del juego.
Soporta: triángulos / sprites, UV o ST/Q con corrección de perspectiva, texturas CT32/T8/T4/T8H con CLUT CT32 (CSM1), TFX modulate/decal, test alfa, test Z, mezcla ALPHA, niebla.
Sin implementar (marcado en la salida): CT16, T4HL/HH, filtrado bilineal/mipmaps (muestreo del vecino), máscara de frame, dithering.
Uso: gsraster.py volcado.gs[.zst] salida.png [--vs N] [--fbp N]"""
import sys, os, struct
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
from PIL import Image
import gs as gsm
import gsdump

BLK = np.array(gsm.BLK); COL32 = np.array(gsm.COL32); COL8 = np.array(gsm.COL8); BLK4 = np.array(gsm.BLK4); COL4 = np.array(gsm.COL4)

def a32(x, y, w): return ((y // 32) * ((w + 63) // 64) + x // 64) * 2048 + BLK[(y % 32) // 8, (x % 64) // 8] * 64 + COL32[y % 8, x % 8]
def a8(x, y, w): return ((y // 64) * ((w + 127) // 128) + x // 128) * 8192 + BLK[(y % 64) // 16, (x % 128) // 16] * 256 + COL8[y % 16, x % 16]
def a4(x, y, w): return ((y // 128) * ((w + 127) // 128) + x // 128) * 16384 + BLK4[(y % 128) // 16, (x % 128) // 32] * 512 + COL4[y % 16, x % 32]

class Textures:
    def __init__(self, vram): self.v32 = np.frombuffer(bytes(vram), np.uint32); self.vb = np.frombuffer(bytes(vram), np.uint8); self.cache = {}; self.unsupported = 0
    def clut(self, tex0):
        cbp, cpsm, csm, csa, psm = tex0 >> 37 & 0x3FFF, tex0 >> 51 & 15, tex0 >> 55 & 1, tex0 >> 56 & 31, tex0 >> 20 & 63
        n = 16 if psm in (0x14, 0x24, 0x2C) else 256; e = csa * 16 + np.arange(n) if n == 16 else np.arange(256)
        if cpsm != 0 or csm != 0: return None
        e2 = ((e & 0xE7) | ((e & 8) << 1) | ((e & 0x10) >> 1)) if n == 256 else e
        c = self.v32[cbp * 64 + a32(e2 & 15, e2 >> 4, 64)]
        return np.stack([c & 255, c >> 8 & 255, c >> 16 & 255, c >> 24 & 255], -1).astype(np.float32)
    def get(self, tex0):
        key = tex0 & ((1 << 61) - 1)
        if key in self.cache: return self.cache[key]
        tbp, tbw, psm, tw, th = tex0 & 0x3FFF, tex0 >> 14 & 63, tex0 >> 20 & 63, tex0 >> 26 & 15, tex0 >> 30 & 15
        W, H = 1 << tw, 1 << th; bw = max(tbw, 1) * 64; yy, xx = np.mgrid[0:H, 0:W]
        if psm == 0: c = self.v32[tbp * 64 + a32(xx, yy, bw)]; img = np.stack([c & 255, c >> 8 & 255, c >> 16 & 255, c >> 24 & 255], -1).astype(np.float32)
        elif psm in (0x13, 0x1B, 0x14):
            cl = self.clut(tex0)
            if cl is None: img = None
            else:
                if psm == 0x13: idx = self.vb[tbp * 256 + a8(xx, yy, bw)]
                elif psm == 0x1B: idx = (self.v32[tbp * 64 + a32(xx, yy, bw)] >> 24 & 255).astype(np.uint8)
                else: a = a4(xx, yy, bw); idx = self.vb[tbp * 256 + (a >> 1)] >> ((a & 1) * 4) & 15
                img = cl[idx]
        else: img = None
        if img is None: self.unsupported += 1; img = np.tile(np.array([255, 0, 255, 128], np.float32), (H, W, 1))
        self.cache[key] = img; return img

def clampuv(u, mode, mn, mx, size):
    u = np.floor(u).astype(np.int64)
    if mode == 0: return u & (size - 1)
    if mode == 1: return np.clip(u, 0, size - 1)
    if mode == 2: return np.clip(u, mn, mx)
    return ((u & mn) | mx) & (size - 1)

def render(rp, vs, fbp, W=640, H=448, log=None, owner=None, owner_any=None):   # owner: array int32 HxW opcional (índice del último dibujo que escribió cada píxel)
    tx = Textures(rp.vram); col = np.zeros((H, W, 3), np.float32); zb = np.zeros((H, W), np.float64); drawn = 0
    for di, d in enumerate(rp.draws):
        if d['vs'] != vs or (d['frame'] & 0x1FF) != fbp: continue
        prim = d['prim']; t = prim & 7; tme, fge, abe, fst = prim >> 4 & 1, prim >> 5 & 1, prim >> 6 & 1, prim >> 8 & 1
        ofx, ofy = d['xyoff'] & 0xFFFF, d['xyoff'] >> 32 & 0xFFFF; vs_ = d['verts']
        P = np.array([[(v[0] - ofx) / 16.0, (v[1] - ofy) / 16.0, float(v[2])] for v in vs_]);
        if t == 6:   # sprite: rectángulo entre 2 vértices, color del segundo
            (x0, y0, z0), (x1, y1, _) = P; q = [vs_[0], vs_[1]]
            xs, xe, ys, ye = int(max(0, np.ceil(min(x0, x1) - 0.5))), int(min(W, np.ceil(max(x0, x1) - 0.5))), int(max(0, np.ceil(min(y0, y1) - 0.5))), int(min(H, np.ceil(max(y0, y1) - 0.5)))
            if xs >= xe or ys >= ye: continue
            gx, gy = np.meshgrid(np.arange(xs, xe) + 0.5, np.arange(ys, ye) + 0.5)
            fx, fy = (gx - x0) / (x1 - x0 + 1e-9), (gy - y0) / (y1 - y0 + 1e-9)
            z = np.full(gx.shape, z0); rgba = np.broadcast_to(np.array(vs_[1][4], np.float32), gx.shape + (4,)); fogv = np.full(gx.shape, vs_[1][3], np.float32)
            if fst: u = vs_[0][7][0] / 16.0 + fx * (vs_[1][7][0] - vs_[0][7][0]) / 16.0; v = vs_[0][7][1] / 16.0 + fy * (vs_[1][7][1] - vs_[0][7][1]) / 16.0
            else:
                tw, th = d['tex0'] >> 26 & 15, d['tex0'] >> 30 & 15; s0, s1 = vs_[0][5], vs_[1][5]; q0, q1 = vs_[0][6] or 1., vs_[1][6] or 1.
                u = (s0[0] / q0 + fx * (s1[0] / q1 - s0[0] / q0)) * (1 << tw); v = (s0[1] / q0 + fy * (s1[1] / q1 - s0[1] / q0)) * (1 << th)
            m = np.ones(gx.shape, bool); xi, yi = gx.astype(int), gy.astype(int)
        else:
            (x0, y0, z0), (x1, y1, z1), (x2, y2, z2) = P; area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0)
            if abs(area) < 1e-6: continue
            xs, xe, ys, ye = int(max(0, np.floor(min(x0, x1, x2)))), int(min(W, np.ceil(max(x0, x1, x2)))), int(max(0, np.floor(min(y0, y1, y2)))), int(min(H, np.ceil(max(y0, y1, y2))))
            if xs >= xe or ys >= ye: continue
            gx, gy = np.meshgrid(np.arange(xs, xe) + 0.5, np.arange(ys, ye) + 0.5)
            w0 = ((x1 - gx) * (y2 - gy) - (x2 - gx) * (y1 - gy)) / area; w1 = ((x2 - gx) * (y0 - gy) - (x0 - gx) * (y2 - gy)) / area; w2 = 1 - w0 - w1
            m = (w0 >= -1e-6) & (w1 >= -1e-6) & (w2 >= -1e-6)
            if not m.any(): continue
            z = w0 * z0 + w1 * z1 + w2 * z2; xi, yi = gx.astype(int), gy.astype(int)
            C = [np.array(v[4], np.float32) for v in vs_]; rgba = w0[..., None] * C[0] + w1[..., None] * C[1] + w2[..., None] * C[2]
            fogv = w0 * vs_[0][3] + w1 * vs_[1][3] + w2 * vs_[2][3]
            if fst: u = (w0 * vs_[0][7][0] + w1 * vs_[1][7][0] + w2 * vs_[2][7][0]) / 16.0; v = (w0 * vs_[0][7][1] + w1 * vs_[1][7][1] + w2 * vs_[2][7][1]) / 16.0
            else:
                tw, th = d['tex0'] >> 26 & 15, d['tex0'] >> 30 & 15; qs = [vs_[i][6] or 1. for i in range(3)]
                den = w0 * qs[0] + w1 * qs[1] + w2 * qs[2]; den = np.where(np.abs(den) < 1e-12, 1e-12, den)
                u = (w0 * vs_[0][5][0] * qs[0] * 0 + 0) if False else (w0 * vs_[0][5][0] + w1 * vs_[1][5][0] + w2 * vs_[2][5][0])
                # ST ya son s/q de cada vértice (RGBAQ.Q guarda 1/w): s/q interpolado en perspectiva = (Σ w_i·S_i) / (Σ w_i·Q_i)
                u = u / den * (1 << tw); v = (w0 * vs_[0][5][1] + w1 * vs_[1][5][1] + w2 * vs_[2][5][1]) / den * (1 << th)
        # --- textura ---
        if tme:
            t0 = d['tex0']; img = tx.get(t0); tw, th = 1 << (t0 >> 26 & 15), 1 << (t0 >> 30 & 15); cl = d['clamp']
            wms, wmt, minu, maxu, minv, maxv = cl & 3, cl >> 2 & 3, cl >> 4 & 0x3FF, cl >> 14 & 0x3FF, cl >> 24 & 0x3FF, cl >> 34 & 0x3FF
            ui, vi = clampuv(u, wms, minu, maxu, tw), clampuv(v, wmt, minv, maxv, th); tc = img[vi, ui]; tfx, tcc = t0 >> 35 & 3, t0 >> 34 & 1
            if tfx == 0:   # modulate
                rgb = np.clip(tc[..., :3] * rgba[..., :3] / 128.0, 0, 255); a = (tc[..., 3] * rgba[..., 3] / 128.0) if tcc else rgba[..., 3]
            elif tfx == 1: rgb = tc[..., :3]; a = tc[..., 3] if tcc else rgba[..., 3]
            else: rgb = np.clip(tc[..., :3] * rgba[..., :3] / 128.0 + rgba[..., 3:4], 0, 255); a = tc[..., 3] if tcc else rgba[..., 3]
        else: rgb, a = rgba[..., :3], rgba[..., 3]
        if fge: f = fogv[..., None] / 255.0; fc = d['fogcol']; fcol = np.array([fc & 255, fc >> 8 & 255, fc >> 16 & 255], np.float32); rgb = rgb * f + fcol * (1 - f)
        # --- tests ---
        test = d['test']; ate, atst, aref = test & 1, test >> 1 & 7, test >> 4 & 255
        if ate:
            ok = {0: np.zeros_like(a, bool), 1: np.ones_like(a, bool), 2: a < aref, 3: a <= aref, 4: a == aref, 5: a >= aref, 6: a > aref, 7: a != aref}[atst]
            if not (test >> 12 & 3 == 0 and False): m = m & ok
        zte, ztst = test >> 16 & 1, test >> 17 & 3
        ys_, xs_ = np.clip(yi, 0, H - 1), np.clip(xi, 0, W - 1)
        if zte and ztst >= 2:
            zold = zb[ys_, xs_]; m = m & ((z >= zold) if ztst == 2 else (z > zold))
        elif zte and ztst == 0: m = m & False
        if not m.any(): continue
        cd = col[ys_, xs_]
        if abe:
            al = d['alpha']; A, B, Cc, D, fix = al & 3, al >> 2 & 3, al >> 4 & 3, al >> 6 & 3, al >> 32 & 255
            src, dst = rgb, cd; zero = np.zeros_like(src); pick = lambda k: (src, dst, zero)[k]
            cf = {0: a[..., None], 1: np.full_like(a, 128)[..., None], 2: np.full_like(a, fix)[..., None]}[min(Cc, 2)]
            if Cc == 1: cf = np.full_like(a, 128)[..., None]
            out = np.clip((pick(A) - pick(B)) * cf / 128.0 + pick(D), 0, 255)
        else: out = np.clip(rgb, 0, 255)
        col[ys_[m], xs_[m]] = out[m]
        if owner_any is not None: owner_any[ys_[m], xs_[m]] = di
        if owner is not None and not abe: owner[ys_[m], xs_[m]] = di   # sólo escrituras opacas (las mezcladas no se apropian del píxel)
        if not (d['zbuf'] >> 32 & 1): zb[ys_[m], xs_[m]] = z[m]
        drawn += 1
    if log is not None: log.update(drawn=drawn, unsupported_textures=tx.unsupported)
    return np.clip(col, 0, 255).astype(np.uint8)

def main(argv):
    path, out = argv[0], argv[1]; vs = int(argv[argv.index('--vs') + 1]) if '--vs' in argv else None
    dump = gsdump.Dump(path); rp = gsdump.Replay(dump, '--vram-start' not in argv); rp.run()
    vs = vs if vs is not None else max(x['vs'] for x in rp.draws); fb = [x for x in rp.draws if x['vs'] == vs]
    import collections; cnt = collections.Counter(x['frame'] & 0x1FF for x in fb); fbp = int(argv[argv.index('--fbp') + 1]) if '--fbp' in argv else cnt.most_common(1)[0][0]
    log = {}; img = render(rp, vs, fbp, log=log); Image.fromarray(img).save(out); print('vsync', vs, 'FBP', fbp, log)

if __name__ == '__main__': main(sys.argv[1:])
