// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Texto provisional (src/text.hpp): todas las cadenas de la interfaz tienen glifo, el ancho cuadra, 'O' y '0' se distinguen y el dibujo de una letra se reconoce.
#include "../src/text.hpp"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#define CHECK(c) do { if (!(c)) { std::printf("FALLO %s:%d %s\n", __FILE__, __LINE__, #c); std::exit(1); } } while (0)
int main() {
    const char* all[] = {"RACE RESULTS", "TIME TRIAL RESULTS", "GREAT FINISH!", "PLAYER 1 PAUSED", "07:10.98", "122 KM/H", "GATES 28/28", "TOP SPEED", "RESETS 0", "00:03.21", "time trial results"};
    for (const char* s : all) for (const char* p = s; *p; p++) CHECK(gfx::font5x7::find(*p) != nullptr);
    CHECK(gfx::font5x7::find('~') == nullptr);
    CHECK(gfx::textWidth("AB", 2.f) == 22.f && gfx::textWidth("", 2.f) == 0.f);
    std::vector<gfx::Quad> q; gfx::textQuads("ab", 10.f, 20.f, 3.f, 1, 1, 1, 1, q); CHECK(!q.empty());
    for (const auto& r : q) CHECK(r.x >= 10.f && r.x + r.w <= 10.f + 11 * 3.f + 1e-3f && r.y >= 20.f && r.y + r.h <= 20.f + 7 * 3.f + 1e-3f && r.w > 0 && r.h > 0);
    std::vector<gfx::Quad> o, z; gfx::textQuads("O", 0, 0, 1, 1, 1, 1, 1, o); gfx::textQuads("0", 0, 0, 1, 1, 1, 1, 1, z); CHECK(o.size() != z.size() || std::memcmp(o.data(), z.data(), o.size() * sizeof(gfx::Quad)) != 0);
    // reconocimiento: la «T» tiene la barra superior completa y un solo rectángulo por fila debajo
    std::vector<gfx::Quad> t; gfx::textQuads("T", 0, 0, 1, 1, 1, 1, 1, t); CHECK(t.size() == 7 && t[0].w == 5.f && t[1].w == 1.f && t[1].x == 2.f);
    std::puts("text_test OK"); return 0;
}
