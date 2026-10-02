// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// dhview: visor de .pts (puntos) y .msh (triángulos). Uso: dhview archivo   |  WASD + ratón, Shift = rápido, Esc = salir
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <bits/basic_string.h>

int main(int argc, char** argv) {
    if (argc < 2) { std::fprintf(stderr, "uso: dhview archivo.pts\n"); return 1; }
    std::vector<float> v, col;
    bool tris = std::string(argv[1]).ends_with(".msh");
    bool mdl = std::string(argv[1]).ends_with(".mdl");
    struct Tex { int w, h; std::vector<unsigned char> px; GLuint id = 0; };
    std::vector<Tex> texs; std::vector<float> mv; std::vector<uint32_t> mi;   // modelo texturizado
    {
        FILE* f = std::fopen(argv[1], "rb");
        if (!f) return 1;
        std::vector<uint8_t> raw;
        std::fseek(f, 0, SEEK_END); raw.resize(std::ftell(f)); std::rewind(f);
        if (std::fread(raw.data(), 1, raw.size(), f) != raw.size()) return 1;
        std::fclose(f);
        if (mdl) {
            uint32_t nt, nv, ni; std::memcpy(&nt, raw.data() + 4, 4); std::memcpy(&nv, raw.data() + 8, 4); std::memcpy(&ni, raw.data() + 12, 4);
            size_t o = 16;
            for (uint32_t k = 0; k < nt; k++) {
                Tex t; std::memcpy(&t.w, raw.data() + o, 4); std::memcpy(&t.h, raw.data() + o + 4, 4); o += 8;
                t.px.assign(raw.begin() + o, raw.begin() + o + (size_t)t.w * t.h * 4); o += (size_t)t.w * t.h * 4; texs.push_back(std::move(t));
            }
            mv.resize((size_t)nv * 9); std::memcpy(mv.data(), raw.data() + o, mv.size() * 4); o += mv.size() * 4;
            mi.resize(ni); std::memcpy(mi.data(), raw.data() + o, ni * 4);
            for (uint32_t k = 0; k < nv; k++) v.insert(v.end(), mv.begin() + k * 9, mv.begin() + k * 9 + 3);
        } else if (!tris) { v.resize(raw.size() / 4); std::memcpy(v.data(), raw.data(), v.size() * 4); }
        else {
            uint32_t nv, ni; std::memcpy(&nv, raw.data(), 4); std::memcpy(&ni, raw.data() + 4, 4);
            const float* P = (const float*)(raw.data() + 8); const uint32_t* I = (const uint32_t*)(P + nv * 3);
            for (uint32_t t = 0; t + 2 < ni; t += 3) {
                const float *a = P + I[t] * 3, *b = P + I[t+1] * 3, *c = P + I[t+2] * 3;
                float e1[3], e2[3], n[3];
                for (int k = 0; k < 3; k++) { e1[k] = b[k] - a[k]; e2[k] = c[k] - a[k]; }
                n[0] = e1[1]*e2[2] - e1[2]*e2[1]; n[1] = e1[2]*e2[0] - e1[0]*e2[2]; n[2] = e1[0]*e2[1] - e1[1]*e2[0];
                float len = std::sqrt(n[0]*n[0] + n[1]*n[1] + n[2]*n[2]) + 1e-9f;
                float sh = 0.3f + 0.7f * std::fabs(n[1] / len);   // luz cenital; doble cara
                for (const float* q : {a, b, c}) { v.insert(v.end(), q, q + 3); col.insert(col.end(), {0.45f * sh, 0.8f * sh, 0.55f * sh}); }
            }
        }
    }
    size_t n = v.size() / 3;
    if (!tris && !mdl) col.resize(n * 3);
    float ymin = 1e30f, ymax = -1e30f;
    for (size_t i = 0; i < n; i++) { ymin = std::fmin(ymin, v[i*3+1]); ymax = std::fmax(ymax, v[i*3+1]); }
    if (!tris && !mdl) for (size_t i = 0; i < n; i++) {
        float t = (v[i*3+1] - ymin) / (ymax - ymin);
        col[i*3] = 0.3f + 0.7f * t; col[i*3+1] = 0.9f - 0.5f * t; col[i*3+2] = 0.4f + 0.4f * (1 - t);
    }
    if (!SDL_Init(SDL_INIT_VIDEO)) return 1;
    SDL_Window* w = SDL_CreateWindow("dhview", 1280, 720, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    SDL_GLContext gl = SDL_GL_CreateContext(w);
    SDL_GL_SetSwapInterval(1);
    for (auto& t : texs) {
        glGenTextures(1, &t.id); glBindTexture(GL_TEXTURE_2D, t.id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, t.w, t.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, t.px.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    }
    SDL_SetWindowRelativeMouseMode(w, true);
    float px = v[0], py = v[1] + (mdl ? 0.f : 300.f), pz = v[2] + (mdl ? 8.f : 0.f), yaw = 0, pitch = mdl ? 0.f : -0.4f;
    if (const char* c = std::getenv("DH_CAM")) std::sscanf(c, "%f %f %f %f %f", &px, &py, &pz, &yaw, &pitch);  // depuración: "x y z yaw pitch"
    const char* shot = std::getenv("DH_SHOT");  // depuración: guarda el 3er fotograma como BMP y sale
    int frame = 0;
    Uint64 last = SDL_GetTicks();
    for (bool run = true; run;) {
        for (SDL_Event e; SDL_PollEvent(&e);) {
            if (e.type == SDL_EVENT_QUIT || (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE)) run = false;
            if (e.type == SDL_EVENT_MOUSE_MOTION) { yaw += e.motion.xrel * 0.003f; pitch -= e.motion.yrel * 0.003f; }
        }
        pitch = std::fmax(-1.55f, std::fmin(1.55f, pitch));
        Uint64 now = SDL_GetTicks(); float dt = (now - last) / 1000.f; last = now;
        const bool* k = SDL_GetKeyboardState(nullptr);
        float sp = (mdl ? (k[SDL_SCANCODE_LSHIFT] ? 12.f : 4.f) : (k[SDL_SCANCODE_LSHIFT] ? 4000.f : 800.f)) * dt;
        float fx = std::sin(yaw) * std::cos(pitch), fy = std::sin(pitch), fz = -std::cos(yaw) * std::cos(pitch);
        float rx = std::cos(yaw), rz = std::sin(yaw);
        if (k[SDL_SCANCODE_W]) { px += fx*sp; py += fy*sp; pz += fz*sp; }
        if (k[SDL_SCANCODE_S]) { px -= fx*sp; py -= fy*sp; pz -= fz*sp; }
        if (k[SDL_SCANCODE_D]) { px += rx*sp; pz += rz*sp; }
        if (k[SDL_SCANCODE_A]) { px -= rx*sp; pz -= rz*sp; }
        int ww, hh; SDL_GetWindowSizeInPixels(w, &ww, &hh);
        glViewport(0, 0, ww, hh); glClearColor(0.05f, 0.06f, 0.09f, 1); glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        glMatrixMode(GL_PROJECTION); glLoadIdentity();
        float asp = (float)ww / hh, nr = mdl ? 0.05f : 5, fr = 60000, t = nr * std::tan(0.5f * 1.1f);
        glFrustum(-t*asp, t*asp, -t, t, nr, fr);
        glMatrixMode(GL_MODELVIEW); glLoadIdentity();
        glRotatef(-pitch * 57.2958f, 1, 0, 0); glRotatef(yaw * 57.2958f, 0, 1, 0); glTranslatef(-px, -py, -pz);
        if (mdl) {
            glEnable(GL_TEXTURE_2D); glDisable(GL_CULL_FACE); glColor3f(1, 1, 1);
            glEnableClientState(GL_VERTEX_ARRAY); glEnableClientState(GL_TEXTURE_COORD_ARRAY);
            glVertexPointer(3, GL_FLOAT, 36, mv.data()); glTexCoordPointer(2, GL_FLOAT, 36, mv.data() + 3);
            for (size_t t = 0; t + 2 < mi.size(); ) {                       // agrupa por textura consecutiva
                int tid = (int)mv[mi[t] * 9 + 8]; size_t e = t;
                while (e + 2 < mi.size() && (int)mv[mi[e] * 9 + 8] == tid) e += 3;
                if (tid >= 0 && tid < (int)texs.size()) glBindTexture(GL_TEXTURE_2D, texs[tid].id); else glBindTexture(GL_TEXTURE_2D, 0);
                glDrawElements(GL_TRIANGLES, (GLsizei)(e - t), GL_UNSIGNED_INT, mi.data() + t); t = e;
            }
            glDisableClientState(GL_TEXTURE_COORD_ARRAY); glDisable(GL_TEXTURE_2D);
        } else {
            glEnableClientState(GL_VERTEX_ARRAY); glEnableClientState(GL_COLOR_ARRAY);
            glVertexPointer(3, GL_FLOAT, 0, v.data()); glColorPointer(3, GL_FLOAT, 0, col.data());
            glPointSize(2); glDrawArrays(tris ? GL_TRIANGLES : GL_POINTS, 0, (GLsizei)n);
        }
        if (shot && ++frame == 3) {
            std::vector<unsigned char> px4((size_t)ww * hh * 4);
            glReadPixels(0, 0, ww, hh, GL_RGBA, GL_UNSIGNED_BYTE, px4.data());
            std::vector<unsigned char> flip(px4.size());
            for (int y = 0; y < hh; y++) std::memcpy(&flip[(size_t)y * ww * 4], &px4[(size_t)(hh - 1 - y) * ww * 4], (size_t)ww * 4);
            SDL_Surface* sf = SDL_CreateSurfaceFrom(ww, hh, SDL_PIXELFORMAT_RGBA32, flip.data(), ww * 4);
            SDL_SaveBMP(sf, shot); SDL_DestroySurface(sf); run = false;
        }
        SDL_GL_SwapWindow(w);
    }
    SDL_GL_DestroyContext(gl); SDL_DestroyWindow(w); SDL_Quit();
}
