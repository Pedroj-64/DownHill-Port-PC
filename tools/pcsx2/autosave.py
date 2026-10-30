#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Monitoriza una carrera en PCSX2 (PINE activado) y guarda un savestate cada N segundos para analizarlos después con tools/p2s.py.
Guarda en un slot propio (por defecto el 9, para no pisar tus F1) y copia cada estado con marca de tiempo FUERA del repo. Los .p2s no se suben nunca.
Uso: python3 tools/pcsx2/autosave.py [--every 5] [--count 12] [--slot 9] [--out ~/dh-captures/saves]"""
import argparse, glob, os, shutil, sys, time
sys.path.insert(0, os.path.dirname(__file__))
import pine

SST = os.path.expanduser('~/.var/app/net.pcsx2.PCSX2/config/PCSX2/sstates')

def main():
    a = argparse.ArgumentParser(); a.add_argument('--every', type=float, default=5); a.add_argument('--count', type=int, default=12)
    a.add_argument('--slot', type=int, default=9); a.add_argument('--out', default=os.path.expanduser('~/dh-captures/saves')); a.add_argument('--sstates', default=SST)
    o = a.parse_args(); os.makedirs(o.out, exist_ok=True)
    p = pine.Pine(pine.default_socket())
    if p.status() != 0: sys.exit('el juego no está corriendo (pausado o apagado)')
    pat = os.path.join(o.sstates, f'*.{o.slot:02d}.p2s')
    for n in range(o.count):
        t0 = time.time(); before = max((os.path.getmtime(f) for f in glob.glob(pat)), default=0)
        p.save_state(o.slot)
        while time.time() - t0 < 15:   # el guardado es asíncrono: esperar a que el archivo cambie y deje de crecer
            fs = [f for f in glob.glob(pat) if os.path.getmtime(f) > before]
            if fs:
                s1 = os.path.getsize(fs[0]); time.sleep(0.5)
                if os.path.getsize(fs[0]) == s1: break
            time.sleep(0.2)
        else: sys.exit('timeout esperando el savestate')
        dst = os.path.join(o.out, time.strftime('%H%M%S') + f'_{n:03d}.p2s'); shutil.copy(fs[0], dst); print(dst, flush=True)
        time.sleep(max(0, o.every - (time.time() - t0)))

if __name__ == '__main__': main()
