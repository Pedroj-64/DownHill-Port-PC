// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Fuente grande del juego (menús y HUD): atlas T8H 512x512 que el juego lleva en el .RTX (id 2, paleta 1) con celdas de 32x32 en cuadrícula 16x16, y la tabla carácter -> (columna, fila) de 256 entradas
// {código, columna, fila} del ejecutable (VA 0x2849A0 = desplazamiento de archivo 0x17B9A0; FUN_00215b90 la busca ordenada). Maquetación de FUN_00215628/FUN_00215858/FUN_00215100, validada contra volcados GS del
// menú principal a 0,02 px: cada letra es un sprite de la celda (UV = celda*32 px + 1 px, 32 px) dibujado a qw x qh; avance = 0,65*qw; antes de dibujar se desplaza s(c) y tras dibujar se avanza adv + s(c), con
// s = -1 px en i I l 1 : . (estrechas), +0,625 px en c C m M n N o O w W u U h H g G b B 8 (anchas), 0 en el resto; el espacio avanza adv. Coordenadas en el espacio de sprites 640 x 256 (campo entrelazado) de la pantalla PS2.
#pragma once
#include <algorithm>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#include "nativelevel.hpp"
#include "renderer.hpp"
#include "tex.hpp"

namespace menufont {
// Colores propios (PS2 0..128 = 1,0) de los iconos de botón del texto, de FUN_00215858: 14 = cruz (azul), 15 = círculo (rojo), 16 = triángulo (verde), 17 = cuadrado (rosa)
inline bool buttonTint(int c, float& r, float& g, float& b) { switch (c) { case 14: r = 19.f; g = 37.f; b = 57.f; return true; case 15: r = 53.f; g = 12.f; b = 7.f; return true; case 16: r = 10.f; g = 28.f; b = 10.f; return true; case 17: r = 30.f; g = 18.f; b = 26.f; return true; default: return false; } }
struct Font { gfx::Texture atlas; std::map<int, std::pair<int, int>> cell; bool ok = false; };
struct Style { float qw = 27.8f, qh = 16.6f, adv = 0.f; float advance() const { return adv > 0.f ? adv : 0.65f * qw; } };   // tamaño del sprite de cada letra y avance base en px del espacio 640x256 (menú principal: 27,8 x 16,6, avance 0,65*qw = 18,06; barra inferior: 21,8 x 10,4, avance 13,85); adv = 0 -> 0,65*qw
inline float shiftOf(int c) { switch (c) { case 'i': case 'I': case 'l': case '1': case ':': case '.': return -1.f;
        case 'c': case 'C': case 'm': case 'M': case 'n': case 'N': case 'o': case 'O': case 'w': case 'W': case 'u': case 'U': case 'h': case 'H': case 'g': case 'G': case 'b': case 'B': case '8': return 0.625f; default: return 0.f; } }

inline bool load(const nativelevel::Reader& rd, Font& F) {
    std::vector<uint8_t> n, p, t, c, elf; if (!rd("SHELL/UI.NGP", n) || !rd("SHELL/UI.PTR", p) || !rd("SHELL/UI.TEX", t) || !rd("SHELL/UI.RTX", c) || !rd("SLES_522.02", elf)) return false;
    tex::Level L; if (!tex::load(n, p, t, c, L)) return false;
    uint32_t tid = 2, cbp = 1; for (const tex::Material& m : L.mats) if (m.psm == 0x1b && m.w == 512 && m.h == 512) { tid = m.tid; cbp = m.cbp; break; }
    auto ci = L.cluts.find(cbp); if (ci == L.cluts.end()) return false;
    uint32_t W, H; std::vector<uint8_t> px; if (!tex::makeRtxTexture(c, tid, ci->second, W, H, px) || W != 512 || H != 512) return false;
    // El juego dibuja las letras con alfa de vértice 255 (= 1,99 en unidades PS2): alfa efectivo = alfa de textura x 1,99 con tope 1,0; como OpenGL recorta el color de vértice a 1, se hornea en el atlas.
    for (size_t i = 3; i < px.size(); i += 4) px[i] = (uint8_t)std::min(255, (int)px[i] * 255 / 128);
    F.atlas.w = (int)W; F.atlas.h = (int)H; F.atlas.rgba = std::move(px);
    const size_t off = 0x17B9A0; if (elf.size() < off + 256 * 12) return false;
    for (int i = 0; i < 256; i++) { int32_t v[3]; std::memcpy(v, &elf[off + 12 * (size_t)i], 12); if (v[1] >= 0 && v[1] < 16 && v[2] >= 0 && v[2] < 16) F.cell.emplace(v[0], std::make_pair(v[1], v[2])); }
    F.ok = !F.cell.empty(); return F.ok;
}
inline float width(const std::string& s, const Style& st) { const float adv = st.advance(); float x = 0; for (unsigned char ch : s) x += adv + (ch == ' ' ? 0.f : 2.f * shiftOf(ch)); return x; }
// Letras de `s` con el borde izquierdo en x (espacio 640x448); `u`/`v` normalizados (v = 0 arriba) -> TexQuad escalado por (sx, sy) con el color dado.
inline void quads(const Font& F, const std::string& s, float x, float y, const Style& st, float sx, float sy, float r, float g, float b, float a, std::vector<gfx::TexQuad>& out) {
    const float adv = st.advance();
    for (unsigned char ch : s) {
        if (ch == ' ') { x += adv; continue; }
        const float sh = shiftOf(ch); x += sh; auto it = F.cell.find(ch);
        if (it != F.cell.end()) { const float u0 = (it->second.first * 32.f + 1.f) / 512.f, v0 = (it->second.second * 32.f + 1.f) / 512.f; float cr = r, cg = g, cb = b, tr, tg, tb; if (buttonTint(ch, tr, tg, tb)) { cr = tr / 128.f; cg = tg / 128.f; cb = tb / 128.f; }   // los iconos de botón llevan su propio color
            out.push_back({x * sx, y * sy, st.qw * sx, st.qh * sy, u0, v0, u0 + 32.f / 512.f, v0 + 32.f / 512.f, cr, cg, cb, a}); }
        x += adv + sh;
    }
}
}  // namespace menufont
