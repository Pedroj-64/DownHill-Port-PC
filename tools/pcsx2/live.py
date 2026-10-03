# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Utilidades de sesión PINE en vivo (PCSX2 ya abierto): conectar, leer/escribir RAM del juego. Sin datos del juego."""
import os, struct, sys
sys.path.insert(0, os.path.dirname(__file__)); sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
import pine
from p2s import RIDER_BASE, RIDER_STRIDE, RIDER_COUNT_ADDR, PHYS_OFF

def connect(): return pine.Pine(pine.default_socket())
def rd(p, addr, n): return p.read_windows([(addr, n)])[0]            # n múltiplo de 8
WR32 = 6   # PINE MsgWrite32
def wr32(p, addr, v): p.send(struct.pack('<BII', WR32, addr, v & 0xFFFFFFFF))
def wr_f32(p, addr, f): wr32(p, addr, struct.unpack('<I', struct.pack('<f', f))[0])
def rider(i): return RIDER_BASE + i * RIDER_STRIDE
