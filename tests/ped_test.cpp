// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Activación de grupos de peatones por altura (src/peds.hpp, FUN_00127c60). Valores del grupo 7 de un savestate: Z del jugador 410,2 dentro de [-1773,628), grupos vecinos fuera.
#include "../src/peds.hpp"
#include <cstdio>
#include <cstdlib>
#define CHECK(c) do { if (!(c)) { std::printf("FALLO %s:%d %s\n", __FILE__, __LINE__, #c); std::exit(1); } } while (0)
int main() {
    const std::array<float, 6> g7{-1.f, -1573.4f, -2.f, 1.f, 427.6f, 2.f};   // viewer y = Z del archivo; la caja añade 200 u
    CHECK(peds::groupActive(g7, 410.2f));
    CHECK(peds::groupActive(g7, -1773.4f) && !peds::groupActive(g7, -1773.5f));   // borde inferior inclusivo
    CHECK(peds::groupActive(g7, 627.5f) && !peds::groupActive(g7, 627.6f));        // borde superior exclusivo
    const std::array<float, 6> far{5000.f, -1573.4f, 5000.f, 6000.f, 427.6f, 6000.f};
    CHECK(peds::groupActive(far, 410.2f));   // lejos en X/Z: el juego no mira X ni Y
    CHECK(peds::cheers(2, 199.f * 199.f) && !peds::cheers(2, 201.f * 201.f));   // tipo 2: dentro de 200 u
    CHECK(peds::cheers(6, 39.f * 39.f) && !peds::cheers(6, 41.f * 41.f));      // tipo 6: solo muy cerca
    CHECK(!peds::cheers(0, 1.f));                                               // tipo 0: no vitorea
    std::puts("ped_test OK");
}
