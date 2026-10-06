# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Cliente PINE de PCSX2 autocontenido (solo biblioteca estándar): socket Unix (Linux, Flatpak, macOS) o TCP (Windows). / Self-contained PCSX2 PINE client: Unix socket or TCP (Windows).
Protocolo: paquete = u32 tamaño total (incluye esos 4 B) + comandos; respuesta = u32 tamaño + u8 resultado (0 = OK) + datos. Comandos: Read64 = 3, Version 8, Title 11, ID 12, Status 15."""
import os, socket, struct, sys

RD32, RD64, SAVE, VERSION, TITLE, ID, STATUS = 2, 3, 9, 8, 11, 12, 15

def candidate_sockets(slot=28011):
    """Rutas posibles del socket Unix de PINE, de la más a la menos probable. / Likely Unix socket paths."""
    sfx = '' if slot == 28011 else f'.{slot}'; out = []
    rt = os.environ.get('XDG_RUNTIME_DIR') or f'/run/user/{os.getuid()}' if hasattr(os, 'getuid') else None
    if rt: out += [os.path.join(rt, '.flatpak/net.pcsx2.PCSX2/xdg-run/pcsx2.sock' + sfx), os.path.join(rt, 'pcsx2.sock' + sfx), os.path.join(rt, 'app/net.pcsx2.PCSX2/pcsx2.sock' + sfx)]
    tmp = os.environ.get('TMPDIR') or '/tmp'; out += [os.path.join(tmp, 'pcsx2.sock' + sfx), '/tmp/pcsx2.sock' + sfx]
    seen = []; [seen.append(p) for p in out if p not in seen]; return seen

class Link:
    """Conexión PINE. Con tcp=(host, puerto) usa TCP (Windows: localhost:28011); si no, busca el socket Unix (o usa `path`)."""
    def __init__(self, path=None, tcp=None, slot=28011, timeout=5.0):
        if tcp is None and path is None and sys.platform.startswith('win'): tcp = ('127.0.0.1', slot)
        if tcp:
            self.where = f'tcp://{tcp[0]}:{tcp[1]}'; self.s = socket.create_connection(tcp, timeout=timeout)
        else:
            cands = [path] if path else candidate_sockets(slot); found = [p for p in cands if p and os.path.exists(p)]
            if not found: raise FileNotFoundError('no encuentro el socket PINE (¿PCSX2 abierto y PINE activado?). Probé: ' + ', '.join(cands))
            self.where = found[0]; self.s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM); self.s.settimeout(timeout); self.s.connect(found[0])
    def close(self):
        try: self.s.close()
        except OSError: pass
    def _recvn(self, n):
        b = bytearray()
        while len(b) < n:
            c = self.s.recv(n - len(b))
            if not c: raise ConnectionError('PINE cerró la conexión')
            b += c
        return bytes(b)
    def send(self, body):
        self.s.sendall(struct.pack('<I', len(body) + 4) + body)
        n, = struct.unpack('<I', self._recvn(4)); r = self._recvn(n - 4)
        if r[0] != 0: raise IOError(f'PINE: resultado {r[0]}')
        return r[1:]
    def _str(self, cmd):
        d = self.send(bytes([cmd])); n, = struct.unpack_from('<I', d); return d[4:4 + n].rstrip(b'\0').decode(errors='replace')
    def version(self): return self._str(VERSION)
    def title(self): return self._str(TITLE)
    def game_id(self): return self._str(ID)
    def save_state(self, slot): self.send(bytes([SAVE, slot]))        # PCSX2 escribe el savestate de esa ranura (asíncrono)
    def status(self): return struct.unpack('<I', self.send(bytes([STATUS])))[0]       # 0 corriendo, 1 pausado, 2 apagado
    def read_windows(self, windows, max_cmds=6000):
        """windows = [(dirección, longitud múltiplo de 8)] -> lista de bytes por ventana (Read64 en lotes, un paquete por lote)."""
        cmds = [(a + 8 * i, wi) for wi, (a, ln) in enumerate(windows) for i in range(ln // 8)]; out = [bytearray() for _ in windows]; k = 0
        while k < len(cmds):
            chunk = cmds[k:k + max_cmds]; d = self.send(b''.join(struct.pack('<BI', RD64, a) for a, _ in chunk))
            for j, (_, wi) in enumerate(chunk): out[wi] += d[8 * j:8 * j + 8]
            k += max_cmds
        return [bytes(o) for o in out]
