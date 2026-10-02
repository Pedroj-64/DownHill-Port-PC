// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Geometría sintética (Y arriba, triángulos con cara frontal +Y según (v2-v1)x(v0-v1)). Cubre: láminas superpuestas y puentes (regresión del comportamiento anterior:
// elegir la lámina correcta), bordes de celda, segmentos oblicuos, impacto en arista y en vértice, solape inicial, caras traseras.
#include "../src/ground.hpp"
#include <cstdio>
#include <cstdlib>
#define CHECK(c) do { if (!(c)) { std::printf("FALLO %s:%d %s\n", __FILE__, __LINE__, #c); std::exit(1); } } while (0)
#define NEAR(a, b, e) CHECK(std::fabs((a) - (b)) <= (e))
using Tri = Ground::Tri;
static Tri tri(float x0, float y0, float z0, float x1, float y1, float z1, float x2, float y2, float z2, uint16_t s) { Tri t{{x0, y0, z0, x1, y1, z1, x2, y2, z2}, s}; return t; }
static void quad(std::vector<Tri>& o, float y, float x0, float z0, float x1, float z1, uint16_t s) {   // dos triángulos con cara +Y
    o.push_back(tri(x0, y, z0, x0, y, z1, x1, y, z0, s)); o.push_back(tri(x1, y, z1, x1, y, z0, x0, y, z1, s));
}
int main() {
    std::vector<Tri> T; quad(T, 0, -50, -50, 50, 50, 1); quad(T, 100, -50, -50, 50, 50, 7);
    Ground g; g.set(T);
    // --- regresión: elegir la lámina correcta con láminas superpuestas ---
    { SweepHit h = g.sweep({10, 110, 10}, {10, -10, 10}, 0); CHECK(h.hit && h.surface == 7); NEAR(110 - 120 * h.frac, 100, 0.02f); }    // desde arriba: el puente primero
    { SweepHit h = g.sweep({10, 50, 10}, {10, -50, 10}, 0); CHECK(h.hit && h.surface == 1); NEAR(50 - 100 * h.frac, 0, 0.02f); }          // desde entre láminas: el suelo
    { SweepHit h = g.sweep({10, 50, 10}, {10, 150, 10}, 0); CHECK(!h.hit); }                                                              // desde abajo el puente no bloquea (un solo lado)
    { SweepHit h = g.sweep({500, 50, 0}, {500, -50, 0}, 0); CHECK(!h.hit); }                                                              // fuera de la malla
    // --- fracción y esfera: el centro se detiene a r por encima del plano ---
    { SweepHit h = g.sweep({10, 50, 10}, {10, -50, 10}, 1.f); CHECK(h.hit); NEAR(50 - 100 * h.frac, 1.f, 0.02f); NEAR(h.normal.y, 1.f, 1e-5f); NEAR(h.point.y, 0, 0.02f); }
    // --- segmento oblicuo: de (-5,10) a (15,-10) cruza y=0 en x=5 ---
    { SweepHit h = g.sweep({-5, 10, 0}, {15, -10, 0}, 0); CHECK(h.hit); NEAR(h.frac, 0.5f, 0.001f); NEAR(h.point.x, 5, 0.02f); }
    // --- borde de celda (kCell=100): triángulo a caballo de x=100 ---
    { Ground c; std::vector<Tri> t; quad(t, 0, 95, -5, 105, 5, 3); c.set(t);
      for (float x : {99.99f, 100.f, 100.01f}) { SweepHit h = c.sweep({x, 5, 1}, {x, -5, 1}, 0); CHECK(h.hit && h.surface == 3); }   // z=1: la diagonal compartida pasa por (100,0)
      CHECK(!c.sweep({100.f, 5, 0}, {100.f, -5, 0}, 0).hit);   // r=0 exactamente sobre la arista compartida: ningún triángulo la reclama (c0.c1>0 estricto, como el motor); con r>0 lo cubren las aristas
      CHECK(c.sweep({100.f, 5, 0}, {100.f, -5, 0}, 0.5f).hit); }
    // --- impacto en arista: triángulo (0,0,0),(0,0,10),(10,0,0); esfera r=1 cae a x=-0.5 (fuera del triángulo): toca la arista x=0 cuando y = sqrt(1-0.25) ---
    { Ground e; e.set({tri(0, 0, 0, 0, 0, 10, 10, 0, 0, 2)});
      SweepHit h = e.sweep({-0.5f, 5, 2}, {-0.5f, -5, 2}, 1.f); CHECK(h.hit); NEAR(5 - 10 * h.frac, std::sqrt(0.75f), 0.01f);
      NEAR(h.normal.x, -0.5f, 0.01f); NEAR(h.normal.y, std::sqrt(0.75f), 0.01f); NEAR(h.point.x, 0, 1e-4f);
      // vértice (0,0,0): cae a (-0.5, y, -0.5): y = sqrt(1-0.5)
      SweepHit v = e.sweep({-0.5f, 5, -0.5f}, {-0.5f, -5, -0.5f}, 1.f); CHECK(v.hit); NEAR(5 - 10 * v.frac, std::sqrt(0.5f), 0.01f); NEAR(v.point.x, 0, 1e-4f); NEAR(v.point.z, 0, 1e-4f);
      // impacto exacto sobre la arista con r=0: el punto cae justo en x=0 (borde del triángulo): el motor exige c0.c1>0 estricto -> sin impacto de cara; sin radio no hay aristas
      SweepHit x = e.sweep({0, 5, 2}, {0, -5, 2}, 0); CHECK(!x.hit);
      // solape inicial: centro a 0.5 sobre el triángulo con r=1 -> frac = -FLT_MAX, pen = 0.01 - (0.5-1)
      SweepHit o = e.sweep({2, 0.5f, 2}, {2, -5, 2}, 1.f); CHECK(o.hit && o.frac == kOverlap); NEAR(o.pen, 0.51f, 1e-4f); NEAR(o.normal.y, 1.f, 1e-5f);
      // cara trasera: desde debajo del triángulo no bloquea
      SweepHit b = e.sweep({2, -5, 2}, {2, 5, 2}, 1.f); CHECK(!b.hit); }
    // --- groundQuery: altura bajo el centro de la esfera ---
    { Ground::GroundInfo q = g.groundQuery(5, 20, 5, 0.5f, 40); CHECK(q.hit); NEAR(q.height, 0, 0.03f); CHECK(q.surface == 1); CHECK(q.normal.y > 0.99f); }
    std::printf("ground_test OK\n"); return 0;
}
