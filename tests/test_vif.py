#!/usr/bin/env python3
# Author: Brandon Gil
"""Pruebas de los tamaños y nombres básicos del decodificador VIF."""
import struct
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parents[1] / "tools"))

from vif import FMT, iter_vif


def packet(command, num=0, immediate=0):
    """Construye una cabecera VIF sintética en little-endian."""
    return struct.pack("<I", (command << 24) | (num << 16) | immediate)


class IterVifTests(unittest.TestCase):
    """Comprueba tamaños, cuentas y offsets del iterador VIF."""

    def test_num_zero_means_256_for_unpack(self):
        """Interpreta NUM=0 como 256 vectores para UNPACK."""
        data = packet(0x68, num=0) + bytes(256 * 12)

        self.assertEqual(list(iter_vif(data, 0, len(data))), [(0, "UNPACK V3-32", 0, 256, 3072)])

    def test_num_zero_means_256_for_mpg(self):
        """Interpreta NUM=0 como 256 microinstrucciones para MPG."""
        data = packet(0x4A, num=0) + bytes(256 * 8)

        self.assertEqual(list(iter_vif(data, 0, len(data))), [(0, "MPG", 0, 256, 2048)])

    def test_decodes_every_unpack_format(self):
        """Calcula correctamente el tamaño de cada formato UNPACK."""
        for code, (name, components, bits) in FMT.items():
            with self.subTest(code=hex(code)):
                count = 3
                size = (count * components * bits + 31) // 32 * 4
                data = packet(0x60 | code, num=count) + bytes(size)

                self.assertEqual(
                    list(iter_vif(data, 0, len(data))),
                    [(0, f"UNPACK {name}", 0, count, size)],
                )

    def test_decodes_scalar_command_and_payload_size(self):
        """Decodifica un comando escalar y avanza sobre su payload."""
        data = packet(0x20, immediate=0x1234) + b"ABCD" + packet(0x00)

        result = list(iter_vif(data, 0, len(data)))

        self.assertEqual(result[0], (0, "STMASK", 0x1234, 0, 4))
        self.assertEqual(result[1], (8, "NOP", 0, 0, 0))

    def test_decodes_unpack_size_from_components(self):
        """Calcula el tamaño de un bloque V3-32 de dos vectores."""
        data = packet(0x68, num=2, immediate=0x3FF) + bytes(24)

        result = list(iter_vif(data, 0, len(data)))

        self.assertEqual(result, [(0, "UNPACK V3-32", 0x3FF, 2, 24)])

    def test_decodes_mpg_and_direct_variable_payloads(self):
        """Distingue las unidades de 64 bits de MPG y 128 bits de DIRECT."""
        mpg = packet(0x4A, num=2) + bytes(16)
        direct = packet(0x50, immediate=3) + bytes(48)

        self.assertEqual(list(iter_vif(mpg, 0, len(mpg))), [(0, "MPG", 0, 2, 16)])
        self.assertEqual(list(iter_vif(direct, 0, len(direct))), [(0, "DIRECT", 3, 0, 48)])

    def test_decodes_fixed_payload_commands(self):
        """Decodifica los payloads fijos de STROW y STCOL."""
        data = packet(0x30) + bytes(16) + packet(0x31) + bytes(16) + packet(0x00)

        self.assertEqual(
            list(iter_vif(data, 0, len(data))),
            [
                (0, "STROW", 0, 0, 16),
                (20, "STCOL", 0, 0, 16),
                (40, "NOP", 0, 0, 0),
            ],
        )

    def test_starts_at_nonzero_offset(self):
        """Permite comenzar a leer un paquete dentro de un buffer mayor."""
        data = b"prefix" + packet(0x20, immediate=7) + bytes(4)

        self.assertEqual(list(iter_vif(data, 6, len(data))), [(6, "STMASK", 7, 0, 4)])

    def test_preserves_unpack_immediate_and_num(self):
        """Conserva el inmediato y la cuenta de un bloque V4-16."""
        data = packet(0x6D, num=5, immediate=0x155) + bytes(5 * 4 * 16 // 8)

        self.assertEqual(list(iter_vif(data, 0, len(data))), [(0, "UNPACK V4-16", 0x155, 5, 40)])

    def test_stops_at_end_offset(self):
        """No lee cabeceras posteriores al límite solicitado."""
        data = packet(0x00) + packet(0x00)

        result = list(iter_vif(data, 0, 4))

        self.assertEqual(result, [(0, "NOP", 0, 0, 0)])

    def test_keeps_unknown_command_name(self):
        """Expone los comandos desconocidos sin descartarlos silenciosamente."""
        data = packet(0x22)

        result = list(iter_vif(data, 0, len(data)))

        self.assertEqual(result, [(0, "?0x22", 0, 0, 0)])


if __name__ == "__main__":
    unittest.main()