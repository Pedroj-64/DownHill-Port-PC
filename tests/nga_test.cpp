// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// .NGA sintético: un clip con una pista constante (3,0) y una de muestras u8 (2,2) con interpolación lineal; archivo truncado rechazado.
#include "../src/nga.hpp"
#include <cstdio>
int main() {
    // disposición: [pista1 (cabecera cuantizada 16 B + registro)] [pista0 constante] [cabecera de clip]; las pistas están ANTES de la cabecera (nga.md)
    std::vector<uint8_t> b(0x200, 0); auto w = [&](size_t o, const void* p, size_t n) { std::memcpy(&b[o], p, n); };
    uint32_t magic = (1u << 16) | 6040; w(0, &magic, 4); uint16_t id = 7, pad = 0; uint32_t off = 0x100; w(4, &id, 2); w(6, &pad, 2); w(8, &off, 4);
    // clip en 0x100: u16 12, u16 id, u32 0, f32 dur, u16 nt = 2, u16 slot, counts[2]
    uint16_t twelve = 12, nt = 2; float dur = 10; w(0x100, &twelve, 2); w(0x102, &id, 2); w(0x108, &dur, 4); w(0x10c, &nt, 2);
    uint16_t c0 = 2, c1 = 8; w(0x112, &c0, 2); w(0x114, &c1, 2);               // pista0 = 0x100 - 8 = 0xf8 (8 B); pista1 = 0xf8 - 32 = 0xd8 (32 B incl. cabecera de 16 B en 0xc8... la de pista1 está en pista1-0x10 = 0xc8)
    uint16_t fl0 = 3, ch0 = 5; float v0 = 2.5f; w(0xf8, &fl0, 2); w(0xfa, &ch0, 2); w(0xfc, &v0, 4);
    float hdr[4] = {0, 1, 10, 0.5f}; w(0xc8, hdr, 16);                          // t0=0, dt=1, v0=10, dv=0.5
    uint16_t fl1 = 2 | (2 << 3), ch1 = 9, n1 = 3; w(0xd8, &fl1, 2); w(0xda, &ch1, 2); w(0xdc, &n1, 2); b[0xde] = 0; b[0xdf] = 4; b[0xe0] = 8;   // valores 10, 12, 14
    nga::File f; if (!f.load(b) || f.clips.size() != 1) { std::puts("FAIL load"); return 1; }
    std::vector<float> p(16, -1.f); nga::File::pose(f.clips[0], 0.5f, p);
    if (std::fabs(p[5] - 2.5f) > 1e-6f || std::fabs(p[9] - 11.f) > 1e-5f) { std::printf("FAIL values %f %f\n", p[5], p[9]); return 1; }
    nga::File::pose(f.clips[0], 99.f, p); if (std::fabs(p[9] - 14.f) > 1e-5f) { std::puts("FAIL clamp"); return 1; }
    std::vector<uint8_t> cut(b.begin(), b.begin() + 0x108); nga::File g; if (g.load(cut) && g.clips.size()) { std::puts("FAIL truncated"); return 1; }
    std::puts("nga_test OK"); return 0;
}
