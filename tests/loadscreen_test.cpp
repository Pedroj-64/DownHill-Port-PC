// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Pantalla de carga (src/loadscreen.hpp) y estado Carga de Race con datos sintéticos (nunca imágenes del juego): cargador, detección del borde del cuadro, nombres de archivo, maquetación y duración.
#include "../src/loadscreen.hpp"
#include "../src/race.hpp"
#include <cstdio>
#include <cstdlib>
#define CHECK(c) do { if (!(c)) { std::printf("FALLO %s:%d %s\n", __FILE__, __LINE__, #c); std::exit(1); } } while (0)
using Tri = Ground::Tri;
static std::vector<uint8_t> synth(uint32_t w, uint32_t h, int paintRows) {   // cuadro claro en las filas 0..paintRows y negro debajo; línea de borde en la fila 0
    std::vector<uint8_t> d = {'D', 'L', 'B', 'R'}; auto u32 = [&](uint32_t v) { for (int i = 0; i < 4; i++) d.push_back((uint8_t)(v >> (8 * i))); }; u32(w); u32(h);
    for (uint32_t y = 0; y < h; y++) for (uint32_t x = 0; x < w; x++) { bool on = (int)y < paintRows && y > 0; d.push_back(on ? 200 : 0); d.push_back(on ? 120 : 0); d.push_back(on ? 80 : 0); d.push_back(255); }
    return d;
}
int main() {
    gfx::Texture t; auto d = synth(512, 512, 400); CHECK(loadscreen::load(d, t) && t.w == 512 && t.h == 512);
    CHECK(loadscreen::pictureBottom(t) == 400);
    { auto bad = d; bad[0] = 'X'; CHECK(!loadscreen::load(bad, t)); } { auto bad = d; bad.resize(100); CHECK(!loadscreen::load(bad, t)); } { std::vector<uint8_t> e; CHECK(!loadscreen::load(e, t)); }
    { auto huge = synth(8, 8, 4); huge[4] = 0; huge[5] = 0x40; huge.resize(20); CHECK(!loadscreen::load(huge, t)); }       // 16384 de ancho / datos que no alcanzan
    { gfx::Texture dark; dark.w = dark.h = 16; dark.rgba.assign(16 * 16 * 4, 0); CHECK(loadscreen::pictureBottom(dark) == 16); }   // imagen vacía: se usa entera
    // nombres de archivo
    { auto c = loadscreen::candidates("ALPINEMX"); CHECK(c[0] == "LALPMX"); c = loadscreen::candidates("ALP2"); CHECK(c[0] == "LALPMX"); c = loadscreen::candidates("ALPINET"); CHECK(c[0] == "LALPTD");
      c = loadscreen::candidates("alpine"); CHECK(c[0] == "LALPMX"); c = loadscreen::candidates("TRAINER"); CHECK(c[0] == "LOADMX"); c = loadscreen::candidates("AUBERMX2"); CHECK(c[0] == "LAUBMX"); CHECK(loadscreen::find("/no/existe", "ALP2").empty()); }
    // maquetación: dentro de pantalla, proporción conservada, progreso proporcional, a varias resoluciones y progresos fuera de rango
    for (int vw : {640, 1280, 1920}) for (int vh : {480, 720, 1080}) for (float pr : {-1.f, 0.f, 0.5f, 1.f, 3.f}) {
        gfx::Texture tt; auto dd = synth(512, 512, 400); loadscreen::load(dd, tt); auto L = loadscreen::layout(tt, vw, vh, pr);
        CHECK(L.pic.x >= -0.5f && L.pic.y >= -0.5f && L.pic.x + L.pic.w <= vw + 0.5f && L.pic.y + L.pic.h <= vh * 0.88f + 0.5f); CHECK(std::fabs(L.pic.w / L.pic.h - 512.f / 400.f) < 1e-3f); CHECK(std::fabs(L.pic.v1 - 400.f / 512.f) < 1e-6f);
        for (auto& q : L.bar) CHECK(q.x >= 0 && q.y >= 0 && q.x + q.w <= vw && q.y + q.h <= vh);
        float fill = pr <= 0.f ? 0.f : std::fmin(pr, 1.f); CHECK(L.bar.size() == (fill > 0.f ? 3u : 2u)); if (fill > 0.f) CHECK(std::fabs(L.bar.back().w - L.bar[1].w * fill) < 1e-2f); }
    // Race: con loadLen > 0 pasa por Carga antes de la cuenta atrás; con 0 (por defecto) pasa directo; el progreso va de 0 a 1
    Ground g; { std::vector<Tri> T; float a = -4000, b = 4000; T.push_back({{a, 0, a, a, 0, b, b, 0, a}, 1}); T.push_back({{b, 0, b, b, 0, a, a, 0, b}, 1}); g.set(T); }
    Gates gts; { std::vector<Gate> c; Gate q; q.kind = 8050; q.idx = 0; q.n = {0, 0, -1}; q.d = 100.f; c.push_back(q); gts.set(c, {}); }
    { Race r; r.load(g, &gts, {0, 0, 0}, 0.f); CHECK(r.state == RaceState::Countdown); }
    { Race r; r.loadLen = 1.5f; r.load(g, &gts, {0, 0, 0}, 0.f); CHECK(r.state == RaceState::Loading && r.loadProgress() < 0.01f); BikeInput in; int n = 0; float last = 0;
      while (r.state == RaceState::Loading && n++ < 600) { r.update(g, in, 1.f / 60.f); CHECK(r.loadProgress() >= last); last = r.loadProgress(); CHECK(r.displayTime() == 0.0); }
      CHECK(r.state == RaceState::Countdown && n >= 89 && n <= 92 && r.countdownDigit() == 3 && r.loadProgress() == 1.f); }
    std::puts("loadscreen_test OK"); return 0;
}
