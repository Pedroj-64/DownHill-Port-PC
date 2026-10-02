#!/usr/bin/env python3
# Author: Brandon Gil
import struct
import sys
import unittest
import zlib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parents[1] / "tools"))

from unpack_ie import unpack_ie_data


def container(variant, payload, name=b"file.bin"):
    """Construye un contenedor IE sintético comprimido o anidado."""
    header = bytearray(0x1e)
    struct.pack_into("<I", header, 0, 0x04034549)
    struct.pack_into("<I", header, 4, variant)
    struct.pack_into("<I", header, 0x1a, len(name))
    body = payload
    if variant != 0x0a:
        body = zlib.compress(payload, wbits=-15)
    return bytes(header) + name + body


class UnpackIeDataTests(unittest.TestCase):
    """Comprueba datos válidos, corrupción y límites de contenedores IE."""

    def test_unpacks_standard_container(self):
        """Descomprime una capa IE estándar con deflate crudo."""
        self.assertEqual(unpack_ie_data(container(0x14, b"payload")), b"payload")

    def test_unpacks_nested_variant_0a_containers(self):
        """Desenvuelve varias capas alternadas 0x0a y deflate."""
        data = container(0x0a, container(0x14, container(0x0a, container(0x14, b"payload"))))

        self.assertEqual(unpack_ie_data(data), b"payload")

    def test_returns_non_ie_data_unchanged(self):
        """Devuelve intactos los datos que no empiezan por la firma IE."""
        self.assertEqual(unpack_ie_data(b"plain data"), b"plain data")

    def test_supports_empty_payload_and_long_name(self):
        """Acepta payload vacío y nombres de 128 bytes."""
        self.assertEqual(unpack_ie_data(container(0x14, b"", b"x" * 128)), b"")

    def test_rejects_truncated_header(self):
        """Rechaza una firma IE sin la cabecera completa."""
        with self.assertRaisesRegex(ValueError, "cabecera IE truncada"):
            unpack_ie_data(b"IE\x03\x04")

    def test_rejects_truncated_name(self):
        """Rechaza una cabecera cuyo nombre excede el buffer."""
        data = bytearray(0x1e)
        struct.pack_into("<I", data, 0, 0x04034549)
        struct.pack_into("<I", data, 4, 0x0A)
        struct.pack_into("<I", data, 0x1A, 8)

        with self.assertRaisesRegex(ValueError, "nombre IE truncado"):
            unpack_ie_data(bytes(data))

    def test_rejects_invalid_deflate(self):
        """Propaga el error cuando el payload deflate está corrupto."""
        data = bytearray(container(0x14, b"payload"))
        data[-1] ^= 0xFF

        with self.assertRaises(zlib.error):
            unpack_ie_data(bytes(data))

    def test_rejects_excessive_nesting(self):
        """Impide recorrer indefinidamente capas IE anidadas."""
        data = container(0x0A, b"payload")

        with self.assertRaisesRegex(ValueError, "demasiadas capas"):
            unpack_ie_data(data, max_layers=1)


if __name__ == "__main__":
    unittest.main()