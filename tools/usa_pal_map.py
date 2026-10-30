#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Mapa de funciones USA (downhill-revived) -> PAL (nuestro ELF) / USA->PAL function map.
Uso / usage: usa_pal_map.py SLES_522.02 downhill-revived/progress/report.json [salida.json]
ES. Alinea por tamaño la lista de funciones USA (report.json de downhill-revived: func_XXXXXXXX + tamaño)
con las funciones detectadas en el ELF PAL (destinos de jal y prólogos tras `jr ra`). Reproduce las 7
equivalencias verificadas a mano (bloque de física, USA = PAL - 0x2F8) y cubre ~3650 de 4884 funciones.
Las direcciones fuera de ese mapa siguen siendo desconocidas. Una equivalencia es HIPÓTESIS hasta comprobar
el contenido (p. ej. la tabla de saltos de 316 entradas de las funciones de sonido de superficie).
EN. Aligns USA function sizes with PAL function starts; reproduces the 7 hand-verified pairs.
"""
import json,struct,re,difflib,sys
ELF=sys.argv[1]; REP=sys.argv[2]; OUT=sys.argv[3] if len(sys.argv)>3 else 'usa_pal.json'
d=open(ELF,'rb').read()
phoff=struct.unpack_from('<I',d,0x1C)[0]; phnum=struct.unpack_from('<H',d,0x2C)[0]; phes=struct.unpack_from('<H',d,0x2A)[0]
entry=struct.unpack_from('<I',d,0x18)[0]
segs=[struct.unpack_from('<8I',d,phoff+i*phes) for i in range(phnum)]
segs=[s for s in segs if s[0]==1]
print('entry',hex(entry),[(hex(s[2]),hex(s[4])) for s in segs],file=sys.stderr)
# .text = first exec segment
tx=[s for s in segs if s[6]&1][0]
off,va,fs=tx[1],tx[2],tx[4]
words=struct.unpack_from('<%dI'%(fs//4),d,off)
# USA functions
rep=json.load(open(REP))
usa=[]
for u in rep['units']:
    for f in u.get('functions',[]):
        m=re.match(r'func_([0-9A-F]{8})',f['name'])
        if m and 'size' in f: usa.append((int(m.group(1),16),int(f['size'])))
usa=sorted(set(usa))
usa=[x for x in usa if x[0]>=0x10A000 and x[0]<0x244000]
print('usa funcs',len(usa),file=sys.stderr)
# PAL starts
n=len(words)
jal=set()
for i,w in enumerate(words):
    if w>>26==3: jal.add(((w&0x3FFFFFF)<<2))
starts=set(a for a in jal if va<=a<va+fs)
for i in range(n-2):
    if words[i]==0x03E00008:
        j=i+2
        while j<n and words[j]==0: j+=1
        if j<n and (words[j]>>16)==0x27BD and (words[j]&0x8000): starts.add(va+4*j)  # addiu sp,sp,-N
# add prologue starts regardless
starts=sorted(starts)
pal=[(a,(starts[i+1] if i+1<len(starts) else va+fs)-a) for i,a in enumerate(starts)]
print('pal starts',len(pal),file=sys.stderr)
# align by size sequences (strip trailing nop padding from PAL size)
def trim(a,s):
    k=s//4
    while k>0 and words[(a-va)//4+k-1]==0: k-=1
    return k*4
# USA sizes in report likely include padding? compare both
ps=[trim(a,s) for a,s in pal]; us=[s for a,s in usa]
sm=difflib.SequenceMatcher(None,us,ps,autojunk=False)
mp={}
for t,i1,i2,j1,j2 in sm.get_opcodes():
    if t=='equal':
        for k in range(i2-i1): mp[usa[i1+k][0]]=pal[j1+k][0]
print('mapped',len(mp),file=sys.stderr)
json.dump({hex(k):hex(v) for k,v in mp.items()},open(OUT,'w'))
chk={0x135A30:0x135D28,0x136038:0x136330,0x136790:0x136A88,0x133D68:0x134060,0x131998:0x131C90,0x132CE0:0x132FD8,0x138560:0x138858}
for k,v in chk.items(): print(hex(k),'->',hex(mp.get(k,0)),'expected',hex(v),'OK' if mp.get(k)==v else 'no')
