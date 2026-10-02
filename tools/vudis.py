#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Desensamblador de microcódigo VU1 (PS2). Uso: vudis.py in.bin [dirección_base_en_instrucciones] > salida.asm
Codificación según tablas públicas (documentación de VU / PCSX2). Cada instrucción = 64 bit: palabra baja = LOWER, alta = UPPER."""
import struct, sys
UP = ['ADDx','ADDy','ADDz','ADDw','SUBx','SUBy','SUBz','SUBw','MADDx','MADDy','MADDz','MADDw','MSUBx','MSUBy','MSUBz','MSUBw',
      'MAXx','MAXy','MAXz','MAXw','MINIx','MINIy','MINIz','MINIw','MULx','MULy','MULz','MULw','MULq','MAXi','MULi','MINIi',
      'ADDq','MADDq','ADDi','MADDi','SUBq','MSUBq','SUBi','MSUBi','ADD','MADD','MUL','MAX','SUB','MSUB','OPMSUB','MINI']
FD = [['ADDAx','SUBAx','MADDAx','MSUBAx','ITOF0','FTOI0','MULAx','MULAq','ADDAq','SUBAq','ADDA','SUBA'],
      ['ADDAy','SUBAy','MADDAy','MSUBAy','ITOF4','FTOI4','MULAy','ABS','MADDAq','MSUBAq','MADDA','MSUBA'],
      ['ADDAz','SUBAz','MADDAz','MSUBAz','ITOF12','FTOI12','MULAz','MULAi','ADDAi','SUBAi','MULA','OPMULA'],
      ['ADDAw','SUBAw','MADDAw','MSUBAw','ITOF15','FTOI15','MULAw','CLIP','MADDAi','MSUBAi',None,'NOP']]
T3 = [{0xc:'MOVE',0xd:'LQI',0xe:'DIV',0xf:'MTIR',0x10:'RNEXT',0x19:'MFP',0x1a:'XTOP',0x1b:'XGKICK',0x1c:'ESADD',0x1d:'EATANxy',0x1e:'ESQRT',0x1f:'ESIN'},
      {0xc:'MR32',0xd:'SQI',0xe:'SQRT',0xf:'MFIR',0x10:'RGET',0x1a:'XITOP',0x1c:'ERSADD',0x1d:'EATANxz',0x1e:'ERSQRT',0x1f:'EATAN'},
      {0xd:'LQD',0xe:'RSQRT',0xf:'ILWR',0x10:'RINIT',0x1c:'ELENG',0x1d:'ESUM',0x1e:'ERCPR',0x1f:'EEXP'},
      {0xd:'SQD',0xe:'WAITQ',0xf:'ISWR',0x10:'RXOR',0x1c:'ERLENG',0x1e:'WAITP'}]
LOW = {0:'LQ',1:'SQ',4:'ILW',5:'ISW',8:'IADDIU',9:'ISUBIU',0x10:'FCEQ',0x11:'FCSET',0x12:'FCAND',0x13:'FCOR',0x14:'FSEQ',0x15:'FSSET',
       0x16:'FSAND',0x17:'FSOR',0x18:'FMEQ',0x1a:'FMAND',0x1b:'FMOR',0x1c:'FCGET',0x20:'B',0x21:'BAL',0x24:'JR',0x25:'JALR',
       0x28:'IBEQ',0x29:'IBNE',0x2c:'IBLTZ',0x2d:'IBGTZ',0x2e:'IBLEZ',0x2f:'IBGEZ'}
LOWSP = {0x30:'IADD',0x31:'ISUB',0x32:'IADDI',0x34:'IAND',0x35:'IOR'}
def s(v, b): return v - (1 << b) if v & (1 << (b - 1)) else v
def dest(c): return ''.join(ch for ch, bit in zip('xyzw', (24, 23, 22, 21)) if c >> bit & 1)
def upper(c):
    op = c & 0x3f; fd, fs, ft = c >> 6 & 31, c >> 11 & 31, c >> 16 & 31; d = dest(c)
    if op >= 0x3c: n = FD[op & 3][c >> 6 & 31] if (c >> 6 & 31) < len(FD[op & 3]) else None
    else: n = UP[op] if op < len(UP) else None
    if n is None: return f'?U{c:08x}'
    if n == 'NOP': return 'NOP'
    br = n[-1] if n[-1] in 'xyzwiq' and n not in ('MAX','MINI') and n[-1] != 'A' else ''
    src = f'vf{ft}' + (br if br in 'xyzw' else '') if br in ('', 'x', 'y', 'z', 'w') else {'i': 'I', 'q': 'Q'}[br]
    if n.startswith(('ITOF', 'FTOI', 'ABS')): return f'{n}.{d} vf{ft}, vf{fs}'
    if n == 'CLIP': return f'CLIP vf{fs}xyz, vf{ft}w'
    if 'A' in n[:5] and n.endswith(('A','Ax','Ay','Az','Aw','Ai','Aq')) and n.startswith(('ADDA','SUBA','MULA','MADDA','MSUBA','OPMULA')):
        return f'{n}.{d} ACC, vf{fs}, {src}'
    if n.startswith(('MADD', 'MSUB')) and not n.startswith('OPMSUB'): return f'{n}.{d} vf{fd}, ACC, vf{fs}, {src}'
    return f'{n}.{d} vf{fd}, vf{fs}, {src}'
def lower(c, pc):
    top = c >> 25; fd, fs, ft = c >> 6 & 31, c >> 11 & 31, c >> 16 & 31; d = dest(c)
    im11, im15 = s(c & 0x7ff, 11), (c >> 10 & 0x7800) | (c & 0x7ff)
    if top == 0x40:
        op = c & 0x3f
        if op >= 0x3c:
            n = T3[op & 3].get(fd)
            if not n: return f'?L{c:08x}'
            if n in ('MOVE','MR32'): return f'{n}.{d} vf{ft}, vf{fs}'
            if n in ('LQI','LQD'): return f'{n}.{d} vf{ft}, (vi{fs}++)' if n == 'LQI' else f'{n}.{d} vf{ft}, (--vi{fs})'
            if n in ('SQI','SQD'): return f'{n}.{d} vf{fs}, (vi{ft}++)' if n == 'SQI' else f'{n}.{d} vf{fs}, (--vi{ft})'
            if n in ('MTIR',): return f'{n} vi{ft}, vf{fs}{"xyzw"[c >> 21 & 3]}'
            if n in ('MFIR',): return f'{n}.{d} vf{ft}, vi{fs}'
            if n in ('ILWR',): return f'{n}.{d} vi{ft}, (vi{fs})'
            if n in ('ISWR',): return f'{n}.{d} vi{ft}, (vi{fs})'
            if n == 'XGKICK': return f'XGKICK vi{fs}'
            if n in ('XTOP','XITOP'): return f'{n} vi{ft}'
            if n == 'DIV': return f'DIV Q, vf{fs}{"xyzw"[c >> 21 & 3]}, vf{ft}{"xyzw"[c >> 23 & 3]}'
            if n in ('SQRT',): return f'SQRT Q, vf{ft}{"xyzw"[c >> 23 & 3]}'
            if n == 'RSQRT': return f'RSQRT Q, vf{fs}{"xyzw"[c >> 21 & 3]}, vf{ft}{"xyzw"[c >> 23 & 3]}'
            return f'{n} vf{fs}, vf{ft}'
        n = LOWSP.get(op)
        if not n: return f'?L{c:08x}'
        if n == 'IADDI': return f'IADDI vi{fd}... vi{ft}, vi{fs}, {s(c >> 6 & 31, 5)}'
        return f'{n} vi{fd}, vi{fs}, vi{ft}'
    n = LOW.get(top)
    if not n: return f'?L{c:08x}'
    if n == 'LQ': return f'LQ.{d} vf{ft}, {im11}(vi{fs})'
    if n == 'SQ': return f'SQ.{d} vf{fs}, {im11}(vi{ft})'
    if n in ('ILW', 'ISW'): return f'{n}.{d} vi{ft}, {im11}(vi{fs})'
    if n in ('IADDIU', 'ISUBIU'): return f'{n} vi{ft}, vi{fs}, {im15}'
    if n == 'B' or n == 'BAL': return f'{n} ->{pc + 1 + im11:#x}' + (f' (vi{ft})' if n == 'BAL' else '')
    if n.startswith('IB'): return f'{n} vi{ft}, vi{fs}, ->{pc + 1 + im11:#x}'
    if n in ('JR', 'JALR'): return f'{n} vi{fs}'
    return f'{n} vi{ft}, vi{fs}, {c & 0xfff:#x}'
def main():
    d = open(sys.argv[1], 'rb').read(); base = int(sys.argv[2], 0) if len(sys.argv) > 2 else 0
    for i in range(len(d) // 8):
        lo, hi = struct.unpack_from('<II', d, 8 * i); pc = base + i
        fl = ('E' if hi >> 30 & 1 else '') + ('M' if hi >> 29 & 1 else '') + ('D' if hi >> 28 & 1 else '') + ('T' if hi >> 27 & 1 else '')
        lw = f'LOI {struct.unpack("<f", struct.pack("<I", lo))[0]:g}' if hi >> 31 else lower(lo, pc)
        print(f'{pc:04x}  {fl:<3} {upper(hi):<34} | {lw}')
if __name__ == '__main__': main()
