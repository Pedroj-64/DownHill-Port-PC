// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Texturas de interfaz extraídas en nativo, mismos bytes que tools/hud_export.py (bloque del HUD en las texturas del nivel) y tools/loadbar_export.py (imagen 512x512 de las pantallas de carga LOADBAR/*.NGP).
// Formatos: hud 'DHUD' u32 n + n x {u32 w, u32 h, RGBA}; loadbar 'DLBR' u32 w, u32 h, RGBA con las filas de ARRIBA a ABAJO (la textura viene boca abajo, se voltea).
#pragma once
#include "tex.hpp"
#include <algorithm>

namespace uiextract {
// hud_export.py: primera textura de cada id (en el orden de los materiales del PTR), ordenadas por id; el bloque se localiza por la secuencia de tamaños. false si no hay bloque.
inline bool hudDat(const tex::Level& L, std::vector<uint8_t>& out) {
    static const uint32_t SEQ[8][2] = {{256, 128}, {64, 64}, {64, 64}, {64, 64}, {128, 128}, {128, 32}, {128, 128}, {256, 128}};
    struct T { uint32_t id, w, h; std::vector<uint8_t> px; bool ok; }; std::vector<T> tex; std::set<uint32_t> seen;
    for (const tex::Material& m : L.mats) {
        if (seen.count(m.tid) || !L.texs.count(m.tid)) continue; seen.insert(m.tid);
        T t{m.tid, 0, 0, {}, false}; t.ok = tex::makeTexture(L, m, t.w, t.h, t.px); tex.push_back(std::move(t));
    }
    std::stable_sort(tex.begin(), tex.end(), [](const T& a, const T& b) { return a.id < b.id; });
    for (size_t i = 0; i + 8 <= tex.size(); i++) {
        bool all = true; for (size_t k = 0; k < 8 && all; k++) all = tex[i + k].ok && tex[i + k].id == tex[i].id + k && tex[i + k].w == SEQ[k][0] && tex[i + k].h == SEQ[k][1];
        if (!all) continue;
        out.assign({'D', 'H', 'U', 'D'}); const uint32_t n = 8; out.insert(out.end(), (const uint8_t*)&n, (const uint8_t*)&n + 4);
        for (size_t k = 0; k < 8; k++) { out.insert(out.end(), (const uint8_t*)&tex[i + k].w, (const uint8_t*)&tex[i + k].w + 4); out.insert(out.end(), (const uint8_t*)&tex[i + k].h, (const uint8_t*)&tex[i + k].h + 4); out.insert(out.end(), tex[i + k].px.begin(), tex[i + k].px.end()); }
        return true;
    }
    return false;
}
// loadbar_export.py: primer material con textura PSMT8 lineal (fmt 19) de 512x512 de la tabla de texturas; se decodifica y se voltea en vertical. false si no hay imagen o no se puede decodificar.
inline bool loadbarLbr(const tex::Level& L, std::vector<uint8_t>& out) {
    for (const tex::Material& m : L.mats) {
        auto it = L.texs.find(m.tid); if (it == L.texs.end() || it->second.fmt != 19 || it->second.w != 512 || it->second.h != 512) continue;
        uint32_t w, h; std::vector<uint8_t> px; if (!tex::makeTexture(L, m, w, h, px)) return false;
        out.assign({'D', 'L', 'B', 'R'}); out.insert(out.end(), (const uint8_t*)&w, (const uint8_t*)&w + 4); out.insert(out.end(), (const uint8_t*)&h, (const uint8_t*)&h + 4);
        for (uint32_t y = 0; y < h; y++) out.insert(out.end(), px.begin() + (size_t)(h - 1 - y) * w * 4, px.begin() + (size_t)(h - y) * w * 4);
        return true;
    }
    return false;
}
}  // namespace uiextract
