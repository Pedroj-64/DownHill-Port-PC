// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// HUD (src/hud.hpp) con datos sintéticos (nunca texturas del juego): el cargador rechaza entrada corrupta, voltea las filas, y la maquetación del ordenador de la bici queda dentro de la pantalla a varias resoluciones.
#include "../src/hud.hpp"
#include <cstdio>
#include <cstdlib>
#define CHECK(c) do { if (!(c)) { std::printf("FALLO %s:%d %s\n", __FILE__, __LINE__, #c); std::exit(1); } } while (0)
static std::vector<uint8_t> synth(uint32_t n = hud::kTexCount, uint32_t w0 = 128, uint32_t h0 = 128) {
    std::vector<uint8_t> d = {'D', 'H', 'U', 'D'}; auto u32 = [&](uint32_t v) { for (int i = 0; i < 4; i++) d.push_back((uint8_t)(v >> (8 * i))); }; u32(n);
    for (uint32_t i = 0; i < n; i++) { uint32_t w = i == hud::kHousing ? 128 : w0, h = i == hud::kHousing ? 128 : h0; u32(w); u32(h); for (uint32_t y = 0; y < h; y++) for (uint32_t x = 0; x < w; x++) { d.push_back((uint8_t)y); d.push_back((uint8_t)x); d.push_back(7); d.push_back(255); } }
    return d;
}
int main() {
    std::vector<gfx::Texture> t; auto d = synth(); CHECK(hud::load(d, t) && t.size() == hud::kTexCount);
    CHECK(t[hud::kLcdFont].rgba[0] == 127);                                   // primera fila de la salida = última de la entrada (filas volteadas: v = 0 arriba)
    { auto bad = d; bad[0] = 'X'; CHECK(!hud::load(bad, t)); }                // firma
    { auto bad = d; bad.resize(bad.size() - 5); CHECK(!hud::load(bad, t)); } // truncado
    { std::vector<uint8_t> e; CHECK(!hud::load(e, t)); }                      // vacío
    { auto bad = synth(hud::kTexCount - 1); CHECK(!hud::load(bad, t)); }     // recuento incorrecto
    { auto bad = synth(hud::kTexCount, 5000, 5000); bad.resize(200); CHECK(!hud::load(bad, t)); }   // dimensiones enormes / datos que no alcanzan
    // carcasa: las esquinas quedan transparentes (máscara redondeada) y el centro opaco
    CHECK(hud::load(d, t) && t[hud::kHousing].rgba[3] == 0 && t[hud::kHousing].rgba[((64 * 128) + 64) * 4 + 3] == 255);
    // fuente LCD: cadena conocida da un rectángulo por glifo visible, desconocida se ignora, el «6» es el «9» girado
    { std::vector<gfx::TexQuad> q; const float c[4] = {1, 1, 1, 1}; hud::lcdText("17", 100.f, 10.f, 42.f, c, q); CHECK(q.size() == 2 && q[1].x + q[1].w <= 100.f + 1e-3f && q[0].x < q[1].x);
      q.clear(); hud::lcdText("69", 100.f, 10.f, 42.f, c, q); CHECK(q.size() == 2 && q[0].u0 > q[0].u1 && q[1].u0 < q[1].u1); q.clear(); hud::lcdText("a?", 100.f, 10.f, 42.f, c, q); CHECK(q.empty()); }
    // maquetación: todo dentro de la pantalla en 640x480, 1280x720 y 1920x1080
    for (int vw : {640, 1280, 1920}) { int vh = vw * 9 / 16; if (vw == 640) vh = 480;
        std::vector<gfx::TexQuad> h, l, k; std::vector<gfx::Quad> leds; hud::bikeComputer(vw, vh, 123, "07:10", "28", h, l, k); hud::bikeComputerLeds(vw, vh, 123, leds);
        CHECK(h.size() == 1 && !l.empty() && k.size() == 1 && leds.size() >= 6);
        auto in = [&](float x, float y, float w, float hh) { return x >= -0.5f && y >= -0.5f && x + w <= vw + 0.5f && y + hh <= vh + 0.5f; };
        for (auto& q : h) CHECK(in(q.x, q.y, q.w, q.h)); for (auto& q : l) CHECK(in(q.x, q.y, q.w, q.h)); for (auto& q : k) CHECK(in(q.x, q.y, q.w, q.h)); for (auto& q : leds) CHECK(in(q.x, q.y, q.w, q.h)); }
    // panel de progreso: recorrido normalizado, puntos dentro de la caja, jugador dentro del panel y avance monótono a lo largo del trazado
    { std::vector<float> pts; for (int i = 0; i < 300; i++) { float a = i * 0.05f; pts.push_back(100.f * std::cos(a)); pts.push_back(-5.f * i); pts.push_back(40.f * i + 30.f * std::sin(a * 3)); }
      auto c = hud::fitCourse(pts, 64); CHECK(c.size() >= 4 && c.size() / 2 <= 65 + 1); for (float f : c) CHECK(f >= -1e-4f && f <= 1.0001f);
      CHECK(c[1] < c[c.size() - 1]);                                                                                        // la salida arriba, la meta abajo
      CHECK(hud::fitCourse({}).empty() && hud::fitCourse({1, 2, 3}).empty() && hud::fitCourse({5, 0, 5, 5, 9, 5}).empty());  // degenerados: ningún trazado
      float lastY = -1; for (float fr : {-1.f, 0.f, 0.3f, 0.7f, 1.f, 2.f}) for (int vw : {640, 1920}) { int vh = vw == 640 ? 480 : 1080; std::vector<gfx::Quad> q; hud::Rect lb; hud::progressPanel(vw, vh, c, fr, q, lb);
        CHECK(q.size() > 10); for (auto& r : q) CHECK(r.x >= 0 && r.y >= 0 && r.x + r.w <= vw * 0.2f + 1 && r.y + r.h <= vh + 1); CHECK(lb.y + lb.h <= vh + 1 && lb.x >= 0);
        if (vw == 640 && fr >= 0.f && fr <= 1.f) { float py = q.back().y; CHECK(py >= lastY - 1e-3f); lastY = py; } }   // el punto del jugador (último rectángulo) baja con el avance
      std::vector<gfx::Quad> q; hud::Rect lb; hud::progressPanel(640, 480, {}, 0.5f, q, lb); CHECK(q.size() > 10); }          // sin recorrido: línea recta
    std::puts("hud_test OK"); return 0;
}
