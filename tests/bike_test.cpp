// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Moto jugable (src/bike.hpp) sobre terreno sintético: apoya sobre las dos ruedas, no atraviesa el suelo, acelera, frena sin invertir, gira, baja una rampa y respeta el balanceo bloqueado.
// Sin datos del juego. Los números son del modelo propio (hipótesis, docs/formats/bike-physics.md), no del motor: aquí se comprueba coherencia física, no fidelidad.
#include "../src/bike.hpp"
#include <cstdio>
#include <cstdlib>
#define CHECK(c) do { if (!(c)) { std::printf("FALLO %s:%d %s\n", __FILE__, __LINE__, #c); std::exit(1); } } while (0)
using Tri = Ground::Tri;
static Tri tri(V3 a, V3 b, V3 c, uint16_t s) { Tri t{{a.x, a.y, a.z, b.x, b.y, b.z, c.x, c.y, c.z}, s}; return t; }
// plano de 4000x4000 con cara +Y e inclinación `slope` (dy/dx = -slope: baja hacia +X)
static Ground plane(float slope) {
    auto y = [&](float x) { return -slope * x; }; float a = -2000, b = 2000; std::vector<Tri> T;
    T.push_back(tri({a, y(a), a}, {a, y(a), b}, {b, y(b), a}, 1)); T.push_back(tri({b, y(b), b}, {b, y(b), a}, {a, y(a), b}, 1));
    Ground g; g.set(T); return g;
}
static void run(Bike& b, const Ground& g, BikeInput in, float secs) { for (int i = 0; i < (int)(secs * 60); i++) b.step(g, in, 1.f / 60.f); }
int main() {
    // --- reposo: cae de 6 u, se apoya con las dos ruedas y se queda quieta; los centros de rueda a ~radio sobre el suelo ---
    { Ground g = plane(0); Bike b; b.place({0, 6.f, 0}, 0); run(b, g, {}, 4.f);
      Axes A = b.axes(); V3 f = b.point(0, A, b.pos), r = b.point(1, A, b.pos);
      CHECK(b.grounded && b.wheelF && b.wheelR); CHECK(b.speed() < 0.3f);
      CHECK(f.y > 0.74f && f.y < 0.85f); CHECK(r.y > 0.74f && r.y < 0.85f);
      CHECK(b.axes().u.y > 0.99f); }
    // --- caída libre desde 300 u a ~150 u/s: no atraviesa el suelo ---
    { Ground g = plane(0); Bike b; b.place({0, 300.f, 0}, 0); float miny = 1e9f; for (int i = 0; i < 60 * 12; i++) { b.step(g, {}, 1.f / 60.f); miny = std::fmin(miny, b.point(0, b.axes(), b.pos).y); }
      CHECK(miny > 0.f); CHECK(b.pos.y > 2.f); }
    // --- acelerar: gana velocidad hacia delante y se mantiene recta (rumbo 0 = -Z) ---
    { Ground g = plane(0); Bike b; b.place({0, 4.f, 0}, 0); run(b, g, {}, 1.f); BikeInput in; in.throttle = 1; run(b, g, in, 6.f);
      float sf = dot(b.vel(), b.axes().f); CHECK(sf > 10.f && sf < 45.f); CHECK(std::fabs(b.vel().x) < 0.04f * b.speed()); CHECK(b.pos.z < -20.f); }
    // --- frenar: la velocidad cae a ~0 y nunca invierte el sentido ---
    { Ground g = plane(0); Bike b; b.place({0, 4.f, 0}, 0); BikeInput in; in.throttle = 1; run(b, g, in, 6.f); in.throttle = 0; in.brake = 1; float minsf = 1e9f;
      for (int i = 0; i < 60 * 5; i++) { b.step(g, in, 1.f / 60.f); minsf = std::fmin(minsf, dot(b.vel(), b.axes().f)); } CHECK(b.speed() < 1.5f); CHECK(minsf > -0.5f); }
    // --- girar a la derecha (steer > 0): el rumbo aumenta y la trayectoria se curva hacia +X ---
    { Ground g = plane(0); Bike b; b.place({0, 4.f, 0}, 0); BikeInput in; in.throttle = 1; run(b, g, in, 4.f); float h0 = b.heading(); in.steer = 1; run(b, g, in, 2.f);
      CHECK(b.heading() - h0 > 0.4f); CHECK(b.pos.x > 5.f); CHECK(b.axes().u.y > 0.95f); }
    // --- rampa del 36% (20 grados) hacia +X: sin mandos baja y acelera pegada al plano ---
    { Ground g = plane(0.36f); Bike b; b.place({0, 4.f, 0}, 1.5708f); run(b, g, {}, 5.f);
      CHECK(b.pos.x > 40.f); CHECK(b.speed() > 15.f); CHECK(b.grounded);
      float planeY = -0.36f * b.pos.x; CHECK(b.pos.y - planeY < 5.f && b.pos.y - planeY > 2.f); }
    // --- el balanceo se mantiene bloqueado: tras girar en rampa el eje lateral es horizontal ---
    { Ground g = plane(0.2f); Bike b; b.place({0, 4.f, 0}, 1.5708f); BikeInput in; in.throttle = 1; in.steer = 0.6f; run(b, g, in, 4.f); CHECK(std::fabs(b.axes().r.y) < 1e-4f); }
    // --- salto: en el suelo despega y vuelve a caer; en el aire el impulso no se aplica ---
    { Ground g = plane(0); Bike b; b.place({0, 4.f, 0}, 0); run(b, g, {}, 2.f); float y0 = b.pos.y; BikeInput in; in.hop = true; b.step(g, in, 1.f / 60.f); in.hop = false;
      float maxy = b.pos.y; for (int i = 0; i < 40; i++) { b.step(g, in, 1.f / 60.f); maxy = std::fmax(maxy, b.pos.y); } CHECK(maxy > y0 + 0.5f); run(b, g, {}, 2.f); CHECK(b.grounded); }
    // --- Run: el reinicio devuelve a la moto al último punto bueno y cuenta ---
    { Ground g = plane(0); Run r; r.start(g, {0, 0, 0}, 0); BikeInput in; in.throttle = 1; for (int i = 0; i < 60 * 4; i++) r.update(g, in, 1.f / 60.f);
      V3 good = r.goodPos; CHECK(good.z < -3.f); r.bike.place({500, -300, 500}, 0); r.update(g, in, 1.f / 60.f); CHECK(r.respawns == 1); CHECK(std::fabs(r.bike.pos.z - good.z) < 5.f); }
    // --- dirección en el aire (hipótesis, BikeParams::airYawAccel): en caída libre steer > 0 aumenta el rumbo y se queda en el tope airYawMax; sin mando el rumbo se conserva ---
    { Ground g = plane(0); Bike b; b.place({0, 400.f, 0}, 0); BikeInput in; in.steer = 1; run(b, g, in, 1.f); CHECK(!b.grounded); CHECK(b.heading() > 0.2f); CHECK(std::fabs(b.rb.omega.y) <= b.P.airYawMax + 1e-3f);
      Bike c; c.place({0, 400.f, 0}, 0); run(c, g, {}, 1.f); CHECK(std::fabs(c.heading()) < 1e-3f); }
    std::printf("bike_test OK\n"); return 0;
}
