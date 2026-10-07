// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Pantalla de carga con la imagen real del juego (LOADBAR, 78 archivos 512x512). Sin GL. Los datos se leen en ejecución de out/loadbar/<NOMBRE>.lbr, que genera tools/loadbar_export.py a partir de los
// datos del usuario (nunca van al repo). Formato: 'DLBR', u32 w, u32 h, RGBA con las filas de ARRIBA a ABAJO.
// MEDIDO sobre las 78 imágenes: el cuadro ocupa todo el ancho o un trapecio (x desde ~112-120) y termina en la fila ~400; por debajo la textura es negra (banda de la barra de progreso).
// HIPÓTESIS (no medida en el juego): la imagen se dibuja a ancho completo con su proporción y la barra va en la banda inferior; la barra no tiene textura propia localizada; colores y duración, provisionales.
#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>
#include "renderer.hpp"

namespace loadscreen {
inline bool load(const std::vector<uint8_t>& d, gfx::Texture& t) {
    if (d.size() < 12 || std::memcmp(d.data(), "DLBR", 4) != 0) return false;
    uint32_t w, h; std::memcpy(&w, d.data() + 4, 4); std::memcpy(&h, d.data() + 8, 4);
    if (w == 0 || h == 0 || w > 1024 || h > 1024 || d.size() < 12 + (size_t)w * h * 4) return false;      // entrada no confiable: se acotan tamaños
    t.w = (int)w; t.h = (int)h; t.rgba.assign(d.begin() + 12, d.begin() + 12 + (size_t)w * h * 4); return true;
}

// Fila (exclusiva) donde acaba el cuadro: la última con >= 5 % de píxeles claros, ignorando 2 px de borde; devuelve h si no hay ninguna (se usa la imagen entera).
inline int pictureBottom(const gfx::Texture& t) {
    int last = -1;
    for (int y = 2; y < t.h - 2; y++) {
        int n = 0; for (int x = 2; x < t.w - 2; x++) { const uint8_t* p = &t.rgba[((size_t)y * t.w + x) * 4]; if (std::max({p[0], p[1], p[2]}) > 24) n++; }
        if (n * 20 >= t.w) last = y;
    }
    return last < 0 ? t.h : last + 1;
}

// Nombre de archivo (sin extensión) de la pantalla de carga para un nivel: prefijo de 3 letras + modo (MX = mountain cross, TD = contrarreloj/dual). HIPÓTESIS: el significado de los sufijos sale de los nombres, no del ELF.
inline std::vector<std::string> candidates(const std::string& level) {
    std::string n; for (char c : level) n += (char)std::toupper((unsigned char)c);
    static const char* pre[][2] = {{"ALP", "LALP"}, {"AUB", "LAUB"}, {"BC", "LBC"}, {"CIT", "LCIT"}, {"FUJ", "LFUJ"}, {"GLA", "LGLA"}, {"JUN", "LJUN"}, {"MOA", "LMOA"}, {"PER", "LPER"}};
    std::string base; for (auto& p : pre) if (n.rfind(p[0], 0) == 0) base = p[1];
    std::vector<std::string> out; if (base.empty()) { out = {"LOADMX", "LOADARC"}; return out; }
    bool mx = n.find("MX") != std::string::npos, td = n.find("TD") != std::string::npos || (n.size() > 1 && n.back() == 'T' && !mx);
    out.push_back(base + (td ? "TD" : "MX")); for (const char* s : {"MX", "AR", "TD", "FR", "FS", "SC", "SE"}) out.push_back(base + s); out.push_back("LOADMX"); return out;
}
// Busca en `dir` la primera candidata existente; devuelve la ruta o "".
inline std::string find(const std::string& dir, const std::string& level) {
    namespace fs = std::filesystem; std::error_code ec;
    for (const auto& c : candidates(level)) { fs::path p = fs::path(dir) / (c + ".lbr"); if (fs::exists(p, ec)) return p.string(); }
    return "";
}

// Maqueta en vw x vh: imagen (filas 0..bottom de la textura) a la proporción original dentro de la zona superior (88 %), y barra de progreso en la banda inferior. progress 0..1.
struct Layout { gfx::TexQuad pic; std::vector<gfx::Quad> bar; };
inline Layout layout(const gfx::Texture& t, int vw, int vh, float progress) {
    Layout L; int bot = pictureBottom(t); const float zoneH = vh * 0.88f, ar = (float)t.w / (float)bot;
    float w = zoneH * ar, h = zoneH; if (w > vw) { w = (float)vw; h = w / ar; }
    L.pic = {(vw - w) * 0.5f, (zoneH - h) * 0.5f, w, h, 0.f, 0.f, 1.f, (float)bot / (float)t.h, 1, 1, 1, 1};
    progress = std::min(1.f, std::max(0.f, progress));
    const float bx = vw * 0.15f, bw = vw * 0.70f, by = vh * 0.94f, bh = std::max(4.f, vh * 0.025f), b = std::max(1.f, vh / 240.f);
    L.bar.push_back({bx - b, by - b, bw + 2 * b, bh + 2 * b, 0.8f, 0.8f, 0.8f, 1.f}); L.bar.push_back({bx, by, bw, bh, 0.05f, 0.05f, 0.05f, 1.f});
    if (progress > 0.f) L.bar.push_back({bx, by, bw * progress, bh, 0.95f, 0.55f, 0.1f, 1.f});
    return L;
}
}  // namespace loadscreen
