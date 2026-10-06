#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Extrae del .NGP/.TEX de un nivel el bloque de texturas del HUD (idéntico en todos los niveles, índice de partida distinto) a out/hud/hud.dat (gitignorado) para que dhview las cargue en tiempo de ejecución.
El bloque se localiza por su secuencia de tamaños: logotipo 256x128, 3 rótulos 64x64 (MPH, KPH, K.P.O con «PTS»), fuente LCD 128x128, tira 128x32, carcasa del ordenador 128x128, logotipos de pilotos 256x128.
Formato: 'DHUD' u32 n, luego por textura u32 w, u32 h, w*h*4 bytes RGBA. Uso: hud_export.py unpacked/LVL/ALP2 [out/hud/hud.dat]   (la salida son datos del juego: nunca va al repo)"""
import sys, os, io, contextlib, runpy, struct
SEQ = [(256, 128), (64, 64), (64, 64), (64, 64), (128, 128), (128, 32), (128, 128), (256, 128)]
base = sys.argv[1]; out = sys.argv[2] if len(sys.argv) > 2 else 'out/hud/hud.dat'
tmp = out + '.tmp.mdl'; os.makedirs(os.path.dirname(out) or '.', exist_ok=True)
sys.argv = ['extract_model.py', base, tmp]
with contextlib.redirect_stdout(io.StringIO()): g = runpy.run_path(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'extract_model.py'), run_name='hudexport')
tex = []; seen = set()
for m in g['mats']:
    if m[1] in seen or not g['texs'].get(m[1]): continue
    seen.add(m[1]); t = g['make_texture'](m[1], m[2], m[3], m[4], m[5]); tex.append((m[1], t))
tex.sort(key=lambda x: x[0])   # por número de textura (las materias salen en otro orden)
for i in range(len(tex) - len(SEQ) + 1):
    if all(tex[i + k][1] and tex[i + k][0] == tex[i][0] + k and (tex[i + k][1][0], tex[i + k][1][1]) == SEQ[k] for k in range(len(SEQ))):
        with open(out, 'wb') as f:
            f.write(b'DHUD' + struct.pack('<I', len(SEQ)))
            for k in range(len(SEQ)): w, h, rgba = tex[i + k][1]; f.write(struct.pack('<II', w, h) + rgba)
        print('HUD: bloque en las texturas', [tex[i + k][0] for k in range(len(SEQ))], '->', out); break
else:
    import glob
    for f in glob.glob(tmp[:-4] + '*'): os.remove(f)
    print('HUD: bloque no encontrado en', base); sys.exit(1)
import glob
for f in glob.glob(tmp[:-4] + '*'): os.remove(f)   # restos de la extracción temporal (.mdl, .dome.mdl, .sky.mdl)
