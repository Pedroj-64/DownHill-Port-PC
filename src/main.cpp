// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// dhview: visor de .pts (puntos) y .msh (triángulos). Uso: dhview archivo   |  WASD + ratón, Shift = rápido, Esc = salir
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <bits/basic_string.h>
#include "gates.hpp"
#include "ground.hpp"
#include "ride.hpp"

int main(int argc, char** argv) {
    if (argc < 2) { std::fprintf(stderr, "uso: dhview archivo.pts|.msh|.mdl  (mdl: F = volar/caminar, R = bici, Espacio = saltar)\n"); return 1; }
    std::vector<float> v, col;
    bool tris = std::string(argv[1]).ends_with(".msh");
    bool mdl = std::string(argv[1]).ends_with(".mdl");
    struct Tex { int w, h; std::vector<unsigned char> px; GLuint id = 0; };
    struct Chunk { uint32_t first, count; std::vector<std::array<float, 5>> sels; };   // rango de índices + selectores de distancia (cx cy cz radio dist2_max)
    struct Model { std::vector<Tex> texs; std::vector<float> mv; std::vector<uint32_t> mi; std::vector<Chunk> chunks; };   // .mdl: 10 f32 por vértice (x y z u v r g b a tex)
    auto readFile = [](const std::string& path, std::vector<uint8_t>& raw) {
        FILE* f = std::fopen(path.c_str(), "rb"); if (!f) return false;
        std::fseek(f, 0, SEEK_END); raw.resize(std::ftell(f)); std::rewind(f);
        bool ok = std::fread(raw.data(), 1, raw.size(), f) == raw.size(); std::fclose(f); return ok;
    };
    auto parseMdl = [](const std::vector<uint8_t>& raw, Model& m) {        // valida magia y tamaños antes de leer
        bool v3 = raw.size() >= 4 && !std::memcmp(raw.data(), "DHM3", 4);
        if (raw.size() < 16 || (!v3 && std::memcmp(raw.data(), "DHM2", 4))) return false;
        uint32_t nt, nv, ni; std::memcpy(&nt, raw.data() + 4, 4); std::memcpy(&nv, raw.data() + 8, 4); std::memcpy(&ni, raw.data() + 12, 4);
        size_t o = 16;
        for (uint32_t k = 0; k < nt; k++) {
            if (o + 8 > raw.size()) return false;
            Tex t; std::memcpy(&t.w, raw.data() + o, 4); std::memcpy(&t.h, raw.data() + o + 4, 4); o += 8;
            size_t sz = (size_t)t.w * t.h * 4; if (t.w <= 0 || t.h <= 0 || o + sz > raw.size()) return false;
            t.px.assign(raw.begin() + o, raw.begin() + o + sz); o += sz; m.texs.push_back(std::move(t));
        }
        if (o + (size_t)nv * 40 + (size_t)ni * 4 > raw.size()) return false;
        m.mv.resize((size_t)nv * 10); std::memcpy(m.mv.data(), raw.data() + o, m.mv.size() * 4); o += m.mv.size() * 4;
        m.mi.resize(ni); std::memcpy(m.mi.data(), raw.data() + o, ni * 4);
        for (uint32_t i : m.mi) if (i >= nv) return false;
        o += (size_t)ni * 4;
        if (v3) {                                         // tabla de chunks (DHM3)
            if (o + 4 > raw.size()) return false;
            uint32_t nc; std::memcpy(&nc, raw.data() + o, 4); o += 4;
            for (uint32_t c = 0; c < nc; c++) {
                if (o + 12 > raw.size()) return false;
                Chunk ch; uint32_t ns; std::memcpy(&ch.first, raw.data() + o, 4); std::memcpy(&ch.count, raw.data() + o + 4, 4); std::memcpy(&ns, raw.data() + o + 8, 4); o += 12;
                if ((size_t)ch.first + ch.count > ni || ns > 64 || o + (size_t)ns * 20 > raw.size()) return false;
                for (uint32_t q = 0; q < ns; q++) { std::array<float, 5> a; std::memcpy(a.data(), raw.data() + o, 20); o += 20; ch.sels.push_back(a); }
                m.chunks.push_back(std::move(ch));
            }
        }
        return true;
    };
    // El NGP (y por tanto el .mdl, el .PTS y la colisión) usa Z arriba (docs/formats/coordinates.md); dhview trabaja en Y arriba: (x,y,z) -> (x,z,-y). DH_RAW=1 desactiva la conversión.
    const bool rawAxes = std::getenv("DH_RAW") != nullptr;
    auto toYUp = [&](Model& m) { if (rawAxes) return; for (size_t k = 0; k + 9 < m.mv.size(); k += 10) { float y = m.mv[k+1], z = m.mv[k+2]; m.mv[k+1] = z; m.mv[k+2] = -y; }
        for (auto& c : m.chunks) for (auto& q : c.sels) { float y = q[1], z = q[2]; q[1] = z; q[2] = -y; } };
    Model M, sky, bike;   // bike: DH_BIKE=bici_ensamblada.mdl (tools/assemble_bike.py), se dibuja en el modo bici
    {
        std::vector<uint8_t> raw;
        if (!readFile(argv[1], raw)) { std::fprintf(stderr, "no se puede leer %s\n", argv[1]); return 1; }
        if (mdl) {
            if (!parseMdl(raw, M)) { std::fprintf(stderr, "%s: .mdl inválido (¿DHM1 antiguo? vuelve a extraer)\n", argv[1]); return 1; }
            toYUp(M);
            for (size_t k = 0; k + 9 < M.mv.size(); k += 10) v.insert(v.end(), M.mv.begin() + k, M.mv.begin() + k + 3);
            if (const char* bp = std::getenv("DH_BIKE")) { std::vector<uint8_t> braw; if (!readFile(bp, braw) || !parseMdl(braw, bike)) { std::fprintf(stderr, "aviso: DH_BIKE=%s no válido\n", bp); bike = Model(); } }
            std::vector<uint8_t> sraw; std::string sp = std::string(argv[1]); sp = sp.substr(0, sp.size() - 4) + ".sky.mdl";
            if (readFile(sp, sraw) && !parseMdl(sraw, sky)) { std::fprintf(stderr, "aviso: %s inválido, se ignora\n", sp.c_str()); sky = Model(); } else toYUp(sky);
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
    int winW = 1280, winH = 720; if (const char* ws = std::getenv("DH_SIZE")) std::sscanf(ws, "%d %d", &winW, &winH);   // DH_SIZE="640 480": ventana para comparar con capturas 4:3 del juego
    SDL_Window* w = SDL_CreateWindow("dhview", winW, winH, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    SDL_GLContext gl = SDL_GL_CreateContext(w);
    SDL_GL_SetSwapInterval(1);
    for (Model* m : {&M, &sky, &bike}) for (auto& t : m->texs) {
        glGenTextures(1, &t.id); glBindTexture(GL_TEXTURE_2D, t.id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, t.w, t.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, t.px.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    }
    SDL_SetWindowRelativeMouseMode(w, true);
    float px = v.empty() ? 0.f : v[0], py = v.empty() ? 0.f : v[1] + (mdl ? 0.f : 300.f), pz = v.empty() ? 0.f : v[2] + (mdl ? 8.f : 0.f), yaw = 0, pitch = mdl ? 0.f : -0.4f;
    float scale = 1.f;   // escala del modelo (para velocidad de cámara)
    if (mdl && !v.empty()) {                  // encuadre automático: centro del bbox, a 2.2 radios por delante
        float mn[3], mx[3];                      // caja robusta: percentiles 2 %-98 % por eje (ignora geometría suelta lejana)
        for (int a = 0; a < 3; a++) {
            std::vector<float> c; c.reserve(v.size() / 3); for (size_t i = 0; i + 2 < v.size(); i += 3) c.push_back(v[i + a]);
            std::sort(c.begin(), c.end()); mn[a] = c[c.size() * 2 / 100]; mx[a] = c[c.size() - 1 - c.size() * 2 / 100];
        }
        float r = 0.5f * std::sqrt((mx[0]-mn[0])*(mx[0]-mn[0]) + (mx[1]-mn[1])*(mx[1]-mn[1]) + (mx[2]-mn[2])*(mx[2]-mn[2])) + 1e-3f;
        px = 0.5f * (mn[0] + mx[0]); py = 0.5f * (mn[1] + mx[1]); pz = 0.5f * (mn[2] + mx[2]) + 2.2f * r; scale = r;
        if (const char* ob = std::getenv("DH_ORBIT")) {          // DH_ORBIT="yaw pitch" (rad): cámara en órbita alrededor del centro del modelo, mirándolo
            float oy = 0, op = 0; std::sscanf(ob, "%f %f", &oy, &op);
            float cx = px, cy = py, cz = pz - 2.2f * r, d = 2.2f * r;
            px = cx - std::sin(oy) * std::cos(op) * d; py = cy - std::sin(op) * d; pz = cz + std::cos(oy) * std::cos(op) * d; yaw = oy; pitch = op;
        }
    }
    if (const char* c = std::getenv("DH_CAM")) std::sscanf(c, "%f %f %f %f %f", &px, &py, &pz, &yaw, &pitch);  // depuración: "x y z yaw pitch"
    const char* shot = std::getenv("DH_SHOT");  // depuración: guarda el 3er fotograma como BMP y sale
    // --- Colisión con el terreno: rejilla XZ de triángulos; Y es "arriba" ---
    const float cell = std::fmax(1.f, scale * 0.02f);
    float gx0 = 1e30f, gz0 = 1e30f, gx1 = -1e30f, gz1 = -1e30f; int gnx = 0, gnz = 0; std::vector<std::vector<uint32_t>> grid;
    if (mdl && !v.empty()) {
        for (size_t i = 0; i + 2 < v.size(); i += 3) { gx0 = std::fmin(gx0, v[i]); gx1 = std::fmax(gx1, v[i]); gz0 = std::fmin(gz0, v[i+2]); gz1 = std::fmax(gz1, v[i+2]); }
        gnx = (int)((gx1 - gx0) / cell) + 1; gnz = (int)((gz1 - gz0) / cell) + 1; grid.resize((size_t)gnx * gnz);
        for (size_t t = 0; t + 2 < M.mi.size(); t += 3) {
            const float* q[3] = {&v[M.mi[t]*3], &v[M.mi[t+1]*3], &v[M.mi[t+2]*3]};
            float lx = std::fmin(q[0][0], std::fmin(q[1][0], q[2][0])), hx = std::fmax(q[0][0], std::fmax(q[1][0], q[2][0]));
            float lz = std::fmin(q[0][2], std::fmin(q[1][2], q[2][2])), hz = std::fmax(q[0][2], std::fmax(q[1][2], q[2][2]));
            for (int cz = (int)((lz - gz0) / cell); cz <= (int)((hz - gz0) / cell); cz++) for (int cx = (int)((lx - gx0) / cell); cx <= (int)((hx - gx0) / cell); cx++) grid[(size_t)cz * gnx + cx].push_back((uint32_t)t);
        }
    }
    // altura del suelo en (x,z): triángulo más alto con y <= ymax (así el techo/cúpula no cuenta); NAN si no hay
    Ground gcol; std::vector<float> colLines;   // DH_COL=nivel.col: colisión real del juego (tools/collision.py); sustituye a la malla visual en groundY y se dibuja en verde
    std::string colPath = std::getenv("DH_COL") ? std::getenv("DH_COL") : (mdl ? std::string(argv[1]).substr(0, std::string(argv[1]).size() - 4) + ".col" : std::string());   // por defecto: <modelo>.col junto al .mdl
    if (!colPath.empty()) { const char* cp = colPath.c_str(); std::vector<uint8_t> probe; if (!std::getenv("DH_COL") && !readFile(cp, probe)) cp = nullptr;
      if (cp) { std::vector<uint8_t> craw; if (readFile(cp, craw) && gcol.load(craw)) { for (const auto& t : gcol.tris()) for (int e = 0; e < 3; e++) { colLines.insert(colLines.end(), t.v + 3*e, t.v + 3*e + 3); colLines.insert(colLines.end(), t.v + 3*((e+1)%3), t.v + 3*((e+1)%3) + 3); } std::printf("colisión: %zu triángulos (%s)\n", gcol.tris().size(), cp); } else std::fprintf(stderr, "aviso: colisión %s no válida\n", cp); } }
    auto groundY = [&](float x, float z, float ymax) {
        if (!colLines.empty()) { auto g = gcol.groundQuery(x, ymax, z, 0.f, ymax - gcol.boundsMin().y + 1.f); return g.hit ? g.height : (float)NAN; }   // alcance = hasta el fondo de la malla (sin constantes)
        float best = NAN; int cx = (int)((x - gx0) / cell), cz = (int)((z - gz0) / cell);
        if (cx < 0 || cz < 0 || cx >= gnx || cz >= gnz) return best;
        for (uint32_t t : grid[(size_t)cz * gnx + cx]) {
            const float *a = &v[M.mi[t]*3], *b = &v[M.mi[t+1]*3], *c = &v[M.mi[t+2]*3];
            float d = (b[2]-c[2])*(a[0]-c[0]) + (c[0]-b[0])*(a[2]-c[2]); if (std::fabs(d) < 1e-9f) continue;
            float l1 = ((b[2]-c[2])*(x-c[0]) + (c[0]-b[0])*(z-c[2])) / d, l2 = ((c[2]-a[2])*(x-c[0]) + (a[0]-c[0])*(z-c[2])) / d, l3 = 1 - l1 - l2;
            if (l1 < 0 || l2 < 0 || l3 < 0) continue;
            float y = l1*a[1] + l2*b[1] + l3*c[1];
            if (y <= ymax && !(y <= best)) best = y;
        }
        return best;
    };
    const float U = std::getenv("DH_UNIT") ? (float)std::atof(std::getenv("DH_UNIT")) : 3.2808f;   // unidades por metro: 1 u = 1 pie (savestate, docs/p2s-savestates.md)
    const float eye = 1.7f * U, grav = 9.8f * U; float vy = 0; bool walk = std::getenv("DH_WALK") != nullptr;
    if (walk) { float g = groundY(px, pz, py); if (!std::isnan(g)) py = g + eye; std::printf("walk: suelo en (%.0f, %.0f) = %.1f\n", px, pz, g); }
    std::vector<float> overlay;   // DH_PTS=archivo: floats x y z sueltos, se dibujan encima del modelo (rutas, puntos de control)
    if (const char* op = std::getenv("DH_PTS")) { std::vector<uint8_t> r; if (readFile(op, r)) { overlay.resize(r.size() / 4); std::memcpy(overlay.data(), r.data(), overlay.size() * 4); if (!rawAxes) for (size_t i = 0; i + 2 < overlay.size(); i += 3) { float y = overlay[i+1], z = overlay[i+2]; overlay[i+1] = z; overlay[i+2] = -y; } } }
    // --- Modo bici (R): punto material que baja por el terreno. Unidades del juego sin confirmar: la separación de carriles de la línea PTS (~22 u) sugiere 10 u = 1 m (DH_UNIT cambia el valor) ---
    const float G = 9.8f * U;
    bool ride = false; float rx0 = 0, ry0 = 0, rz0 = 0, rh = 0, rs = 0, rvy = 0, lastSlope = 0, goodX = 0, goodY = 0, goodZ = 0, goodH = 0; bool air = false;
    // --- Modo bici con la colisión del juego (DH_COL o <modelo>.col): esfera cinemática (src/ride.hpp) que sigue la línea .PTS (DH_PTS) con Ground::sweep y cuenta las puertas (<modelo>.gates / DH_GATES) ---
    const bool useCol = !colLines.empty();
    RideBody rb; RideParams rp; Gates gts; Gates::State gst; double rideT = 0;
    { std::string gp = std::getenv("DH_GATES") ? std::getenv("DH_GATES") : (mdl ? std::string(argv[1]).substr(0, std::string(argv[1]).size() - 4) + ".gates" : std::string()); std::vector<uint8_t> graw; if (!gp.empty() && readFile(gp, graw) && !gts.load(graw)) std::fprintf(stderr, "aviso: %s no válido\n", gp.c_str()); }
    float worldMinY = 1e30f; for (size_t i = 1; i < v.size(); i += 3) worldMinY = std::fmin(worldMinY, v[i]);
    auto startRide = [&]() {
        if (useCol && overlay.size() >= 6) {
            size_t k = std::getenv("DH_RIDE_AT") ? (size_t)std::atoi(std::getenv("DH_RIDE_AT")) : 3;   // 3: pasada la verja de salida cerrada (ride_demo.cpp)
            k = std::min(k, overlay.size() / 3 - 2); auto gi = gcol.groundQuery(overlay[3*k], overlay[3*k+1] + 10.f, overlay[3*k+2], rp.radius, 66.f);
            rb = RideBody(); rb.pos = {overlay[3*k], (gi.hit ? gi.height : overlay[3*k+1]) + rp.radius + 3.f, overlay[3*k+2]}; gst = Gates::State(); rideT = 0;
            rx0 = rb.pos.x; ry0 = rb.pos.y - rp.radius; rz0 = rb.pos.z; rh = std::atan2(overlay[3*k+3] - rx0, -(overlay[3*k+5] - rz0)); walk = false; return;
        }
        if (overlay.size() >= 6) {                       // primer punto de la línea PTS con suelo debajo, mirando al siguiente (la plataforma de salida puede no estar en la malla)
            size_t n = overlay.size() / 3, k = std::getenv("DH_RIDE_AT") ? (size_t)std::atoi(std::getenv("DH_RIDE_AT")) : 0;   // DH_RIDE_AT: índice del punto de la línea donde empezar
            while (k + 1 < n && std::isnan(groundY(overlay[3*k], overlay[3*k+2], overlay[3*k+1] + 0.3f * U))) k++;   // la línea queda a ±12 u del suelo: 0.3 m basta para elegir la lámina correcta
            if (k + 1 >= n) k = 0;
            rx0 = overlay[3*k]; ry0 = overlay[3*k+1] + 0.3f * U; rz0 = overlay[3*k+2];
            rh = std::atan2(overlay[3*k+3] - rx0, -(overlay[3*k+5] - rz0));
        }
        else { rx0 = px; ry0 = py; rz0 = pz; rh = yaw; }
        float g = groundY(rx0, rz0, ry0); if (!std::isnan(g)) ry0 = g; rs = 2.f * U; rvy = 0; air = false; walk = false; goodX = rx0; goodY = ry0; goodZ = rz0; goodH = rh;   // empujón inicial de 2 m/s
    };
    if (std::getenv("DH_RIDE")) { ride = true; startRide(); }
    int frame = 0;
    Uint64 last = SDL_GetTicks();
    for (bool run = true; run;) {
        for (SDL_Event e; SDL_PollEvent(&e);) {
            if (e.type == SDL_EVENT_QUIT || (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE)) run = false;
            if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_R && mdl) { ride = !ride; if (ride) startRide(); }   // R: modo bici
            if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_F && mdl) { walk = !walk; vy = 0; ride = false; }   // F: volar <-> caminar con gravedad
            if (e.type == SDL_EVENT_MOUSE_MOTION) { yaw += e.motion.xrel * 0.003f; pitch -= e.motion.yrel * 0.003f; }
        }
        pitch = std::fmax(-1.55f, std::fmin(1.55f, pitch));
        Uint64 now = SDL_GetTicks(); float dt = (now - last) / 1000.f; last = now;
        static const float fixedDt = std::getenv("DH_DT") ? (float)std::atof(std::getenv("DH_DT")) : 0.f; if (fixedDt > 0) dt = fixedDt;   // pruebas deterministas
        const bool* k = SDL_GetKeyboardState(nullptr);
        float sp = (mdl ? (k[SDL_SCANCODE_LSHIFT] ? 4.f : 1.f) * scale : (k[SDL_SCANCODE_LSHIFT] ? 4000.f : 800.f)) * dt;
        float fx = std::sin(yaw) * std::cos(pitch), fy = std::sin(pitch), fz = -std::cos(yaw) * std::cos(pitch);
        float rx = std::cos(yaw), rz = std::sin(yaw);
        if (ride && useCol && overlay.size() >= 6) {
            float ddt = std::fmin(dt, 0.05f); size_t n = overlay.size() / 3, best = 0; float bd = 1e30f; static size_t lastIdx = 0; if (rideT == 0) lastIdx = 0;
            for (size_t q = lastIdx > 30 ? lastIdx - 30 : 0; q < std::min(n, lastIdx + 60); q++) { float ex = overlay[3*q] - rb.pos.x, ez = overlay[3*q+2] - rb.pos.z, dd = ex*ex + ez*ez; if (dd < bd) { bd = dd; best = q; } }
            lastIdx = best; size_t tq = std::min(n - 1, best + 5);
            rb.step(gcol, rp, {overlay[3*tq], overlay[3*tq+1], overlay[3*tq+2]}, ddt); rideT += ddt;
            int ev = gts.update(gst, rb.pos, (float)rideT); if (ev > 0) std::printf("puerta %zu/%zu cruzada a los %.1f s\n", gst.counter, gts.size(), rideT);
            if (gst.finished && gst.finishTime == (float)rideT) std::printf("META a los %.1f s (%zu/%zu puertas)\n", rideT, gst.counter, gts.size());
            rx0 = rb.pos.x; ry0 = rb.pos.y - rp.radius; rz0 = rb.pos.z; float sh = std::sqrt(rb.vel.x*rb.vel.x + rb.vel.z*rb.vel.z); if (sh > 5.f) rh = std::atan2(rb.vel.x, -rb.vel.z);
            if (frame % 10 == 0) { char t[160]; std::snprintf(t, sizeof t, "dhview  puertas %zu/%zu%s  %.0f km/h  t=%.0f s  %s", gst.counter, gts.size(), gst.finished ? " META" : "", 1.0973f * std::sqrt(dot(rb.vel, rb.vel)), rideT, rb.grounded ? "suelo" : "aire"); SDL_SetWindowTitle(w, t); }
            px = rx0 - std::sin(rh) * 6.f * U; pz = rz0 + std::cos(rh) * 6.f * U; py = ry0 + 3.f * U; yaw = rh; pitch = -0.22f;
        } else if (ride) {
            float ddt = std::fmin(dt, 0.05f), dx = std::sin(rh), dz = -std::cos(rh), step = 0.8f * U;   // escalón máximo 0.8 m
            float turn = (k[SDL_SCANCODE_D] ? 1.f : 0.f) - (k[SDL_SCANCODE_A] ? 1.f : 0.f);
            if (std::getenv("DH_AUTOSTEER") && overlay.size() >= 6) {     // piloto automático: apunta a un punto de la línea unos metros por delante del más cercano
                size_t n = overlay.size() / 3, best = 0; float bd = 1e30f;
                for (size_t q = 0; q < n; q++) { float ex = overlay[3*q] - rx0, ez = overlay[3*q+2] - rz0, dd = ex*ex + ez*ez; if (dd < bd) { bd = dd; best = q; } }
                size_t tq = std::min(n - 1, best + 4); float want = std::atan2(overlay[3*tq] - rx0, -(overlay[3*tq+2] - rz0)), df = want - rh;
                while (df > 3.14159f) df -= 6.28318f; while (df < -3.14159f) df += 6.28318f;
                turn = std::fmax(-1.f, std::fmin(1.f, df * 2.f));
            }
            rh += turn * 1.6f * ddt / (1.f + 0.04f * rs / U);                       // gira menos a más velocidad
            dx = std::sin(rh); dz = -std::cos(rh);
            if (std::getenv("DH_DEBUG") && frame % 10 == 0) std::printf("ride dbg f=%d pos=(%.0f %.0f %.0f) rs=%.1f h=%.2f air=%d rvy=%.0f\n", frame, rx0, ry0, rz0, rs, rh, (int)air, rvy);
            if (!air) {
                float e = 0.5f * U, g0 = groundY(rx0, rz0, ry0 + step), g1 = groundY(rx0 + dx * e, rz0 + dz * e, ry0 + step);
                float slope = (std::isnan(g0) || std::isnan(g1)) ? 0.f : std::fmax(-1.5f, std::fmin(1.5f, (g1 - g0) / e)); lastSlope = slope;
                rs += (-G * slope / std::sqrt(1.f + slope * slope) + ((k[SDL_SCANCODE_W] || std::getenv("DH_AUTOPEDAL")) ? 1.5f * U : 0.f)) * ddt;   // gravedad a lo largo de la pendiente (+ pedaleo)
                rs -= 0.04f * rs * ddt;                                            // rodadura
                if (k[SDL_SCANCODE_S]) rs -= std::copysign(std::fmin(6.f * U * ddt, std::fabs(rs)), rs);   // freno: nunca invierte el signo
                rs -= 0.0005f * rs * std::fabs(rs) / U * ddt * 60.f;                // resistencia del aire
                rs = std::fmax(-3.f * U, std::fmin(40.f * U, rs));
            }
            float nx = rx0 + dx * rs * ddt, nz = rz0 + dz * rs * ddt;
            float gy = groundY(nx, nz, ry0 + step);
            if (std::isnan(gy)) { rs *= 0.2f; }                                     // pared / fuera del mapa: no avanza
            else {
                rx0 = nx; rz0 = nz;
                if (k[SDL_SCANCODE_SPACE] && !air) { rvy = 4.f * U; air = true; }
                if (air) {
                    rvy -= G * ddt; ry0 += rvy * ddt;
                    if (ry0 <= gy) { ry0 = gy; air = false; rvy = 0; }
                } else if (gy < ry0 - 0.35f * U - 0.02f * std::fabs(rs)) { air = true; rvy = std::fmin(0.f, lastSlope) * rs; }   // el suelo cae por debajo: despega conservando la velocidad vertical de la pendiente
                else ry0 = gy;
            }
            if (!air && frame % 30 == 0) { goodX = rx0; goodY = ry0; goodZ = rz0; goodH = rh; }               // último punto bueno sobre suelo
            if (ry0 < worldMinY - 5.f * U || (air && ry0 < goodY - 60.f * U)) { rx0 = goodX; ry0 = goodY; rz0 = goodZ; rh = goodH; rs = 2.f * U; rvy = 0; air = false; }   // cayó fuera del mapa: vuelve al último punto bueno
            px = rx0 - std::sin(rh) * 3.5f * U; pz = rz0 + std::cos(rh) * 3.5f * U; py = ry0 + 1.8f * U;   // cámara 3.5 m detrás, 1.8 m arriba
            yaw = rh; pitch = -0.22f;
        } else if (walk) {                                   // modo caminar: movimiento horizontal, gravedad y salto; un escalón máximo de 0.6*eye
            float ws = (k[SDL_SCANCODE_LSHIFT] ? 4.f : 1.f) * scale * 0.01f * std::fmin(dt, 0.05f), hx = std::sin(yaw), hz = -std::cos(yaw);
            if (k[SDL_SCANCODE_W]) { px += hx*ws; pz += hz*ws; }
            if (k[SDL_SCANCODE_S]) { px -= hx*ws; pz -= hz*ws; }
            if (k[SDL_SCANCODE_D]) { px += rx*ws; pz += rz*ws; }
            if (k[SDL_SCANCODE_A]) { px -= rx*ws; pz -= rz*ws; }
            float ddt = std::fmin(dt, 0.05f), g = groundY(px, pz, py - eye + 0.6f * eye);
            if (k[SDL_SCANCODE_SPACE] && !std::isnan(g) && py - eye <= g + 1e-3f) vy = eye * 9.f;
            vy -= grav * ddt; py += vy * ddt;
            if (!std::isnan(g) && py - eye < g) { py = g + eye; vy = 0; }
        } else {
        if (k[SDL_SCANCODE_W]) { px += fx*sp; py += fy*sp; pz += fz*sp; }
        if (k[SDL_SCANCODE_S]) { px -= fx*sp; py -= fy*sp; pz -= fz*sp; }
        if (k[SDL_SCANCODE_D]) { px += rx*sp; pz += rz*sp; }
        if (k[SDL_SCANCODE_A]) { px -= rx*sp; pz -= rz*sp; }
        }
        int ww, hh; SDL_GetWindowSizeInPixels(w, &ww, &hh);
        glViewport(0, 0, ww, hh); { static float bg[3] = {0.05f, 0.06f, 0.09f}; static bool init = false; if (!init && !M.chunks.empty() && sky.mi.empty()) { bg[0] = 0.58f; bg[1] = 0.70f; bg[2] = 0.86f; } /* nivel sin cúpula de cielo: azul claro por defecto */ if (!init) { init = true; if (const char* b = std::getenv("DH_BG")) std::sscanf(b, "%f %f %f", &bg[0], &bg[1], &bg[2]); } glClearColor(bg[0], bg[1], bg[2], 1); }
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);   // DH_BG="r g b": color de fondo (p. ej. magenta para ver huecos) glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LEQUAL);   // LEQUAL: las capas superpuestas del juego comparten posición con el suelo
        glMatrixMode(GL_PROJECTION); glLoadIdentity();
        float asp = (float)ww / hh, nr = mdl ? ((ride || walk) ? std::fmax(0.3f, 0.03f * U) : std::fmax(0.05f, 0.002f * scale)) : 5, fr = 60000, t = nr * std::tan(0.5f * (std::getenv("DH_FOV") ? (float)std::atof(std::getenv("DH_FOV")) : 1.1f));   // DH_FOV: ángulo vertical en rad
        glFrustum(-t*asp, t*asp, -t, t, nr, fr);
        glMatrixMode(GL_MODELVIEW); glLoadIdentity();
        glRotatef(-pitch * 57.2958f, 1, 0, 0); glRotatef(yaw * 57.2958f, 0, 1, 0); glTranslatef(-px, -py, -pz);
        if (mdl) {
            glEnable(GL_TEXTURE_2D); glColor3f(1, 1, 1);
            { static const char* cm = std::getenv("DH_CULL"); if (cm && !std::strcmp(cm, "ccw")) { glEnable(GL_CULL_FACE); glFrontFace(GL_CCW); } else if (cm && !std::strcmp(cm, "cw")) { glEnable(GL_CULL_FACE); glFrontFace(GL_CW); } else glDisable(GL_CULL_FACE); }   // DH_CULL=ccw|cw: descarta caras traseras
            glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);   // alfa PS2 ya normalizado a 0-255 en el .mdl
            { static float gain = std::getenv("DH_GAIN") ? (float)std::atof(std::getenv("DH_GAIN")) : (M.chunks.empty() ? 2.f : 1.f);   // los modelos que no son de nivel se aclaran x2 (sobrebrillo PS2)
              glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE); glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
              glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE); glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_PRIMARY_COLOR); glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE, gain); }
            glEnableClientState(GL_VERTEX_ARRAY); glEnableClientState(GL_TEXTURE_COORD_ARRAY); glEnableClientState(GL_COLOR_ARRAY);
            static const bool noCull = std::getenv("DH_NOCULL") != nullptr; static size_t drawnChunks = 0;
            auto drawModel = [&](const Model& m) {
                glVertexPointer(3, GL_FLOAT, 40, m.mv.data()); glTexCoordPointer(2, GL_FLOAT, 40, m.mv.data() + 3);
                glColorPointer(4, GL_FLOAT, 40, m.mv.data() + 5);   // iluminación horneada por vértice
                auto drawRange = [&](size_t lo, size_t hi) {
                    for (size_t t = lo; t + 2 < hi + 0; ) {          // agrupa por textura consecutiva
                        int tid = (int)m.mv[m.mi[t] * 10 + 9]; size_t e = t;
                        while (e + 2 < hi && (int)m.mv[m.mi[e] * 10 + 9] == tid) e += 3;
                        glBindTexture(GL_TEXTURE_2D, tid >= 0 && tid < (int)m.texs.size() ? m.texs[tid].id : 0);
                        glDrawElements(GL_TRIANGLES, (GLsizei)(e - t), GL_UNSIGNED_INT, m.mi.data() + t); t = e;
                    }
                };
                if (m.chunks.empty()) { drawRange(0, m.mi.size()); return; }
                size_t shown = 0;
                for (const Chunk& c : m.chunks) {                  // cada selector: visible si la cámara está dentro de (sqrt(dist2_max) + radio)
                    bool vis = true;
                    for (auto& sl : c.sels) { float dx = px - sl[0], dy = py - sl[1], dz = pz - sl[2], lim = std::sqrt(sl[4]) + sl[3]; if (!noCull && dx*dx + dy*dy + dz*dz > lim*lim) { vis = false; break; } }
                    if (vis) { drawRange(c.first, c.first + c.count); shown++; }
                }
                drawnChunks = shown;
            };
            if (!sky.mi.empty() && !std::getenv("DH_NOSKY")) { glDepthMask(GL_FALSE); drawModel(sky); glDepthMask(GL_TRUE); }   // telón de fondo/cielo: primero y sin escribir profundidad
            drawModel(M);
            if (ride && !bike.mi.empty()) {                 // la bici ensamblada: 1 unidad del modelo ~ 0.28 m; rueda más baja a ras de suelo
                static float minY = [&] { float m = 1e30f; for (size_t i = 1; i < bike.mv.size(); i += 10) m = std::fmin(m, bike.mv[i]); return m; }();
                float bs = 0.28f * U;
                glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE, 2.6f);   // modelos que no son de nivel: aclarar x2.6 (sobrebrillo PS2)
                glPushMatrix(); glTranslatef(rx0, ry0 - minY * bs, rz0); glRotatef(-rh * 57.2958f, 0, 1, 0); glScalef(bs, bs, bs);
                drawModel(bike); glPopMatrix();
            }
            glDisableClientState(GL_TEXTURE_COORD_ARRAY); glDisableClientState(GL_COLOR_ARRAY); glDisable(GL_TEXTURE_2D); glDisable(GL_BLEND);
        } else {
            glEnableClientState(GL_VERTEX_ARRAY); glEnableClientState(GL_COLOR_ARRAY);
            glVertexPointer(3, GL_FLOAT, 0, v.data()); glColorPointer(3, GL_FLOAT, 0, col.data());
            glPointSize(2); glDrawArrays(tris ? GL_TRIANGLES : GL_POINTS, 0, (GLsizei)n);
        }
        if (ride && bike.mi.empty()) {                 // marcador del ciclista: pirámide roja orientada al rumbo (si no hay DH_BIKE)
            glPushMatrix(); glTranslatef(rx0, ry0, rz0); glRotatef(-rh * 57.2958f, 0, 1, 0);
            float b = 0.35f * U, hgt = 1.2f * U, l = 0.9f * U;
            glBegin(GL_TRIANGLES); glColor3f(0.9f, 0.1f, 0.1f);
            glVertex3f(-b, 0, l); glVertex3f(b, 0, l); glVertex3f(0, hgt, 0);
            glColor3f(0.7f, 0.05f, 0.05f); glVertex3f(-b, 0, l); glVertex3f(0, hgt, 0); glVertex3f(0, 0, -l);
            glVertex3f(b, 0, l); glVertex3f(0, 0, -l); glVertex3f(0, hgt, 0);
            glColor3f(0.4f, 0.f, 0.f); glVertex3f(-b, 0, l); glVertex3f(0, 0, -l); glVertex3f(b, 0, l);
            glEnd(); glPopMatrix();
        }
        if (!colLines.empty() && !std::getenv("DH_NOCOLDRAW")) { glEnableClientState(GL_VERTEX_ARRAY); glColor3f(0.1f, 1.f, 0.3f); glVertexPointer(3, GL_FLOAT, 0, colLines.data()); glDrawArrays(GL_LINES, 0, (GLsizei)(colLines.size() / 3)); }
        if (!overlay.empty()) {
            glDisable(GL_DEPTH_TEST); glEnableClientState(GL_VERTEX_ARRAY); glColor3f(1, 0.2f, 0.9f); glPointSize(5);
            glVertexPointer(3, GL_FLOAT, 0, overlay.data()); glDrawArrays(GL_POINTS, 0, (GLsizei)(overlay.size() / 3)); glEnable(GL_DEPTH_TEST);
        }
        static const int shotFrame = std::getenv("DH_FRAMES") ? std::atoi(std::getenv("DH_FRAMES")) : 3;
        if (shot && frame == shotFrame - 1 && walk) std::printf("walk: py=%.1f vy=%.2f\n", py, vy);
        if (shot && frame == shotFrame - 1 && ride) std::printf("ride: pos=(%.0f %.0f %.0f) vel=%.1f u/s (%.1f m/s) air=%d\n", rx0, ry0, rz0, rs, rs / U, (int)air);
        if (shot && ++frame == shotFrame) {
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
