// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Geometría sintética: dos láminas superpuestas en XZ (suelo y 'puente' encima) + pendiente. Debe elegir la lámina correcta según la altura de la consulta.
#include "../src/ground.hpp"
#include <cstdio>
#include <cstdlib>
#define CHECK(c) do { if (!(c)) { std::printf("FALLO %s:%d %s\n", __FILE__, __LINE__, #c); std::exit(1); } } while (0)
static Ground::Tri quad_half(float y, float x0, float z0, float x1, float z1, uint16_t s, bool second) {
    Ground::Tri t; float p[9] = {x0, y, z0, x1, y, z0, x0, y, z1}, q[9] = {x1, y, z0, x1, y, z1, x0, y, z1};
    std::memcpy(t.v, second ? q : p, 36); t.surface = s; return t;
}
int main() {
    Ground g; g.set({quad_half(0, -50, -50, 50, 50, 1, false), quad_half(0, -50, -50, 50, 50, 1, true),
                     quad_half(100, -50, -50, 50, 50, 7, false), quad_half(100, -50, -50, 50, 50, 7, true)});
    GroundHit a = g.query(10, 5, 10);          CHECK(a.hit && a.height == 0 && a.surface == 1);       // sobre el suelo: ignora el puente (y=100 > 5+30)
    GroundHit b = g.query(10, 110, 10);        CHECK(b.hit && b.height == 100 && b.surface == 7);     // sobre el puente
    GroundHit c = g.query(10, 80, 10);         CHECK(c.hit && c.height == 100);                       // margen 30: el puente a 100 está dentro de y+30 (así el barrido lo alcanza primero)
    GroundHit d = g.query(10, 50, 10);         CHECK(d.hit && d.height == 0);                         // y+30 = 80 < 100: queda el suelo
    GroundHit e = g.query(500, 0, 0);          CHECK(!e.hit);                                         // fuera de la malla
    CHECK(std::fabs(b.ny - 1.f) < 1e-6f);                                                             // normal hacia +Y aunque el devanado sea inverso
    std::printf("ground_test OK\n"); return 0;
}
