// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Arte del menú original leído de la ISO del jugador (SHELL/UI.{NGP,PTR,TEX,RTX}, el mismo contenedor que los niveles): logo, ilustraciones por circuito y atlas con los nombres de nivel.
// Las texturas salen con las filas invertidas (las UV de la escena 3D las compensan): aquí se voltean para dibujarlas como imagen 2D. Numeración = orden de materiales distintos (id, CBP) de la escena (tests/tex_dump.cpp).
// Mapa medido a ojo sobre los volcados (HIPÓTESIS de contenido, docs/formats/menu-assets.md): 136+137+138 logo «Downhill DOMINATION» (tres mosaicos de 128x128); 125/224/233 atlas de nombres de nivel; 222/223, 225/226, 227/228, 229/230, 231/232, 234/235, 236/237, 238/239, 240/241 arte de los 9 circuitos (mitad izquierda/derecha).
#pragma once
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "nativelevel.hpp"
#include "renderer.hpp"

namespace menuart {
struct Sprite { int tex = -1; float u0 = 0, v0 = 0, u1 = 1, v1 = 1; float w = 0, h = 0; };   // textura (índice en Art::tex) + rectángulo UV (v = 0 arriba) y tamaño en px
struct Art { std::vector<gfx::Texture> tex; std::map<int, int> byIndex; std::map<std::string, Sprite> names; bool ok = false; };

// Bandas horizontales con contenido (alfa o brillo) de una textura ya volteada: filas [y0, y1) y extensión x [x0, x1) de cada una.
struct Band { int y0, y1, x0, x1; };
inline std::vector<Band> textBands(const gfx::Texture& t) {
    std::vector<Band> out; int y0 = -1, x0 = t.w, x1 = 0;
    auto lit = [&](int x, int y) { const uint8_t* p = &t.rgba[((size_t)y * t.w + x) * 4]; return p[3] > 40 && (p[0] + p[1] + p[2]) > 150; };
    for (int y = 0; y <= t.h; y++) {
        bool any = false; if (y < t.h) for (int x = 0; x < t.w; x++) if (lit(x, y)) { any = true; x0 = std::min(x0, x); x1 = std::max(x1, x + 1); }
        if (any && y0 < 0) y0 = y; else if (!any && y0 >= 0) { if (y - y0 >= 6) out.push_back({y0, y, x0, x1}); y0 = -1; x0 = t.w; x1 = 0; }
    }
    return out;
}

inline bool load(const nativelevel::Reader& rd, Art& A) {
    std::vector<uint8_t> n, p, t, c; const std::string b = "SHELL/UI";
    if (!rd(b + ".NGP", n) || !rd(b + ".PTR", p) || !rd(b + ".TEX", t) || !rd(b + ".RTX", c)) return false;
    tex::Level L; if (!tex::load(n, p, t, c, L)) return false;
    const std::set<int> want = {136, 137, 138, 125, 224, 233, 222, 223, 225, 226, 227, 228, 229, 230, 231, 232, 234, 235, 236, 237, 238, 239, 240, 241};   // el arte de cada circuito son DOS mitades consecutivas (izquierda, derecha) de 256x256
    std::set<std::pair<uint32_t, uint32_t>> seen; int k = 0;
    for (const tex::Material& m : L.mats) {
        if (!seen.insert({m.tid, m.cbp}).second) continue; k++; if (!want.count(k)) continue;
        uint32_t W, H; std::vector<uint8_t> px; if (!tex::makeTexture(L, m, W, H, px)) continue;
        gfx::Texture g; g.w = (int)W; g.h = (int)H; g.rgba.resize(px.size());
        for (uint32_t y = 0; y < H; y++) std::memcpy(&g.rgba[(size_t)y * W * 4], &px[(size_t)(H - 1 - y) * W * 4], (size_t)W * 4);   // voltea
        A.byIndex[k] = (int)A.tex.size(); A.tex.push_back(std::move(g));
    }
    // nombres de nivel, en el orden en que se leen los atlas (arriba abajo)
    static const std::vector<std::pair<int, std::vector<const char*>>> atlas = {
        {125, {"ITALIAN ALPS FR", "ITALIAN ALPS MX", "UTAH FR", "UTAH MX", "UTAH TD", "HAWAII FR", "HAWAII MX", "HAWAII TD", "PERU FR", "PERU MX", "PERU TD", "RUSSIA FR"}},
        {224, {"ITALIAN ALPS TD", "BRITISH COLUMBIA MX", "UTAH2 FR", "UTAH2 MX", "UTAH2 TD", "MONKEYBALL"}},
        {233, {"RUSSIA MX", "SCOTLAND FR", "SCOTLAND MX", "SCOTLAND TD", "JAPAN FR", "USA FR", "USA MX", "USA TD", "BRITISH COLUMBIA FR", "JAPAN FR 2", "RUSSIA TD", "MOSHPIT 1"}}};
    for (const auto& [idx, labels] : atlas) {
        auto it = A.byIndex.find(idx); if (it == A.byIndex.end()) continue; const gfx::Texture& g = A.tex[it->second]; const auto bands = textBands(g);
        for (size_t i = 0; i < labels.size() && i < bands.size(); i++) { const Band& bd = bands[i];
            A.names[labels[i]] = {it->second, (float)bd.x0 / g.w, (float)bd.y0 / g.h, (float)bd.x1 / g.w, (float)bd.y1 / g.h, (float)(bd.x1 - bd.x0), (float)(bd.y1 - bd.y0)}; }
    }
    A.ok = A.byIndex.count(136) && !A.names.empty(); return A.ok;
}
inline Sprite whole(const Art& A, int index) { Sprite s; auto it = A.byIndex.find(index); if (it == A.byIndex.end()) return s; s.tex = it->second; s.w = (float)A.tex[it->second].w; s.h = (float)A.tex[it->second].h; return s; }
}  // namespace menuart
