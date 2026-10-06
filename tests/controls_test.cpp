// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Controles (src/controls.hpp) sobre terreno sintético, sin datos del juego: mapeo de teclas, registro de entradas y su ida y vuelta, determinismo de la repetición y efecto de los mandos
// (acelerar sube la velocidad hasta una cota, el esfuerzo extra la sube antes, frenar la baja sin invertir, girar desvía el rumbo, salto = un flanco).
#include "../src/controls.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#define CHECK(c) do { if (!(c)) { std::printf("FALLO %s:%d %s\n", __FILE__, __LINE__, #c); std::exit(1); } } while (0)
using Tri = Ground::Tri;
static Tri tri(V3 a, V3 b, V3 c, uint16_t s) { Tri t{{a.x, a.y, a.z, b.x, b.y, b.z, c.x, c.y, c.z}, s}; return t; }
static Ground flat() { Ground g; std::vector<Tri> T; float a = -6000, b = 6000; T.push_back(tri({a, 0, a}, {a, 0, b}, {b, 0, a}, 1)); T.push_back(tri({b, 0, b}, {b, 0, a}, {a, 0, b}, 1)); g.set(T); return g; }
static Bike settled(const Ground& g) { Bike b; b.place({0, 4.f, 0}, 0); for (int i = 0; i < 180; i++) b.step(g, BikeInput{}, 1.f / 60.f); return b; }
static void run(Bike& b, const Ground& g, const std::vector<uint8_t>& bits) { for (uint8_t x : bits) b.step(g, unpackInput(x), 1.f / 60.f); }
int main() {
    // mapeo: flechas y WASD equivalentes, ambas direcciones se anulan, el salto solo con flanco
    { Keys k; k.up = true; CHECK(packKeys(k, false) == kInThrottle); k = Keys(); k.w = true; CHECK(packKeys(k, false) == kInThrottle);
      k = Keys(); k.left = true; k.right = true; CHECK(unpackInput(packKeys(k, false)).steer == 0.f); k = Keys(); k.d = true; CHECK(unpackInput(packKeys(k, false)).steer == 1.f); k = Keys(); k.left = true; CHECK(unpackInput(packKeys(k, false)).steer == -1.f);
      k = Keys(); k.space = true; CHECK(!unpackInput(packKeys(k, false)).hop && unpackInput(packKeys(k, true)).hop);
      k = Keys(); k.q = true; CHECK(unpackInput(packKeys(k, false)).lean == 1.f); k = Keys(); k.e = true; CHECK(unpackInput(packKeys(k, false)).lean == -1.f); k = Keys(); k.shift = true; CHECK(unpackInput(packKeys(k, false)).sprint); }
    // registro: ida y vuelta por archivo; cabecera inválida se rechaza
    { std::vector<uint8_t> v = {0, 1, 3, 255, 64}, w; const char* p = "/tmp/dh_controls_test.in"; CHECK(saveInputs(p, v) && loadInputs(p, w) && v == w);
      FILE* f = std::fopen(p, "wb"); std::fputs("nada", f); std::fclose(f); CHECK(!loadInputs(p, w)); std::remove(p); }
    Ground g = flat();
    // acelerar: sube la velocidad hasta una cota (no crece sin límite)
    { Bike b = settled(g); std::vector<uint8_t> acc(60 * 20, kInThrottle); run(b, g, acc); float v20 = b.speed(); run(b, g, acc); float v40 = b.speed();
      CHECK(v20 > 15.f && v20 < 80.f); CHECK(std::fabs(v40 - v20) < 0.1f * v20 + 1.f); }
    // esfuerzo extra: en 3 s va más rápido que sin él
    { Bike a = settled(g), b = settled(g); std::vector<uint8_t> x(180, kInThrottle), y(180, kInThrottle | kInSprint); run(a, g, x); run(b, g, y); CHECK(b.speed() > a.speed() + 1.f); }
    // frenar: baja la velocidad y nunca invierte
    { Bike b = settled(g); run(b, g, std::vector<uint8_t>(60 * 6, kInThrottle)); float v0 = b.speed(); float minsf = 1e9f; for (int i = 0; i < 60 * 5; i++) { b.step(g, unpackInput(kInBrake), 1.f / 60.f); minsf = std::fmin(minsf, dot(b.vel(), b.axes().f)); }
      CHECK(b.speed() < 0.3f * v0 + 1.f); CHECK(minsf > -0.5f); }
    // girar: derecha aumenta el rumbo, izquierda lo reduce
    { Bike r = settled(g), l = settled(g); run(r, g, std::vector<uint8_t>(60 * 4, kInThrottle)); run(l, g, std::vector<uint8_t>(60 * 4, kInThrottle)); float hr = r.heading(), hl = l.heading();
      run(r, g, std::vector<uint8_t>(60 * 2, kInThrottle | kInRight)); run(l, g, std::vector<uint8_t>(60 * 2, kInThrottle | kInLeft)); CHECK(r.heading() - hr > 0.3f && l.heading() - hl < -0.3f); }
    // salto: un flanco despega y vuelve a caer
    { Bike b = settled(g); float y0 = b.pos.y; b.step(g, unpackInput(kInHop), 1.f / 60.f); float maxy = b.pos.y; for (int i = 0; i < 60; i++) { b.step(g, BikeInput{}, 1.f / 60.f); maxy = std::fmax(maxy, b.pos.y); } CHECK(maxy > y0 + 0.5f); }
    // determinismo: la misma secuencia de entradas da exactamente el mismo estado (base de la repetición y del fantasma)
    { std::vector<uint8_t> seq; for (int i = 0; i < 60 * 15; i++) seq.push_back((i / 50) % 3 == 0 ? (kInThrottle | kInRight) : (i / 50) % 3 == 1 ? (kInThrottle | kInLeft) : (kInThrottle | kInSprint | (i % 97 == 0 ? kInHop : 0)));
      Bike a = settled(g), b = settled(g); run(a, g, seq); run(b, g, seq); CHECK(std::memcmp(&a.pos, &b.pos, sizeof a.pos) == 0 && std::memcmp(&a.rb.vel, &b.rb.vel, sizeof a.rb.vel) == 0); }
    // controlador de dirección del motor (valores de RAM): satura a ~2 rad/s en pocos ticks, se detiene al instante al soltar (suelo) y el rumbo gira ~2 rad en 1 s
    { Bike b = settled(g); b.P.engineSteer = true; run(b, g, std::vector<uint8_t>(60 * 3, kInThrottle)); float h0 = b.heading();
      run(b, g, std::vector<uint8_t>(60, kInThrottle | kInRight)); float dh = b.heading() - h0; CHECK(dh > 1.6f && dh < 2.2f);      // ~2.0 rad/s (0.03316 rad/tick · 1.2 · 50)
      float h1 = b.heading(); run(b, g, std::vector<uint8_t>(6, kInThrottle)); CHECK(std::fabs(b.heading() - h1) < 0.05f); }          // sin entrada: la guiñada se para
    { Bike b = settled(g); b.P.engineSteer = true; b.P.hop = 32.f; float y0 = b.pos.y; b.step(g, unpackInput(kInHop), 1.f / 60.f); float maxy = b.pos.y; for (int i = 0; i < 120; i++) { b.step(g, BikeInput{}, 1.f / 60.f); maxy = std::fmax(maxy, b.pos.y); } CHECK(maxy - y0 > 3.5f && maxy - y0 < 9.f); }   // salto de ~32 u/s: 4-8 u de altura
    std::puts("controls_test OK"); return 0;
}
