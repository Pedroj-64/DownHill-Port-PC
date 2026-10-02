#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Cliente mínimo del protocolo PINE de PCSX2 (socket Unix). Paquete: u32 tamaño total (incluye esas 4 B) + comandos; respuesta: u32 tamaño + u8 resultado (0 = OK) + datos.
Comandos usados: Read8/16/32/64 = 0..3, Write* = 4..7, Version 8, SaveState 9, LoadState 10 (arg u8 slot), Title 11, ID 12, UUID 13, GameVersion 14, Status 15 (0 corriendo, 1 pausado, 2 apagado).
El socket del Flatpak está en $XDG_RUNTIME_DIR/app/net.pcsx2.PCSX2/pcsx2.sock (con PINESlot distinto del 28011 se añade .<slot>)."""
import os, socket, struct, time

RD8, RD16, RD32, RD64, VERSION, SAVE, LOAD, TITLE, ID, STATUS = 0, 1, 2, 3, 8, 9, 10, 11, 12, 15

def default_socket(slot=28011):
    rt = os.environ.get('XDG_RUNTIME_DIR', f'/run/user/{os.getuid()}')
    base = os.path.join(rt, 'app/net.pcsx2.PCSX2/pcsx2.sock')
    return base if slot == 28011 else f'{base}.{slot}'

class Pine:
    def __init__(self, path=None, timeout=5.0):
        self.path = path or default_socket(); self.s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM); self.s.settimeout(timeout); self.s.connect(self.path)
    def close(self): self.s.close()
    def _recvn(self, n):
        b = bytearray()
        while len(b) < n:
            c = self.s.recv(n - len(b))
            if not c: raise ConnectionError('PINE cerró la conexión')
            b += c
        return bytes(b)
    def send(self, body):
        """body = comandos concatenados -> bytes de datos de la respuesta (sin cabecera). Lanza IOError si el resultado != 0."""
        self.s.sendall(struct.pack('<I', len(body) + 4) + body)
        n, = struct.unpack('<I', self._recvn(4)); r = self._recvn(n - 4)
        if r[0] != 0: raise IOError(f'PINE: resultado {r[0]}')
        return r[1:]
    def status(self): return struct.unpack('<I', self.send(bytes([STATUS])))[0]
    def title(self):
        d = self.send(bytes([TITLE])); n, = struct.unpack_from('<I', d); return d[4:4 + n].rstrip(b'\0').decode(errors='replace')
    def game_id(self):
        d = self.send(bytes([ID])); n, = struct.unpack_from('<I', d); return d[4:4 + n].rstrip(b'\0').decode(errors='replace')
    def load_state(self, slot): self.send(bytes([LOAD, slot]))
    def save_state(self, slot): self.send(bytes([SAVE, slot]))
    def read32(self, a): return struct.unpack('<I', self.send(struct.pack('<BI', RD32, a)))[0]
    def read_windows(self, windows, max_cmds=6000):
        """windows = [(dirección, longitud múltiplo de 8)] -> lista de bytes por ventana, leyendo con Read64 en lotes (un paquete por lote)."""
        cmds = [(a + 8 * i, wi) for wi, (a, ln) in enumerate(windows) for i in range(ln // 8)]; out = [bytearray() for _ in windows]; k = 0
        while k < len(cmds):
            chunk = cmds[k:k + max_cmds]; body = b''.join(struct.pack('<BI', RD64, a) for a, _ in chunk); d = self.send(body)
            for j, (_, wi) in enumerate(chunk): out[wi] += d[8 * j:8 * j + 8]
            k += max_cmds
        return [bytes(o) for o in out]
