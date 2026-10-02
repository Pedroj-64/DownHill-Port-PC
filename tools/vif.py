# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Decodificador mínimo de paquetes VIF1 (PS2). iter_vif(data, off, end) -> (offset, nombre, imm, num, payload_bytes)."""
import struct
FMT = {0x0:('S-32',1,32),0x1:('S-16',1,16),0x2:('S-8',1,8),0x4:('V2-32',2,32),0x5:('V2-16',2,16),0x6:('V2-8',2,8),
       0x8:('V3-32',3,32),0x9:('V3-16',3,16),0xA:('V3-8',3,8),0xC:('V4-32',4,32),0xD:('V4-16',4,16),0xE:('V4-8',4,8),0xF:('V4-5',1,16)}
NAMES = {0x00:'NOP',0x01:'STCYCL',0x02:'OFFSET',0x03:'BASE',0x04:'ITOP',0x05:'STMOD',0x06:'MSKPATH3',0x07:'MARK',0x10:'FLUSHE',
         0x11:'FLUSH',0x13:'FLUSHA',0x14:'MSCAL',0x15:'MSCALF',0x17:'MSCNT',0x20:'STMASK',0x30:'STROW',0x31:'STCOL',0x4a:'MPG',0x50:'DIRECT',0x51:'DIRECTHL'}
def iter_vif(d, off, end):
    while off + 4 <= end:
        w = struct.unpack_from('<I', d, off)[0]; imm, num, cmd = w & 0xffff, (w >> 16) & 0xff, (w >> 24) & 0x7f
        if cmd >= 0x60:
            nm, comps, bits = FMT.get(cmd & 0xf, ('?', 0, 0))
            size = (num * comps * bits + 31) // 32 * 4 if comps else 0
            yield off, 'UNPACK ' + nm, imm, num, size
            off += 4 + size
        else:
            size = {0x20:4, 0x30:16, 0x31:16}.get(cmd, 0)
            if cmd == 0x4a: size = num * 8
            if cmd in (0x50, 0x51): size = (imm or 65536) * 16
            yield off, NAMES.get(cmd, f'?{cmd:#x}'), imm, num, size
            off += 4 + size
