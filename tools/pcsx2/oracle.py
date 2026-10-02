#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""El propio juego como oráculo: carga un savestate, modifica la RAM por PINE y devuelve la captura de pantalla que el juego renderiza (vía un savestate temporal).
Uso como librería: e = Emu(); e.snapshot(load_slot=5) -> imagen PIL; e.zero(addr, n); ...   Los savestates quedan en la carpeta sstates de PCSX2 (fuera del repo)."""
import io, os, subprocess, sys, time, zipfile
sys.path.insert(0, os.path.dirname(__file__))
import pine
from PIL import Image

SSDIR = os.path.expanduser('~/.var/app/net.pcsx2.PCSX2/config/PCSX2/sstates'); INI = os.path.expanduser('~/.var/app/net.pcsx2.PCSX2/config/PCSX2/inis/PCSX2.ini')
GAME = 'SLES-52202 (73671EFD)'

class Emu:
    def __init__(self, iso=None, launch=True):
        self.proc = None
        if launch:
            if subprocess.run(['pgrep', '-x', 'pcsx2-qt'], capture_output=True).returncode == 0: raise SystemExit('PCSX2 abierto: ciérralo o usa launch=False')
            s = open(INI).read()
            if 'EnablePINE = true' not in s: open(INI, 'w').write(s.replace('EnablePINE = false', 'EnablePINE = true'))
            self.proc = subprocess.Popen(['flatpak', 'run', 'net.pcsx2.PCSX2', '-batch', '-fastboot', '--', iso], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        t0 = time.time(); sock = pine.default_socket()
        while not os.path.exists(sock):
            if time.time() - t0 > 90: raise SystemExit('sin socket PINE')
            time.sleep(0.5)
        self.p = pine.Pine(sock)
        while time.time() - t0 < 120:
            try:
                if self.p.status() == 0 and self.p.game_id().startswith('SLES'): break
            except Exception: pass
            time.sleep(0.5)
        time.sleep(2)
    def path(self, slot): return os.path.join(SSDIR, f'{GAME}.{slot:02d}.p2s')
    def load(self, slot, settle=1.0): self.p.load_state(slot); time.sleep(settle)
    def snapshot(self, tmp_slot=10, wait=0.4):
        """Guarda un savestate temporal y devuelve su Screenshot.png (espera a que el archivo cambie)."""
        f = self.path(tmp_slot); m0 = os.path.getmtime(f) if os.path.exists(f) else 0; time.sleep(wait); self.p.save_state(tmp_slot)
        for _ in range(100):
            time.sleep(0.1)
            if os.path.exists(f) and os.path.getmtime(f) > m0:
                time.sleep(0.3)
                try: return Image.open(io.BytesIO(zipfile.ZipFile(f).read('Screenshot.png'))).convert('RGB')
                except Exception: continue
        raise RuntimeError('no se generó el savestate temporal')
    def zero(self, addr, n):
        for a in range(addr, addr + n, 8 * 4096): self.p.send(b''.join(__import__('struct').pack('<BIQ', 7, x, 0) for x in range(a, min(a + 8 * 4096, addr + n), 8)))
    def close(self):
        if self.proc: subprocess.run(['flatpak', 'kill', 'net.pcsx2.PCSX2'])

import numpy as np
def veg_fraction(img):
    """Fracción de píxeles de follaje/tronco oscuro (verde dominante, brillo bajo) en el 60 % superior sin HUD: indicador de 'hay árboles' robusto al movimiento de la cámara."""
    a = np.asarray(img, dtype=int)[:int(480 * 0.6), 80:560]; r, g, b = a[..., 0], a[..., 1], a[..., 2]; v = a.max(2)
    return float(((g > r + 4) & (g > b + 4) & (v < 150)).mean())
def sky_fraction(img):
    a = np.asarray(img, dtype=int)[:int(480 * 0.6), 80:560]; r, g, b = a[..., 0], a[..., 1], a[..., 2]; return float(((r > g + 10) & (b > g + 5) & (a.max(2) > 90)).mean())
