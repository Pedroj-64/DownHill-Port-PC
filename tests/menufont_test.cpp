// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Prueba sintética de src/menufont.hpp y tex::makeRtxTexture: textura T8H lineal con paleta conocida (incluido el intercambio de bits 3/4 y la regla de alfa PS2) y maquetación de texto (avance 0,65*qw,
// desplazamientos de letras estrechas/anchas, espacio). Sin datos del juego.
#include <cstdio>
#include <cmath>
#include "menufont.hpp"
static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FALLA %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)
int main() {
    // RTX sintético: un registro de 16 B {next=0, h, lo=id 2, hi = 512x512 fmt 27} y los datos a +0x80
    std::vector<uint8_t> rtx(0x80 + 512 * 512, 0); const uint32_t hdr[4] = {0, 0, 2, (9u << 8) | (9u << 12) | 27u}; std::memcpy(rtx.data(), hdr, 16);
    for (int y = 0; y < 512; y++) for (int x = 0; x < 512; x++) rtx[0x80 + (size_t)y * 512 + x] = (uint8_t)(x + 3 * y);
    std::vector<uint8_t> pal(1024); for (int i = 0; i < 256; i++) { pal[i * 4] = (uint8_t)i; pal[i * 4 + 1] = (uint8_t)(255 - i); pal[i * 4 + 2] = (uint8_t)(i / 2); pal[i * 4 + 3] = i < 128 ? (uint8_t)i : 0x80; }
    uint32_t W = 0, H = 0; std::vector<uint8_t> px; CHECK(tex::makeRtxTexture(rtx, 2, pal, W, H, px)); CHECK(W == 512 && H == 512 && px.size() == 512u * 512 * 4);
    const int x = 7, y = 11; const int p = (x + 3 * y) & 255; const int i = (p & ~0x18) | ((p & 8) << 1) | ((p & 16) >> 1);   // intercambio de los bits 3 y 4 de la CLUT de 8 bpp
    const uint8_t* o = &px[((size_t)y * 512 + x) * 4]; CHECK(o[0] == pal[i * 4] && o[1] == pal[i * 4 + 1] && o[2] == pal[i * 4 + 2]);
    CHECK(o[3] == (pal[i * 4 + 3] >= 0x80 ? 255 : pal[i * 4 + 3] * 2));
    CHECK(!tex::makeRtxTexture(rtx, 3, pal, W, H, px));                      // id inexistente
    // maquetación
    menufont::Font F; for (char c = 'A'; c <= 'Z'; c++) F.cell[c] = {(c - 'A') % 16, (c - 'A') / 16}; F.cell['I'] = {9, 3}; F.ok = true;
    const menufont::Style st; const float adv = 0.65f * st.qw;
    CHECK(std::fabs(menufont::width("S", st) - adv) < 1e-4f);                // letra normal: avance base
    CHECK(std::fabs(menufont::width("I", st) - (adv - 2.f)) < 1e-4f);        // estrecha: -1 px antes y -1 px después
    CHECK(std::fabs(menufont::width("N", st) - (adv + 1.25f)) < 1e-4f);      // ancha: +0,625 antes y después
    CHECK(std::fabs(menufont::width("S S", st) - 3 * adv) < 1e-4f);          // el espacio avanza adv
    std::vector<gfx::TexQuad> q; menufont::quads(F, "SIN", 100.f, 50.f, st, 2.f, 3.f, 1, 1, 1, 1, q); CHECK(q.size() == 3);
    // posición de la 2.ª letra = 100 + adv + s(I) (+ s(S)=0): 100 + adv - 1; escalada x2
    CHECK(std::fabs(q[1].x - (100.f + adv - 1.f) * 2.f) < 1e-3f); CHECK(std::fabs(q[1].y - 150.f) < 1e-4f);
    CHECK(std::fabs(q[1].w - st.qw * 2.f) < 1e-4f && std::fabs(q[1].h - st.qh * 3.f) < 1e-4f);
    CHECK(std::fabs(q[1].u0 - (9 * 32.f + 1.f) / 512.f) < 1e-6f && std::fabs(q[1].v0 - (3 * 32.f + 1.f) / 512.f) < 1e-6f);   // celda (9, 3) = «I» del atlas real
    CHECK(std::fabs(q[1].u1 - q[1].u0 - 32.f / 512.f) < 1e-6f);
    std::vector<gfx::TexQuad> none; menufont::quads(F, "?", 0, 0, st, 1, 1, 1, 1, 1, 1, none); CHECK(none.empty());   // carácter sin celda: no se dibuja
    std::printf(fails ? "menufont_test: %d fallos\n" : "menufont_test OK\n", fails); return fails ? 1 : 0;
}
