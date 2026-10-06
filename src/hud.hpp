// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// HUD con las texturas del juego (ordenador de la bici: carcasa, pantalla LCD de siete segmentos, rótulo KPH), sin GL. Los datos se leen en tiempo de ejecución de out/hud/hud.dat, que genera
// tools/hud_export.py a partir de los datos del usuario (nunca van al repo). Maquetación medida sobre capturas reales de 640x480 (docs/formats/ui-reference.md); todo lo no medido es hipótesis.
#pragma once
#include <cstdint>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "renderer.hpp"

namespace hud {
enum Tex { kLogo, kMph, kKph, kKpo, kLcdFont, kStrip, kHousing, kRiderLogos, kTexCount };

// Lee hud.dat: 'DHUD', u32 n, n x {u32 w, u32 h, w*h*4 RGBA con la fila de abajo primero}. Devuelve las texturas con las filas de arriba abajo. Entrada no confiable: se acotan tamaños.
inline bool load(const std::vector<uint8_t>& d, std::vector<gfx::Texture>& out) {
    out.clear(); if (d.size() < 8 || std::memcmp(d.data(), "DHUD", 4) != 0) return false;
    uint32_t n; std::memcpy(&n, d.data() + 4, 4); if (n != kTexCount) return false; size_t p = 8;
    for (uint32_t i = 0; i < n; i++) {
        if (p + 8 > d.size()) return false; uint32_t w, h; std::memcpy(&w, d.data() + p, 4); std::memcpy(&h, d.data() + p + 4, 4); p += 8;
        if (w == 0 || h == 0 || w > 1024 || h > 1024 || p + (size_t)w * h * 4 > d.size()) return false;
        gfx::Texture t; t.w = (int)w; t.h = (int)h; t.rgba.resize((size_t)w * h * 4);
        for (uint32_t y = 0; y < h; y++) std::memcpy(t.rgba.data() + (size_t)y * w * 4, d.data() + p + (size_t)(h - 1 - y) * w * 4, (size_t)w * 4);   // voltea: v = 0 arriba
        out.push_back(std::move(t)); p += (size_t)w * h * 4;
    }
    // La carcasa trae en las esquinas muestras de color de los leds (verde arriba a la izquierda; rojo, azul y blanco arriba a la derecha): se hacen transparentes.
    gfx::Texture& hs = out[kHousing]; if (hs.w == 128 && hs.h == 128) {
        auto clear = [&](int x0, int y0, int x1, int y1) { for (int y = y0; y < y1; y++) for (int x = x0; x < x1; x++) std::memset(&hs.rgba[((size_t)y * 128 + x) * 4], 0, 4); };
        clear(0, 0, 11, 13); clear(115, 0, 128, 29);
        // La textura no trae alfa: el juego la recorta con un polígono redondeado. Aproximación (HIPÓTESIS medida a ojo sobre la textura): rectángulo redondeado x 8..126, y 6..123, radio 18.
        const float x0 = 8, y0 = 6, x1 = 126, y1 = 123, rad = 18;
        for (int y = 0; y < 128; y++) for (int x = 0; x < 128; x++) {
            float cx = std::fmin(std::fmax(x + 0.5f, x0 + rad), x1 - rad), cy = std::fmin(std::fmax(y + 0.5f, y0 + rad), y1 - rad), dx = x + 0.5f - cx, dy = y + 0.5f - cy;
            if (x + 0.5f < x0 || x + 0.5f > x1 || y + 0.5f < y0 || y + 0.5f > y1 || dx * dx + dy * dy > rad * rad) std::memset(&hs.rgba[((size_t)y * 128 + x) * 4], 0, 4);
        }
    }
    return true;
}

struct Rect { float x, y, w, h; };                                   // en píxeles de la textura
// Fuente LCD (textura 128x128, medida): celdas de 24 de ancho y 42 de alto; el «1» ocupa solo 7 px a la derecha de su celda; el «6» no existe (es el «9» girado 180°).
inline bool lcdGlyph(char c, Rect& r, bool& rot180, float& adv) {
    rot180 = false; adv = 26.f;
    switch (c) {
        case '1': r = {2, 3, 7, 42}; adv = 26.f; return true;  case '2': r = {12, 3, 24, 42}; return true;  case '3': r = {38, 3, 23, 42}; return true;
        case '4': r = {63, 3, 24, 42}; return true;            case '5': r = {90, 3, 24, 42}; return true;  case '6': r = {56, 49, 24, 42}; rot180 = true; return true;
        case '7': r = {2, 49, 24, 42}; return true;            case '8': r = {29, 49, 24, 42}; return true; case '9': r = {56, 49, 24, 42}; return true;
        case '0': r = {85, 49, 24, 42}; return true;           case ':': r = {115, 49, 5, 42}; adv = 9.f; return true;
        case '-': r = {3, 100, 42, 17}; return true;           case '+': r = {108, 100, 17, 17}; return true;
        case ' ': r = {0, 0, 0, 0}; return true;
        default: return false;
    }
}

// Cadena LCD con altura `h` (píxeles de pantalla) y borde derecho en `xr` (alinea a la derecha). Los dígitos se dibujan con el color `c` (la textura ya es verde claro: se multiplica).
inline void lcdText(const std::string& s, float xr, float y, float h, const float c[4], std::vector<gfx::TexQuad>& out) {
    const float f = h / 42.f; float total = 0;
    for (char ch : s) { Rect r; bool rot; float adv; if (lcdGlyph(ch, r, rot, adv)) total += adv * f; }
    float x = xr - total + 2.f * f;                                     // quita la separación final
    for (char ch : s) {
        Rect r; bool rot; float adv; if (!lcdGlyph(ch, r, rot, adv)) continue;
        if (r.w > 0) {
            float dx = ch == '1' ? (24.f - r.w) * f : 0.f, dw = r.w * f, dh = r.h * f, dy = (ch == '-' || ch == '+') ? (42.f - r.h) * 0.5f * f : 0.f;
            float u0 = r.x / 128.f, u1 = (r.x + r.w) / 128.f, v0 = r.y / 128.f, v1 = (r.y + r.h) / 128.f;
            if (rot) { std::swap(u0, u1); std::swap(v0, v1); }
            out.push_back({x + dx, y + dy, dw, dh, u0, v0, u1, v1, c[0], c[1], c[2], c[3]});
        }
        x += adv * f;
    }
}

// Ordenador de la bici en 640x480 de referencia: carcasa x 500-628, y 368-460; velocidad y 392 (altura 21, borde derecho 566), reloj y 418 (altura 16, borde 585), tercera cifra y 438 (altura 13, borde 590),
// rótulo KPH x 570-592, y 395-407. view = tamaño real; la escala es uniforme (vh/480) y se ancla abajo a la derecha.
struct Layout { float s, ox, oy; };
inline Layout anchor(int vw, int vh) { float s = vh / 480.f; return {s, vw - 640.f * s, 0.f}; }
inline void bikeComputer(int vw, int vh, int speedKph, const std::string& clock, const std::string& third, std::vector<gfx::TexQuad>& housing, std::vector<gfx::TexQuad>& lcd, std::vector<gfx::TexQuad>& label) {
    Layout L = anchor(vw, vh); const float white[4] = {1, 1, 1, 1};
    auto X = [&](float x) { return L.ox + x * L.s; }; auto Y = [&](float y) { return L.oy + y * L.s; };
    housing.push_back({X(500), Y(368), 128 * L.s, 92 * L.s, 0, 0, 1, 1, 1, 1, 1, 1});
    char b[16]; std::snprintf(b, sizeof b, "%d", speedKph < 0 ? 0 : speedKph > 999 ? 999 : speedKph);
    const float dark[4] = {0.12f, 0.18f, 0.12f, 1.f};                   // la pantalla es verde claro con cifras oscuras: se tiñe la fuente (hipótesis: en la textura son verde claro sobre fondo transparente)
    lcdText(b, X(566), Y(392), 21 * L.s, dark, lcd);
    lcdText(clock, X(585), Y(418), 16 * L.s, dark, lcd);
    lcdText(third, X(590), Y(438), 13 * L.s, dark, lcd);
    label.push_back({X(570), Y(395), 22 * L.s, 12 * L.s, 6 / 64.f, 2 / 64.f, 62 / 64.f, 21 / 64.f, dark[0], dark[1], dark[2], 1});   // rótulo KPH (textura kKph, filas 2-20)
    (void)white;
}

// Leds y línea de la pantalla (colores planos: la pastilla verde de la textura es (15,192,19)). Ranuras medidas en la textura de la carcasa (x, y, ancho, alto en px de 128x128) y llevadas al rectángulo 128x92 de pantalla.
inline void bikeComputerLeds(int vw, int vh, int speedKph, std::vector<gfx::Quad>& out) {
    Layout L = anchor(vw, vh);
    auto map = [&](float x, float y, float w, float h, float r, float g, float b) { out.push_back({L.ox + (500 + x) * L.s, L.oy + (368 + y * 0.71875f) * L.s, w * L.s, h * 0.71875f * L.s, r, g, b, 1.f}); };
    static const float left[5][4] = {{18, 22, 10, 11}, {14, 38, 10, 11}, {11, 54, 10, 11}, {9, 70, 10, 11}, {8, 86, 10, 11}};
    for (const auto& q : left) map(q[0], q[1], q[2], q[3], 15 / 255.f, 192 / 255.f, 19 / 255.f);                 // 5 leds verdes encendidos (como en las capturas de carrera)
    static const float right[10][4] = {{100, 22, 8, 7}, {102, 31, 9, 7}, {104, 40, 9, 7}, {105, 49, 10, 7}, {106, 58, 11, 7}, {106, 67, 12, 7}, {106, 76, 13, 8}, {106, 86, 14, 8}, {106, 96, 14, 8}, {102, 106, 17, 9}};
    int lit = 0;                                                                                                // barra de la derecha: solo en los modos con puntuación (HIPÓTESIS); en contrarreloj apagada
    for (int i = 0; i < lit && i < 10; i++) { float t = i / 9.f; map(right[9 - i][0], right[9 - i][1], right[9 - i][2], right[9 - i][3], 1.f, 0.8f - 0.6f * t, 0.1f); }
    (void)speedKph;
    out.push_back({L.ox + 528 * L.s, L.oy + 435.5f * L.s, 68 * L.s, 1.5f * L.s, 0.12f, 0.18f, 0.12f, 1.f});   // línea que separa la tercera cifra
}
}  // namespace hud
