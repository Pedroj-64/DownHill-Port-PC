#!/usr/bin/env python3
# Author: Brandon Gil
"""Pruebas sintéticas del parser GIFtag."""
import struct
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parents[1] / "tools"))

from gif import IMAGE, PACKED, REGLIST, iter_gif_packets, parse_giftag


def giftag(nloop=1, eop=False, pre=False, prim=0, flg=PACKED, nreg=1, registers=(0x0F,)):
    """Construye un GIFtag sintético con registros en orden de lectura."""
    low = nloop | (int(eop) << 15) | (int(pre) << 46) | (prim << 47) | (flg << 58) | (nreg << 60)
    high = sum(register << (4 * index) for index, register in enumerate(registers))
    return struct.pack("<QQ", low, high)


class GifParserTests(unittest.TestCase):
    """Comprueba campos, modos y límites de los GIFtags."""

    def test_decodes_control_fields_and_registers(self):
        """Extrae los campos de control y los nombres GS conocidos."""
        tag = parse_giftag(giftag(nloop=3, eop=True, pre=True, prim=0x155, nreg=2, registers=(1, 5)))

        self.assertEqual((tag.nloop, tag.eop, tag.pre, tag.prim), (3, True, True, 0x155))
        self.assertEqual(tag.register_names, ("RGBAQ", "XYZ2"))

    def test_nreg_zero_means_sixteen_registers(self):
        """Interpreta NREG=0 como 16 descriptores."""
        registers = tuple(range(16))
        tag = parse_giftag(giftag(nloop=2, nreg=0, registers=registers))

        self.assertEqual(tag.nreg, 16)
        self.assertEqual(tag.registers, registers)
        self.assertEqual(tag.payload_qwords, 32)

    def test_calculates_packed_payload(self):
        """Calcula un qword por registro en modo PACKED."""
        tag = parse_giftag(giftag(nloop=3, nreg=4, registers=(1, 2, 3, 5)))

        self.assertEqual(tag.mode_name, "PACKED")
        self.assertEqual(tag.payload_size, 3 * 4 * 16)

    def test_calculates_reglist_payload_with_odd_register_count(self):
        """Redondea dos registros de 64 bits por qword en REGLIST."""
        tag = parse_giftag(giftag(nloop=3, flg=REGLIST, nreg=3, registers=(1, 2, 5)))

        self.assertEqual(tag.payload_qwords, 3 * 2)

    def test_calculates_image_payload(self):
        """Calcula un qword por iteración en modo IMAGE."""
        tag = parse_giftag(giftag(nloop=7, flg=IMAGE, nreg=0, registers=()))

        self.assertEqual(tag.payload_size, 7 * 16)

    def test_iterates_packets_and_reports_payload_offset(self):
        """Consume dos GIFtags consecutivos y conserva sus offsets."""
        first = giftag(nloop=1, nreg=1) + bytes(16)
        second = giftag(nloop=2, nreg=1) + bytes(32)

        packets = list(iter_gif_packets(first + second))

        self.assertEqual([packet.tag.offset for packet in packets], [0, 32])
        self.assertEqual([len(packet.payload) for packet in packets], [16, 32])

    def test_rejects_truncated_header(self):
        """Rechaza un GIFtag cuyo encabezado no tiene 16 bytes."""
        with self.assertRaisesRegex(ValueError, "GIFtag truncado"):
            parse_giftag(bytes(15))

    def test_rejects_truncated_payload(self):
        """Rechaza un payload menor que el tamaño declarado por NLOOP/NREG."""
        data = giftag(nloop=2, nreg=1) + bytes(16)

        with self.assertRaisesRegex(ValueError, "payload GIF truncado"):
            list(iter_gif_packets(data))

    def test_rejects_reserved_mode(self):
        """Rechaza FLG=3, que no es un modo GIF válido."""
        with self.assertRaisesRegex(ValueError, "modo GIF reservado"):
            parse_giftag(giftag(flg=3))


if __name__ == "__main__":
    unittest.main()