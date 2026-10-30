// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Casos puntuales de src/surfsound.hpp comprobados a mano contra las tablas de saltos del ejecutable PAL
// (tools/surf_sound_table.py los regenera; si la tabla cambia, estos valores son los que no deben moverse).
#include "../src/surfsound.hpp"
#include <cstdio>
#include <cstdlib>
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FALLO %s:%d: %s\n", __FILE__, __LINE__, #c); std::exit(1); } } while (0)
using namespace surfsound;
static uint16_t code(int c) { return (uint16_t)(c << 11); }   // bits 11-15 del u16 de superficie
int main() {
    CHECK(levelGroup("ALPINEMX") == 0 && levelGroup("MOAB2") == 1 && levelGroup("JUNGLET2") == 2 && levelGroup("PERUTDH") == 3);
    CHECK(levelGroup("GLAC2P") == 4 && levelGroup("AUBERST") == 5 && levelGroup("FUJI") == 6 && levelGroup("CITYTDH") == 7);
    CHECK(levelGroup("BCTDX") == 8 && levelGroup("MOSH") == 9 && levelGroup("TRAINER") == 0);
    CHECK(kind(4, code(5) | 0x7FF) == 4 * 32 + 5);   // los 11 bits bajos (física + efecto) no cuentan
    CHECK(rideName(kind(0, code(2)), true) == "B_GRAS_PEDAL" && rideName(kind(0, code(2)), false) == "B_GRAS_COAST");
    CHECK(rideName(kind(4, code(5)), true) == "B_SNOW_PEDAL");     // glaciar, código 5 = nieve
    CHECK(rideName(kind(0, code(1)), true) == "B_ROCK_PEDAL");     // rareza del juego: el camino de tierra del grupo 0 rueda como ROCK (el freno sí es DIRT)
    CHECK(rideName(kind(8, code(8)), false) == "B_TRAK_COAST");    // BC: pista
    CHECK(rideName(kind(0, code(4)), true).empty());               // sin sonido (código no usado en el grupo 0)
    CHECK(std::string(landMat(kind(4, code(5)))) == "SNOW" && std::string(landMat(kind(0, code(6)))) == "H2O");
    std::puts("surfsound_test OK");
    return 0;
}
