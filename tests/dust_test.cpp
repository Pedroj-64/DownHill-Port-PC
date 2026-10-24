// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Polvo (src/dust.hpp) sintético: sólo emite rodando y rápido, la tasa ronda las 45/s a velocidad alta, todas mueren al cabo de su vida y el color sigue la superficie.
#include "../src/dust.hpp"
#include <cstdio>
#include <cstdlib>
#define CHECK(c) do { if (!(c)) { std::printf("FALLO %s:%d %s\n", __FILE__, __LINE__, #c); std::exit(1); } } while (0)
int main() {
    dust::Emitter e; for (int i = 0; i < 60; i++) dust::emit(e, {0, 0, 0}, {0, 0, -70}, 70.f, false, 1.f / 60.f, 0); CHECK(e.puffs.empty());       // en el aire no
    for (int i = 0; i < 60; i++) dust::emit(e, {0, 0, 0}, {0, 0, -2}, 2.f, true, 1.f / 60.f, 0); CHECK(e.puffs.empty());                              // lento no
    for (int i = 0; i < 60; i++) { dust::emit(e, {0, 0, 0}, {0, 0, -70}, 70.f, true, 1.f / 60.f, 0); } CHECK(e.puffs.size() >= 40 && e.puffs.size() <= 50);   // 1 s a 70 u/s ~ 45
    const size_t n0 = e.puffs.size(); for (int i = 0; i < 20; i++) dust::update(e, 1.f / 60.f); CHECK(e.puffs.size() == n0);                        // 0,33 s: nadie ha muerto (vida mínima 0,9 s)
    for (int i = 0; i < 150; i++) dust::update(e, 1.f / 60.f); CHECK(e.puffs.empty());                                                              // 2,8 s: todas muertas (vida máxima 1,6 s)
    dust::Emitter s; dust::emit(s, {0, 0, 0}, {0, 0, -70}, 70.f, true, 1.f, 14); CHECK(!s.puffs.empty() && s.puffs[0].b > 0.95f);                      // nieve = blanca
    dust::Emitter d2; dust::emit(d2, {0, 0, 0}, {0, 0, -70}, 70.f, true, 1.f, 1); CHECK(!d2.puffs.empty() && d2.puffs[0].r > d2.puffs[0].b);          // tierra = pardo
    std::printf("dust_test OK\n"); return 0;
}
