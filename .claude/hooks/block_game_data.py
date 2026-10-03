#!/usr/bin/env python3
"""PreToolUse (Write|Edit): bloquea datos del juego. Solo stdlib. Exit 2 = bloquear."""
import json
import re
import sys

EXT = re.compile(r'\.(iso|bin|cue|img|elf|irx|mdl|msh|pts|bmp|ngp|ptr|rtx|tex|rst|bnk|vpk|vag|pss|orb|rep|nga|skx|ctl|ico|gpr|pem|key)$'
                 r'|(^|/)(SLES|SCES|SLUS)_|(^|/)(iso_extract|unpacked|decomp|ghidra_projects|assets)/|\.env$', re.I)
GHIDRA = [re.compile(p) for p in (r'undefined[1248]?\b', r'\b[iu]Var\d+\b', r'\bin_(v0|a0)\b', r'/\* WARNING:', r'\bunaff_')]
MAX = 1 << 20


def main():
    try:
        d = json.load(sys.stdin)
    except Exception:
        return 0
    ti = d.get('tool_input') or {}
    path = ti.get('file_path') or ''
    text = ti.get('content') or ti.get('new_string') or ''
    why = None
    if EXT.search(path):
        why = f'ruta de datos del juego o secreto: {path}'
    elif '\0' in text:
        why = 'contenido con bytes NUL (binario)'
    elif len(text.encode('utf-8', 'replace')) > MAX:
        why = 'contenido > 1 MiB'
    else:
        n = sum(1 for g in GHIDRA if g.search(text))
        if n >= 2:
            why = f'parece salida del decompilador Ghidra ({n} firmas); cita solo FUN_xxxxxxxx'
    if why:
        print(f'BLOQUEADO por block_game_data.py: {why}', file=sys.stderr)
        return 2
    return 0


sys.exit(main())
