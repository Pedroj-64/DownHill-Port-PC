# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Mensajes bilingües (idioma del sistema). / Bilingual messages (system language)."""
import os

ES = (os.environ.get('LC_ALL') or os.environ.get('LC_MESSAGES') or os.environ.get('LANG') or '').lower().startswith('es')
if os.name == 'nt' and not os.environ.get('LANG'):                       # Windows: mira el idioma de la interfaz
    try:
        import ctypes
        ES = ctypes.windll.kernel32.GetUserDefaultUILanguage() & 0x3FF == 0x0A          # 0x0A = español
    except Exception: pass

def tr(es, en): return es if ES else en
def say(es, en=None): print(es if ES or en is None else en, flush=True)
