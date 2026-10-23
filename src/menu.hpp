// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Selector mínimo de nivel (plan M2): lista de los LVL/*.NGP de la ISO del jugador, flechas/AvPág/RePág + Enter, Esc sale.
// Texto provisional (src/text.hpp), sin datos del juego. Crea y destruye su propia ventana GL; el visor abre la suya después.
#pragma once
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "gl_renderer.hpp"
#include "text.hpp"

namespace menu {
// Lista de niveles con el elegido resaltado (y desplazamiento `top`), en píxeles de pantalla; la usan el selector con ventana propia y el selector sobre la escena SHELL/UI de dhview.
inline void drawList(const std::vector<std::string>& names, const std::string& title, size_t sel, size_t& top, int ww, int hh, std::vector<gfx::Quad>& q) {
    const float S = std::max(2.f, hh / 240.f), rowH = 10.f * S; const size_t rows = std::max<size_t>(3, (size_t)((hh * 0.72f) / rowH));
    if (sel < top) top = sel; if (sel >= top + rows) top = sel + 1 - rows;
    q.push_back({ww * 0.3f, hh * 0.04f, ww * 0.4f, hh * 0.9f, 0.f, 0.f, 0.f, 0.45f});
    gfx::textQuadsCentered(title, ww * 0.5f, hh * 0.06f, S * 2.f, 1.f, 0.85f, 0.2f, 1.f, q);
    for (size_t i = top; i < std::min(names.size(), top + rows); i++) {
        const float y = hh * 0.2f + (float)(i - top) * rowH; const bool on = i == sel;
        if (on) q.push_back({ww * 0.3f, y - S, ww * 0.4f, rowH, 0.25f, 0.3f, 0.5f, 0.8f});
        gfx::textQuadsCentered(names[i], ww * 0.5f, y, S * 1.0f, on ? 1.f : 0.7f, on ? 1.f : 0.7f, on ? 1.f : 0.7f, 1.f, q);
    }
    gfx::textQuadsCentered("ARROWS = CHOOSE   ENTER = RACE   ESC = QUIT", ww * 0.5f, hh * 0.94f, S, 0.6f, 0.6f, 0.6f, 1.f, q);
}
// Devuelve el nombre elegido o "" si el jugador sale. DH_MENU_PICK=N elige el índice N sin ventana (pruebas headless).
inline std::string pick(const std::vector<std::string>& names, const std::string& title) {
    if (names.empty()) return "";
    if (const char* p = std::getenv("DH_MENU_PICK")) { const std::string r = names[std::min<size_t>((size_t)std::atoi(p), names.size() - 1)]; std::fprintf(stderr, "menú: %zu niveles, elegido %s\n", names.size(), r.c_str()); return r; }
    if (!SDL_Init(SDL_INIT_VIDEO)) return "";
    SDL_Window* w = SDL_CreateWindow("dhview", 1280, 720, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    SDL_GLContext gl = w ? SDL_GL_CreateContext(w) : nullptr; if (!gl) { if (w) SDL_DestroyWindow(w); return ""; }
    SDL_GL_SetSwapInterval(1);
    gfx::GLRenderer gr; size_t sel = 0, top = 0; std::string out; bool run = true;
    while (run) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) run = false;
            if (e.type != SDL_EVENT_KEY_DOWN) continue;
            const SDL_Keycode k = e.key.key; const size_t n = names.size();
            if (k == SDLK_ESCAPE) run = false;
            else if (k == SDLK_RETURN || k == SDLK_KP_ENTER) { out = names[sel]; run = false; }
            else if (k == SDLK_DOWN) sel = (sel + 1) % n;
            else if (k == SDLK_UP) sel = (sel + n - 1) % n;
            else if (k == SDLK_PAGEDOWN) sel = std::min(n - 1, sel + 10);
            else if (k == SDLK_PAGEUP) sel = sel >= 10 ? sel - 10 : 0;
        }
        int ww = 0, hh = 0; SDL_GetWindowSizeInPixels(w, &ww, &hh);
        glViewport(0, 0, ww, hh); glClearColor(0.05f, 0.06f, 0.09f, 1.f); glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        std::vector<gfx::Quad> q; drawList(names, title, sel, top, ww, hh, q);
        gr.drawQuads2D(q, ww, hh); SDL_GL_SwapWindow(w);
    }
    SDL_GL_DestroyContext(gl); SDL_DestroyWindow(w);
    return out;
}
}  // namespace menu
