# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Instantáneas completas del juego: volcado de la RAM del EE (32 MB, por PINE) y savestate de PCSX2 (RAM, VRAM de la GS, memoria de audio, IOP y una captura de pantalla). / Full game snapshots.
Solo biblioteca estándar. Estos archivos son datos del juego: se quedan en tu carpeta de salida y NO van a ningún repositorio."""
import glob, gzip, os, shutil, sys, time

EE_RAM = 0x2000000

def ram_dump(link, path, size=EE_RAM, chunk=0x10000, now=time.monotonic):
    """Vuelca [0, size) de la RAM del EE a un .gz leyendo por PINE en trozos. -> bytes escritos (sin comprimir)."""
    n = 0
    with gzip.open(path, 'wb', compresslevel=3) as f:
        for a in range(0, size, chunk):
            f.write(link.read_windows([(a, min(chunk, size - a))])[0]); n += min(chunk, size - a)
    return n

def sstates_dirs():
    """Carpetas donde PCSX2 guarda los savestates, de la más a la menos probable. / Likely PCSX2 savestate folders."""
    h = os.path.expanduser('~'); out = [os.path.join(h, '.var/app/net.pcsx2.PCSX2/config/PCSX2/sstates'), os.path.join(h, '.config/PCSX2/sstates'), os.path.join(h, '.local/share/PCSX2/sstates'),
                                       os.path.join(h, 'Library/Application Support/PCSX2/sstates'), os.path.join(h, 'Documents/PCSX2/sstates'), os.path.join(h, 'OneDrive/Documents/PCSX2/sstates')]
    up = os.environ.get('USERPROFILE')
    if up: out += [os.path.join(up, 'Documents', 'PCSX2', 'sstates'), os.path.join(up, 'OneDrive', 'Documents', 'PCSX2', 'sstates')]
    seen = []; [seen.append(p) for p in out if p not in seen]; return seen

def find_state_file(folder, serial, slot):
    f = sorted(glob.glob(os.path.join(folder, f'{serial} (*).{slot:02d}.p2s')))
    return f[0] if f else None

class StateSaver:
    """Pide a PCSX2 un savestate en una ranura y copia el archivo. La ranura puede tener un savestate TUYO: se copia a *.dhtel_backup antes de usarla y se restaura al terminar (restore())."""
    def __init__(self, link, serial, folder, slot=10):
        self.link, self.serial, self.folder, self.slot = link, serial, folder, slot; self.backup = None; self.existing = find_state_file(folder, serial, slot)   # existing: tu savestate previo (si lo hay)
        if self.existing:
            self.backup = self.existing + '.dhtel_backup'; shutil.copy2(self.existing, self.backup)
    def save(self, dest, timeout=40.0, now=time.monotonic, sleep=time.sleep):
        before = os.path.getmtime(self.existing) if self.existing and os.path.exists(self.existing) else 0; self.link.save_state(self.slot); t0 = now(); stable = None
        while now() - t0 < timeout:
            f = find_state_file(self.folder, self.serial, self.slot)
            if f and os.path.getmtime(f) > before:
                sz = os.path.getsize(f)
                if stable and stable[0] == sz and now() - stable[1] > 1.0: shutil.copy2(f, dest); self.existing = f; return sz
                if not stable or stable[0] != sz: stable = (sz, now())
            sleep(0.2)
        raise TimeoutError('PCSX2 no escribió el savestate a tiempo')
    def restore(self):
        """Deja la ranura como estaba: TU savestate original de vuelta, o vacía si no había ninguno."""
        cur = find_state_file(self.folder, self.serial, self.slot)
        try:
            if self.backup and os.path.exists(self.backup): shutil.move(self.backup, cur or self.existing)
            elif self.backup is None and cur: os.remove(cur)
        except OSError as e: print('aviso / warning: no pude restaurar la ranura / could not restore the slot:', e, file=sys.stderr)
