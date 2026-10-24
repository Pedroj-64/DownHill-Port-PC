// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Rivales «fantasma» (HIPÓTESIS, sin IA real del juego): siguen la línea de carrera PTS a una velocidad que depende de la pendiente, sin colisionar con el jugador.
// Sólo dan vida a la carrera (el juego tiene 5-9 rivales); la IA verdadera (AiPathBins, docs/formats/ptsext.md) no está portada.
#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
#include "ground.hpp"

namespace ghosts {
struct Line { std::vector<V3> p; std::vector<float> s; float total = 0.f; };

inline Line build(const std::vector<float>& ov) {   // x y z sueltos (espacio del visor, Y arriba)
    Line L; for (size_t i = 0; i + 2 < ov.size(); i += 3) L.p.push_back({ov[i], ov[i + 1], ov[i + 2]});
    L.s.assign(L.p.size(), 0.f);
    for (size_t i = 1; i < L.p.size(); i++) { const V3 d{L.p[i].x - L.p[i - 1].x, L.p[i].y - L.p[i - 1].y, L.p[i].z - L.p[i - 1].z}; L.s[i] = L.s[i - 1] + std::sqrt(dot(d, d)); }
    L.total = L.s.empty() ? 0.f : L.s.back(); return L;
}
// Punto de la línea a la distancia recorrida `s` y su tangente unitaria (con unos pies de adelanto para suavizar los quiebros).
inline void at(const Line& L, float s, V3& pos, V3& dir) {
    if (L.p.size() < 2) { pos = {}; dir = {0, 0, -1}; return; }
    auto pt = [&](float q) { q = std::fmax(0.f, std::fmin(L.total, q)); size_t i = std::upper_bound(L.s.begin(), L.s.end(), q) - L.s.begin(); if (i == 0) i = 1; if (i >= L.p.size()) i = L.p.size() - 1;
        const float seg = L.s[i] - L.s[i - 1], t = seg > 1e-6f ? (q - L.s[i - 1]) / seg : 0.f; return V3{L.p[i - 1].x + (L.p[i].x - L.p[i - 1].x) * t, L.p[i - 1].y + (L.p[i].y - L.p[i - 1].y) * t, L.p[i - 1].z + (L.p[i].z - L.p[i - 1].z) * t}; };
    pos = pt(s); const V3 a = pt(s + 8.f), b = pt(s - 2.f); V3 d{a.x - b.x, a.y - b.y, a.z - b.z}; const float l = std::sqrt(dot(d, d));
    dir = l > 1e-4f ? V3{d.x / l, d.y / l, d.z / l} : V3{0, 0, -1};
}
// Distancia recorrida del vértice de la línea más cercano a `q` (en 3D).
inline float project(const Line& L, V3 q) {
    float best = 1e30f, s = 0.f; for (size_t i = 0; i < L.p.size(); i++) { const V3 d{L.p[i].x - q.x, L.p[i].y - q.y, L.p[i].z - q.z}; const float dd = dot(d, d); if (dd < best) { best = dd; s = L.s[i]; } } return s;
}

struct Rider { float s = 0.f, v = 0.f, lane = 0.f, pace = 0.9f; size_t model = 0; };

// Un paso: acelera hacia la velocidad objetivo según la pendiente de la línea (cuesta abajo = más rápido); 25 u/s² es la aceleración de pedaleo medida en el motor.
inline void step(Rider& r, const Line& L, float dt) {
    V3 p, d; at(L, r.s, p, d);
    const float down = -d.y;                                           // seno de la pendiente cuesta abajo (tangente unitaria)
    const float target = r.pace * std::fmax(18.f, std::fmin(110.f, 32.f + 120.f * down));
    r.v += std::fmax(-22.f * dt, std::fmin(25.f * dt, target - r.v));
    r.s = std::fmin(L.total, r.s + r.v * dt);
}
}  // namespace ghosts
