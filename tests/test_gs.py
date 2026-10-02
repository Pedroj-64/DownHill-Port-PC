#!/usr/bin/env python3
# Author: Brandon Gil
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parents[1] / "tools"))

from gs import addr8, addr32, upload32


class GsAddressTests(unittest.TestCase):
    """Comprueba propiedades de las tablas públicas de direccionamiento GS."""

    def test_ct32_block_addresses_are_unique(self):
        """Comprueba que un bloque CT32 no colisiona ni deja huecos."""
        width, height = 64, 32
        addresses = {addr32(x, y, width) for y in range(height) for x in range(width)}

        self.assertEqual(len(addresses), width * height)
        self.assertEqual(addresses, set(range(width * height)))

    def test_t8_block_addresses_are_unique(self):
        """Comprueba que un bloque T8 no colisiona ni deja huecos."""
        width, height = 128, 64
        addresses = {addr8(x, y, width) for y in range(height) for x in range(width)}

        self.assertEqual(len(addresses), width * height)
        self.assertEqual(addresses, set(range(width * height)))

    def test_upload32_places_each_pixel_at_its_gs_address(self):
        """Verifica que upload32 conserva cada pixel en su dirección GS."""
        width, height = 64, 32
        data = bytes(value % 256 for value in range(width * height * 4))
        memory = upload32(data, width, height)

        for y in range(height):
            for x in range(width):
                source = (y * width + x) * 4
                address = addr32(x, y, width) * 4
                self.assertEqual(memory[address:address + 4], data[source:source + 4])


if __name__ == "__main__":
    unittest.main()