#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Oráculo visual: render nativo de dhview vs la captura del propio juego dentro de un savestate de PCSX2 (misma cámara, mismo nivel).
Cifra: error absoluto medio por canal (0-255), por región (cielo / suelo) y de color medio; imagen lado a lado + diferencia.
Uso: visual_diff.py estado.p2s [estado2.p2s ...] [--iso ruta.iso] [--out dir] [--fov RAD] [--env K=V ...] [--assert MAE]
Los savestates y las capturas no van al repo (regla 4); salida por defecto: ~/dh-captures/vdiff."""
import io, os, subprocess, sys, zipfile
sys.path.insert(0, os.path.dirname(__file__))
import numpy as np
from PIL import Image
from p2s import State
from p2s_camera import dh_cam

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
NAMES_TABLE, LEVEL_ID = 0x2760D0, 0x4DCB40          # tabla de punteros a nombres (ELF) e id de nivel (DAT_004dcb40); ver docs/formats/audio.md (T4)
W, H = 640, 480

def level_name(st):
    p = st.u32(NAMES_TABLE + 4 * st.u32(LEVEL_ID)); return st.ee[p:p + 24].split(b'\0')[0].decode()

def render(st, name, iso, out_bmp, extra):
    (x, y, z), yaw, pitch = dh_cam(st)
    env = dict(os.environ, SDL_VIDEODRIVER='offscreen', DH_SIZE=f'{W} {H}', DH_CAM=f'{x} {y} {z} {yaw} {pitch}', DH_SHOT=out_bmp, DH_DT='0.016667')
    env.update(extra)
    r = subprocess.run([os.path.join(ROOT, 'build/dhview'), iso, name], env=env, timeout=600, capture_output=True, text=True)
    if not os.path.exists(out_bmp): raise SystemExit(f'dhview no produjo captura para {name}:\n{r.stderr[-800:]}')
    return Image.open(out_bmp).convert('RGB').resize((W, H))

def metrics(ref, mine):
    a, b = np.asarray(ref, np.float32), np.asarray(mine, np.float32); d = np.abs(a - b)
    top, bot = d[:H // 2], d[H // 2:]
    return {'mae': float(d.mean()), 'mae_top': float(top.mean()), 'mae_bot': float(bot.mean()),
            'mean_ref': a.mean((0, 1)).round(1).tolist(), 'mean_mine': b.mean((0, 1)).round(1).tolist()}

def main(argv):
    iso = os.path.join(ROOT, 'Downhill Domination.iso'); out = os.path.expanduser('~/dh-captures/vdiff'); extra = {}; limit = None; states = []; extra['DH_FOV'] = '1.4'   # FOV vertical medido (src/main.cpp kDefaultFov)
    i = 0
    while i < len(argv):
        a = argv[i]
        if a == '--iso': iso = argv[i + 1]; i += 1
        elif a == '--out': out = argv[i + 1]; i += 1
        elif a == '--fov': extra['DH_FOV'] = argv[i + 1]; i += 1
        elif a == '--env': k, v = argv[i + 1].split('=', 1); extra[k] = v; i += 1
        elif a == '--assert': limit = float(argv[i + 1]); i += 1
        else: states.append(a)
        i += 1
    if not states: raise SystemExit(__doc__)
    os.makedirs(out, exist_ok=True); worst = 0.0
    for p in states:
        st = State(p); name = level_name(st); tag = os.path.basename(p).split('.')[-2] + '_' + name
        ref = Image.open(io.BytesIO(zipfile.ZipFile(p).read('Screenshot.png'))).convert('RGB').resize((W, H))
        bmp = os.path.join(out, tag + '.bmp'); mine = render(st, name, iso, bmp, extra); os.remove(bmp)
        m = metrics(ref, mine); worst = max(worst, m['mae'])
        diff = Image.fromarray(np.clip(np.abs(np.asarray(ref, np.int16) - np.asarray(mine, np.int16)) * 3, 0, 255).astype(np.uint8))
        sheet = Image.new('RGB', (W * 3, H)); sheet.paste(ref, (0, 0)); sheet.paste(mine, (W, 0)); sheet.paste(diff, (2 * W, 0)); sheet.save(os.path.join(out, tag + '.png'))
        print(f"{tag}: MAE {m['mae']:.1f} (cielo {m['mae_top']:.1f}, suelo {m['mae_bot']:.1f}) color medio juego {m['mean_ref']} port {m['mean_mine']}")
    if limit is not None and worst > limit: raise SystemExit(f'FALLA: MAE máximo {worst:.1f} > {limit}')

if __name__ == '__main__': main(sys.argv[1:])
