#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Sondas sobre la RAM EE de un savestate (se pasa la ruta de eeMemory.bin; los volcados NO van al repo) para localizar la pose viva del cuerpo del piloto
(docs/formats/rider-live.md). Resultado actual: NO localizada; estas funciones documentan el método y descartan pistas.
Uso: rider_live.py eeMemory.bin tablas NGP_resident_base raíz_tablas_hex...   -> estado en RAM de las tablas de articulación (euler en +0x20, máscara en +0xc1)
     rider_live.py eeMemory.bin locrows                                       -> filas (x,y,z,1) de 'loc' de huesos del piloto fuera de la imagen del NGP (matrices del recorrido)"""
import struct, sys
import numpy as np

B = 0xA00000   # base de carga del NGP del nivel en RAM (ngp.hpp kBase)
LOCS = {'shR': (0.54, -0.077, 0.521), 'hipR': (-0.369, -0.083, -0.415), 'knee': (0.0, 0.121, -1.385), 'head': (0.0, 0.0, 0.9)}   # rig de pilotos (rider.md)

def loc_rows(ee, skip=(0x1300000, 0x1500000), tol=3e-3):
    """Direcciones de qwords (x,y,z,1.0) = 'loc' de huesos del piloto, excluyendo la imagen residente del NGP. Si el cuerpo se evaluara por CPU con matrices
    (FUN_00227da8 + suma de loc en la fila 3), aparecerían aquí; en los savestates 01/05 no aparece ninguna."""
    F = np.frombuffer(ee[:len(ee) // 16 * 16], '<f4').reshape(-1, 4); out = {}
    for nm, l in LOCS.items():
        with np.errstate(all='ignore'):
            m = np.where((np.abs(F[:, 0] - l[0]) < tol) & (np.abs(F[:, 1] - l[1]) < tol) & (np.abs(F[:, 2] - l[2]) < tol) & (np.abs(F[:, 3] - 1) < 1e-6))[0] * 16
        out[nm] = [int(x) for x in m if not skip[0] <= x < skip[1]]
    return out

def table_state(ee, table_off):
    """Estado en RAM de una tabla de articulación del NGP residente: (euler xyz en +0x20, flags u32 en +0xc0). Los flags en RAM tienen la máscara de canales
    en el byte +0xc1 (0x07 / 0x01 / 0x3f) frente a 0x03 / 0x01 / 0x1f en el archivo: el caso 0x11 de FUN_0020edb8 SÍ se ejecutó sobre estas tablas, pero con euler = 0."""
    a = B + table_off
    return struct.unpack_from('<3f', ee, a + 0x20), struct.unpack_from('<I', ee, a + 0xc0)[0]

if __name__ == '__main__':
    if len(sys.argv) > 2 and sys.argv[2] == 'selftest' or len(sys.argv) == 1:
        ee = bytearray(0x2000); struct.pack_into('<4f', ee, 0x100, 0.54, -0.077, 0.521, 1.0)
        assert loc_rows(bytes(ee))['shR'] == [0x100] and loc_rows(bytes(ee))['hipR'] == []
        struct.pack_into('<3fI', ee, B * 0 + 0x20, 1, 2, 3, 0); print('rider_live selftest OK'); sys.exit(0)
    ee = open(sys.argv[1], 'rb').read()
    if sys.argv[2] == 'locrows':
        for k, v in loc_rows(ee).items(): print(k, [hex(x) for x in v])
    elif sys.argv[2] == 'tablas':
        for t in sys.argv[3:]: print(t, table_state(ee, int(t, 0)))
