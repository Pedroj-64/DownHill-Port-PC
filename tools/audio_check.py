#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Decodificador de audio nativo (src/vag.hpp): cada .VAG y .VPK de VAG/ en la ISO debe decodificarse BIT A BIT igual que ffmpeg (libavcodec adpcm_psx) en todo el tramo que ffmpeg entrega. Necesita la imagen, build/iso_cat,
build/audio_dump, build/iso_check y ffmpeg (datos locales, no va a CI; las pistas se extraen a un directorio temporal, nunca al repo).
/ Native audio decoder (src/vag.hpp): every .VAG and .VPK under VAG/ in the ISO must decode BIT-IDENTICALLY to ffmpeg (libavcodec adpcm_psx) over the whole stretch ffmpeg delivers. Needs the image, build/iso_cat, build/audio_dump,
build/iso_check and ffmpeg (local data, not for CI; tracks are extracted to a temp dir, never to the repo).
Uso / Usage:  python3 tools/audio_check.py [--assert] [PATRON ...]   (por defecto todos / default: all)"""
import glob, os, re, subprocess, sys, tempfile
import numpy as np
R = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

def main():
    iso = (glob.glob(os.path.join(R, '*.iso')) or [None])[0]
    if not iso or not os.path.exists(os.path.join(R, 'build/audio_dump')): print('falta la imagen o build/audio_dump'); sys.exit(2)
    lst = subprocess.run([os.path.join(R, 'build/iso_check'), iso], capture_output=True, text=True).stdout.splitlines()
    names = [l.split(' ', 2)[2] for l in lst if not l.startswith('DIR ') and re.match(r'VAG/.*\.(VAG|VPK)$', l.split(' ', 2)[2], re.I)]
    pats = [a.lower() for a in sys.argv[1:] if not a.startswith('--')]
    if pats: names = [n for n in names if any(p in n.lower() for p in pats)]
    bad = tot = 0; trunc = 0
    with tempfile.TemporaryDirectory() as t:
        for n in names:
            src = os.path.join(t, 'in'); subprocess.run([os.path.join(R, 'build/iso_cat'), iso, n, src], check=True)
            vpk = n.upper().endswith('.VPK'); ffsrc = src
            if not vpk:   # ffmpeg trata VAGp versión 4 como estéreo: se parchea la versión a 3 (mono) solo para la referencia
                d = bytearray(open(src, 'rb').read()); d[4:8] = (3).to_bytes(4, 'big'); ffsrc = os.path.join(t, 'ff.vag'); open(ffsrc, 'wb').write(d)
            raw = os.path.join(t, 'nat.raw'); r = subprocess.run([os.path.join(R, 'build/audio_dump'), 'vpk' if vpk else 'vag', src, raw], capture_output=True, text=True)
            ffr = os.path.join(t, 'ff.raw'); subprocess.run(['ffmpeg', '-v', 'quiet', '-y', '-i', ffsrc, '-f', 's16le', '-acodec', 'pcm_s16le', ffr])
            if r.returncode or not os.path.exists(ffr): bad += 1; print('ERROR', n, r.stderr.strip()); continue
            a, b = np.fromfile(raw, dtype='<i2'), np.fromfile(ffr, dtype='<i2'); m = min(len(a), len(b)); ok = m > 0 and np.array_equal(a[:m], b[:m]); tot += 1; bad += not ok; trunc += len(a) != len(b)
            print(('PASA ' if ok else 'FALLA'), n, f'{m} muestras comparadas' + ('' if len(a) == len(b) else f' (nativo {len(a)}, ffmpeg {len(b)})'))
    print(f'{tot} pistas comparadas, {bad} distintas; {trunc} con longitud distinta (ffmpeg corta el final) / tracks compared, differing, length mismatches')
    if '--assert' in sys.argv and (bad or not tot): sys.exit(1)

if __name__ == '__main__':
    main()
