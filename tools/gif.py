#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
# Author: Brandon Gil (GIF parser and tests)
"""Parser sintético de GIFtags y payloads GIF de PlayStation 2."""
from dataclasses import dataclass
import struct

GIFTAG_SIZE = 16
QWORD_SIZE = 16
PACKED = 0
REGLIST = 1
IMAGE = 2

REGISTER_NAMES = {
    0x00: "PRIM",
    0x01: "RGBAQ",
    0x02: "ST",
    0x03: "UV",
    0x04: "XYZF2",
    0x05: "XYZ2",
    0x06: "TEX0_1",
    0x07: "TEX0_2",
    0x08: "CLAMP_1",
    0x09: "CLAMP_2",
    0x0A: "FOG",
    0x0C: "XYZF3",
    0x0D: "XYZ3",
    0x0E: "A_D",
    0x0F: "NOP",
}


@dataclass(frozen=True)
class GifTag:
    """Describe los campos decodificados de una cabecera GIFtag."""

    offset: int
    nloop: int
    eop: bool
    pre: bool
    prim: int
    flg: int
    nreg_field: int
    registers: tuple[int, ...]

    @property
    def nreg(self):
        """Devuelve el número efectivo de registros, donde cero significa 16."""
        return self.nreg_field or 16

    @property
    def mode_name(self):
        """Devuelve el nombre del modo GIF o ``UNKNOWN`` para valores reservados."""
        return {PACKED: "PACKED", REGLIST: "REGLIST", IMAGE: "IMAGE"}.get(self.flg, "UNKNOWN")

    @property
    def register_names(self):
        """Devuelve nombres GS para los descriptores registrados en el GIFtag."""
        return tuple(REGISTER_NAMES.get(register, f"REG_{register:#x}") for register in self.registers[:self.nreg])

    @property
    def payload_qwords(self):
        """Calcula cuántos qwords siguen a esta cabecera GIFtag."""
        if self.flg == PACKED:
            return self.nloop * self.nreg
        if self.flg == REGLIST:
            return self.nloop * ((self.nreg + 1) // 2)
        if self.flg == IMAGE:
            return self.nloop
        raise ValueError(f"modo GIF reservado: {self.flg}")

    @property
    def payload_size(self):
        """Devuelve el tamaño del payload en bytes."""
        return self.payload_qwords * QWORD_SIZE


@dataclass(frozen=True)
class GifPacket:
    """Representa un GIFtag junto con el payload que describe."""

    tag: GifTag
    payload_offset: int
    payload: bytes


def parse_giftag(data, offset=0):
    """Decodifica un GIFtag de 128 bits desde ``offset``."""
    if offset < 0 or offset + GIFTAG_SIZE > len(data):
        raise ValueError("GIFtag truncado")
    low, high = struct.unpack_from("<QQ", data, offset)
    flg = (low >> 58) & 0x3
    tag = GifTag(
        offset=offset,
        nloop=low & 0x7FFF,
        eop=bool((low >> 15) & 1),
        pre=bool((low >> 46) & 1),
        prim=(low >> 47) & 0x7FF,
        flg=flg,
        nreg_field=(low >> 60) & 0xF,
        registers=tuple((high >> (4 * index)) & 0xF for index in range(16)),
    )
    if flg == 3:
        raise ValueError("modo GIF reservado: 3")
    return tag


def iter_gif_packets(data, offset=0, end=None):
    """Itera GIFtags consecutivos y valida el tamaño de cada payload."""
    end = len(data) if end is None else end
    if offset < 0 or end < offset or end > len(data):
        raise ValueError("rango GIF inválido")
    while offset < end:
        tag = parse_giftag(data, offset)
        payload_offset = offset + GIFTAG_SIZE
        payload_end = payload_offset + tag.payload_size
        if payload_end > end:
            raise ValueError("payload GIF truncado")
        yield GifPacket(tag, payload_offset, data[payload_offset:payload_end])
        offset = payload_end