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
#include <limits>
#include <string>
#include <vector>
#include <bits/basic_string.h>
#include "gates.hpp"
#include "ground.hpp"
#include "ride.hpp"
#include "race.hpp"
#include "controls.hpp"
#include "text.hpp"
#include "hud.hpp"
static std::vector<uint8_t> g_inLog; static bool g_inLogSaved = false;   // registro de entradas por tick (DH_INPUT_LOG): se guarda al llegar a meta o, si no, al cerrar el visor
#include "gl_renderer.hpp"
#include "rider.hpp"
#include "rider_mesh.hpp"

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
    Model M, sky, dome, bike;   // bike: DH_BIKE=bici_ensamblada.mdl (tools/assemble_bike.py), se dibuja en el modo bici
    {
        std::vector<uint8_t> raw;
        if (!readFile(argv[1], raw)) { std::fprintf(stderr, "no se puede leer %s\n", argv[1]); return 1; }
        if (mdl) {
            if (!parseMdl(raw, M)) { std::fprintf(stderr, "%s: .mdl inválido (¿DHM1 antiguo? vuelve a extraer)\n", argv[1]); return 1; }
            toYUp(M);
            for (size_t k = 0; k + 9 < M.mv.size(); k += 10) v.insert(v.end(), M.mv.begin() + k, M.mv.begin() + k + 3);
            if (const char* bp = std::getenv("DH_BIKE")) { std::vector<uint8_t> braw; if (!readFile(bp, braw) || !parseMdl(braw, bike)) { std::fprintf(stderr, "aviso: DH_BIKE=%s no válido\n", bp); bike = Model(); } }
            std::vector<uint8_t> sraw; std::string sp = std::string(argv[1]); sp = sp.substr(0, sp.size() - 4) + ".sky.mdl";
            { std::vector<uint8_t> draw; std::string dp = std::string(argv[1]); dp = dp.substr(0, dp.size() - 4) + ".dome.mdl";   // panorama del horizonte (cielo, montañas, nubes): se dibuja centrado en la cámara
              if (readFile(dp, draw) && !parseMdl(draw, dome)) { std::fprintf(stderr, "aviso: %s inválido, se ignora\n", dp.c_str()); dome = Model(); } else toYUp(dome); }
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
    for (Model* m : {&M, &sky, &dome, &bike}) for (auto& t : m->texs) {
        glGenTextures(1, &t.id); glBindTexture(GL_TEXTURE_2D, t.id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, t.w, t.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, t.px.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    }
    // --- Piloto (hito 4): DH_RIDER=modelo.mdl DH_RIDER_SKIN=modelo.skin DH_RIDER_NGP=nivel.NGP DH_RIDER_ROOT=0x9e2f40 [DH_RIDER_POSE=40 floats por línea/espacios] [DH_RIDER_AT="x y z"]. Sólo habla con gfx::Renderer (no GL).
    gfx::GLRenderer gr; gfx::MeshId riderMesh = -1; rider::Skeleton riderSk; std::vector<rider::SkinVertex> riderSkin; std::vector<float> riderBind, riderVerts, riderPose(40, 0.f); float riderAt[3] = {0.f, 2.0f, 0.f}; bool riderApprox = false, riderCadenceExplicit = false; float riderPhase = 0.f, riderCadence = 5.5f; rider::ApproxResult riderApproxResult; std::array<float, 3> riderPelvisZ{}, riderTorso{}, riderPelvisRot{}; const bool riderDebug = std::getenv("DH_RIDER_DEBUG") != nullptr;
    if (const char* rp = std::getenv("DH_RIDER")) {
        Model rm; std::vector<uint8_t> raw, sraw, nraw; const char* sk = std::getenv("DH_RIDER_SKIN"); const char* np = std::getenv("DH_RIDER_NGP"); const char* rt = std::getenv("DH_RIDER_ROOT");
        if (readFile(rp, raw) && parseMdl(raw, rm) && sk && readFile(sk, sraw) && rider::parseSkin(sraw, riderSkin) && np && readFile(np, nraw) && rt && riderSk.load(nraw, std::strtoul(rt, nullptr, 0)) && riderSkin.size() * 10 == rm.mv.size()) {
            std::vector<gfx::TextureId> tids; for (auto& t : rm.texs) tids.push_back(gr.createTexture({t.w, t.h, t.px}));
            riderVerts = rm.mv; riderBind.resize(riderSkin.size() * 3); for (size_t i = 0; i < riderSkin.size(); i++) for (int c = 0; c < 3; c++) riderBind[3*i+c] = rm.mv[10*i+c];
            riderMesh = gr.createMesh(riderVerts, rm.mi, tids, true);
        } else std::fprintf(stderr, "aviso: DH_RIDER/_SKIN/_NGP/_ROOT no válidos (mdl %d skin %d ngp %d esqueleto %d vértices %zu/%zu), se ignora el piloto\n", !raw.empty(), !riderSkin.empty(), !nraw.empty(), (int)riderSk.j.size(), riderSkin.size(), rm.mv.size() / 10);
    } else if (const char* np = std::getenv("DH_RIDER_NGP")) {      // carga nativa: sólo el NGP del nivel (sin Python); DH_RIDER_KIND=4090 elige el modelo, DH_RIDER_CHAIN fuerza el inicio de la cadena
        const char* co = std::getenv("DH_RIDER_CHAIN");
        std::vector<uint8_t> nraw; rider_mesh::Mesh native; const size_t none = std::numeric_limits<size_t>::max();
        size_t off = co ? std::strtoull(co, nullptr, 0) : none, end = none, skRoot = none;
        if (readFile(np, nraw)) {
            if (!co) { uint32_t kind = 0; if (const char* rk = std::getenv("DH_RIDER_KIND")) kind = static_cast<uint32_t>(std::strtoul(rk, nullptr, 0)); rider_mesh::findChain(nraw, off, kind, &end, &skRoot); }
            if (const char* rt = std::getenv("DH_RIDER_ROOT")) skRoot = std::strtoul(rt, nullptr, 0);
            const bool ok = off != none && (end != none ? rider_mesh::loadRange(nraw, off, end, native) : rider_mesh::load(nraw, off, native));
            if (ok) {
                riderVerts = native.vertices; riderMesh = gr.createMesh(riderVerts, native.indices, {}, true);   // sin texturas: la decodificación de texturas es del hito 5 (docs/formats/rider.md)
                if (skRoot != none && riderSk.load(nraw, skRoot) && native.boneIds.size() == 3 * (riderVerts.size() / 10)) {   // piel y esqueleto nativos
                    const size_t nv = riderVerts.size() / 10; riderSkin.resize(nv); riderBind.resize(3 * nv);
                    for (size_t i = 0; i < nv; i++) { for (int k = 0; k < 3; k++) { riderSkin[i].bone[k] = native.boneIds[3*i+k]; riderSkin[i].w[k] = native.weights[3*i+k]; riderBind[3*i+k] = riderVerts[10*i+k]; } }
                }
                std::fprintf(stderr, "rider_mesh: cadena 0x%zx..0x%zx, %u posiciones, %zu vértices, %u paquetes, esqueleto %s (sin texturas)\n", off, end, native.positionCount, riderVerts.size() / 10, native.packetCount, riderSk.j.empty() ? "no" : "sí");
            } else std::fprintf(stderr, "aviso: DH_RIDER_NGP/KIND/CHAIN no válidos; se ignora el piloto nativo\n");
        } else std::fprintf(stderr, "aviso: no se pudo leer DH_RIDER_NGP\n");
    }
    if (riderMesh >= 0) {   // pose y colocación comunes a la ruta con .mdl y a la nativa
        if (const char* pp = std::getenv("DH_RIDER_POSE")) {
            riderApprox = !std::strcmp(pp, "approx");
            if (riderApprox) {
                if (const char* cad = std::getenv("DH_RIDER_CADENCE")) { riderCadence = std::fmax(0.f, (float)std::atof(cad)); riderCadenceExplicit = true; }
                std::fprintf(stderr, "rider pose: approx (HIPÓTESIS; cadencia %.3f rad/s, solver no es la pose del juego)\n", riderCadence);
                // La pelvis y el torso se optimizan UNA vez (fase 0) y se fijan; por fotograma sólo se resuelven las extremidades (antes: ~0.1 s/fotograma y la pelvis saltaba).
                // DH_RIDER_AT está en el marco de la bici ensamblada (Y arriba, adelante -Z); el solver trabaja en el marco del modelo (X der., Y adelante, Z arriba).
                const auto first = rider::approxPose(riderSk, 0.f, {riderAt[0], -riderAt[2], riderAt[1]}, riderCadence, true);
                riderPelvisZ = first.pelvis; riderPelvisRot = first.pelvisRotation; riderTorso = {first.pose[6], first.pose[7], first.pose[8]};
                std::fprintf(stderr, "rider approx: pelvis=(%.2f %.2f %.2f) errores máx. de la fase 0: %.3f %.3f %.3f %.3f u\n", riderPelvisZ[0], riderPelvisZ[1], riderPelvisZ[2], first.errors[0], first.errors[1], first.errors[2], first.errors[3]);
            } else {
                std::vector<uint8_t> pr; if (readFile(pp, pr)) { std::string txt(pr.begin(), pr.end()); const char* c = txt.c_str(); for (size_t i = 0; i < riderPose.size(); i++) { char* e; float v = std::strtof(c, &e); if (e == c) break; riderPose[i] = v; c = e; } }
            }
        }
        if (const char* at = std::getenv("DH_RIDER_AT")) std::sscanf(at, "%f %f %f", &riderAt[0], &riderAt[1], &riderAt[2]);
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
    // --- Modo jugable (P, o DH_PLAY=1): la moto de src/bike.hpp con teclado. FÍSICA APROXIMADA (docs/formats/bike-physics.md). W acelerar, S frenar, A/D girar, Q/E inclinar (morro arriba/abajo),
    // Espacio saltar, Enter reiniciar en el último punto bueno, T volver a la rejilla de salida. DH_PLAYIN="acelerador freno giro inclinar" fija los mandos sin teclado (capturas DH_SHOT) ---
    bool play = false, hopPressed = false, finishedMsg = false; Race race; Run& prun = race.run; V3 camPos{}, startPt{}; float camHead = 0, leanVis = 0; bool haveStart = false, gateOpened = false;
    { std::vector<uint8_t> sraw; std::string sp0 = mdl ? std::string(argv[1]).substr(0, std::string(argv[1]).size() - 4) + ".start.pts" : std::string();   // primera plaza de la rejilla (tools/markers.py), espacio NGP -> Y arriba
      if (!sp0.empty() && readFile(sp0, sraw) && sraw.size() >= 12) { float f[3]; std::memcpy(f, sraw.data(), 12); startPt = {f[0], f[2], -f[1]}; haveStart = true; } }
    // DH_REPLAY=traza6.bin (tests/bike_demo.cpp, DH_TRACE6): reproduce la trayectoria del piloto automático de pruebas (6 f32 por muestra: posición y dirección, Y arriba) en el modo jugable, sin simular; DH_REPLAY_AT=i fija la muestra (capturas). Sólo para evidencia visual.
    std::vector<float> replay; size_t replayAt = std::getenv("DH_REPLAY_AT") ? (size_t)std::atoi(std::getenv("DH_REPLAY_AT")) : 0;
    if (const char* rp0 = std::getenv("DH_REPLAY")) { std::vector<uint8_t> rr; if (readFile(rp0, rr)) { replay.resize(rr.size() / 4); std::memcpy(replay.data(), rr.data(), replay.size() * 4); } }
    auto startPlay = [&]() {
        if (!useCol) { std::fprintf(stderr, "modo jugable: hace falta la colisión (<modelo>.col o DH_COL)\n"); return; }
        if (!gateOpened) { openStartGate(gcol); gateOpened = true; }   // la verja de salida (superficie 0x681D) está cerrada en la malla estática
        const Gate* g0 = gts.courseGate(0); float h0 = g0 ? std::atan2(g0->n.x, -g0->n.z) : yaw;
        race.load(gcol, &gts, haveStart ? startPt : V3{px, py, pz}, h0);   // cuenta atrás de 3 s con la moto en la parrilla y reloj a cero al «¡Ya!» (src/race.hpp) camHead = h0; leanVis = 0; finishedMsg = false;
        if (std::getenv("DH_DEMO_RESULTS")) { race.res.finished = true; race.res.time = 430.98; race.res.gates = race.res.totalGates = gts.size(); race.res.respawns = 0; race.res.maxSpeed = 111.f; race.res.splits = {16.00f, 26.26f, 39.66f}; race.state = RaceState::Results; }   // solo para capturas de la maquetación (valores de muestra)
        V3 f = prun.bike.fwd; camPos = prun.bike.pos + V3{-f.x * 11.f, 4.5f, -f.z * 11.f}; play = true; ride = false; walk = false;
    };
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
    if (std::getenv("DH_PLAY")) startPlay();
    int frame = 0;
    Uint64 last = SDL_GetTicks();
    for (bool run = true; run;) {
        for (SDL_Event e; SDL_PollEvent(&e);) {
            if (e.type == SDL_EVENT_QUIT || (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE)) run = false;
            if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_R && mdl) { static bool fromPlay = false; if (!ride) { fromPlay = play; ride = true; play = false; startRide(); } else { ride = false; if (fromPlay) startPlay(); } }   // R: bici automática y de vuelta al modo jugable (antes la segunda R dejaba la cámara libre sin bici)   // R: modo bici (demo cinemática)
            if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_P && mdl) { play = !play; if (play) startPlay(); }   // P: modo jugable
            if (play && e.type == SDL_EVENT_KEY_DOWN) { if (e.key.key == SDLK_SPACE) hopPressed = true; if (e.key.key == SDLK_T) { startPlay(); std::fprintf(stderr, "T: salida (%.0f %.0f %.0f)\n", prun.bike.pos.x, prun.bike.pos.y, prun.bike.pos.z); } if (e.key.key == SDLK_RETURN) { prun.respawn(); std::fprintf(stderr, "Enter: reaparece en (%.0f %.0f %.0f), reapariciones %d\n", prun.bike.pos.x, prun.bike.pos.y, prun.bike.pos.z, (int)prun.respawns); } }
            if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_F && mdl) { walk = !walk; vy = 0; ride = false; play = false; }   // F: volar <-> caminar con gravedad
            if (e.type == SDL_EVENT_MOUSE_MOTION) { yaw += e.motion.xrel * 0.003f; pitch -= e.motion.yrel * 0.003f; }
        }
        pitch = std::fmax(-1.55f, std::fmin(1.55f, pitch));
        Uint64 now = SDL_GetTicks(); float dt = (now - last) / 1000.f; last = now;
        static const float fixedDt = std::getenv("DH_DT") ? (float)std::atof(std::getenv("DH_DT")) : 0.f; if (fixedDt > 0) dt = fixedDt;   // pruebas deterministas
        const bool* k = SDL_GetKeyboardState(nullptr);
        float sp = (mdl ? (k[SDL_SCANCODE_LSHIFT] ? 4.f : 1.f) * scale : (k[SDL_SCANCODE_LSHIFT] ? 4000.f : 800.f)) * dt;
        float fx = std::sin(yaw) * std::cos(pitch), fy = std::sin(pitch), fz = -std::cos(yaw) * std::cos(pitch);
        float rx = std::cos(yaw), rz = std::sin(yaw);
        if (play) {
            float ddt = std::fmin(dt, 0.05f); int ev = 0;
            // Paso FIJO de 1/60 s (acumulador): la simulación no depende de los fps, así que las entradas por tick se pueden registrar (DH_INPUT_LOG=archivo, se guarda al salir)
            // y repetir (DH_INPUT_REPLAY=archivo) con el mismo resultado. DH_PLAYIN="a b c d" fija las entradas analógicas (pruebas).
            static std::vector<uint8_t> inReplay; std::vector<uint8_t>& inLog = g_inLog; static size_t inTick = 0; static bool inInit = false; static double acc = 0;
            if (!inInit) { inInit = true; if (const char* rp = std::getenv("DH_INPUT_REPLAY")) { if (!loadInputs(rp, inReplay)) std::fprintf(stderr, "aviso: no se pudo leer %s\n", rp); } }
            Keys ky; ky.up = k[SDL_SCANCODE_UP]; ky.down = k[SDL_SCANCODE_DOWN]; ky.left = k[SDL_SCANCODE_LEFT]; ky.right = k[SDL_SCANCODE_RIGHT]; ky.w = k[SDL_SCANCODE_W]; ky.s = k[SDL_SCANCODE_S]; ky.a = k[SDL_SCANCODE_A]; ky.d = k[SDL_SCANCODE_D];
            ky.q = k[SDL_SCANCODE_Q]; ky.e = k[SDL_SCANCODE_E]; ky.space = k[SDL_SCANCODE_SPACE]; ky.shift = k[SDL_SCANCODE_LSHIFT] || k[SDL_SCANCODE_RSHIFT];
            bool hopEdge = hopPressed; hopPressed = false; acc += ddt;
            for (int n = 0; acc >= 1.0 / 60.0 && n < 6; n++, acc -= 1.0 / 60.0) {
                BikeInput in; uint8_t bits = 0;
                if (!inReplay.empty()) bits = inTick < inReplay.size() ? inReplay[inTick] : (uint8_t)0;      // repetición: se acabó el registro = sin mandos
                else bits = packKeys(ky, hopEdge);
                hopEdge = false; in = unpackInput(bits);
                if (const char* pi = std::getenv("DH_PLAYIN")) std::sscanf(pi, "%f %f %f %f", &in.throttle, &in.brake, &in.steer, &in.lean);
                if (std::getenv("DH_INPUT_LOG")) inLog.push_back(bits);
                inTick++;
                if (replay.size() >= 12) {
                    size_t ns = replay.size() / 6, i = std::min(replayAt, ns - 1), j = std::min(i + 1, ns - 1); if (j == i && i > 0) j = i - 1; Bike& b0 = prun.bike;
                    b0.pos = {replay[6*i], replay[6*i+1], replay[6*i+2]}; b0.fwd = {replay[6*i+3], replay[6*i+4], replay[6*i+5]}; b0.grounded = true;
                    V3 dv{replay[6*j] - replay[6*i], replay[6*j+1] - replay[6*i+1], replay[6*j+2] - replay[6*i+2]}; if (j < i) dv = dv * -1.f; b0.rb.vel = dv * 10.f;
                    camHead = std::atan2(dv.x, -dv.z); camPos = b0.pos + V3{-std::sin(camHead) * 11.f, 4.5f, std::cos(camHead) * 11.f};
                } else { int e2 = race.update(gcol, in, 1.f / 60.f); if (e2 && !ev) ev = e2; }
            }
            { static const char* lp = std::getenv("DH_INPUT_LOG"); if (lp && !g_inLogSaved && race.state >= RaceState::Finished) { g_inLogSaved = saveInputs(lp, inLog); std::fprintf(stderr, "registro de entradas (meta): %zu ticks -> %s (%s)\n", inLog.size(), lp, g_inLogSaved ? "ok" : "error"); } }
            if (ev > 0) std::printf("puerta %zu/%zu cruzada a los %.1f s\n", prun.gs.counter, gts.size(), prun.t);
            if (race.state >= RaceState::Finished && !finishedMsg) { finishedMsg = true; std::printf("META %s (%zu/%zu puertas, reinicios %u, máx %.0f km/h)\n", formatTime(race.res.time).c_str(), race.res.gates, race.res.totalGates, race.res.respawns, 1.0973f * race.res.maxSpeed); }
            Bike& bk = prun.bike; Axes ax = bk.axes(); V3 vv = bk.vel(); float hs = std::sqrt(vv.x * vv.x + vv.z * vv.z);
            static float lastSteer = 0; { uint8_t lb = packKeys(ky, false); lastSteer = unpackInput(lb).steer; } leanVis += (-lastSteer * std::fmin(0.45f, bk.speed() * 0.012f) * (bk.grounded ? 1.f : 0.f) - leanVis) * std::fmin(1.f, 6.f * ddt);   // inclinación visual al girar (sólo dibujo)
            float want = hs > 4.f ? std::atan2(vv.x, -vv.z) : bk.heading(), df = want - camHead; while (df > 3.14159f) df -= 6.28318f; while (df < -3.14159f) df += 6.28318f;
            camHead += df * std::fmin(1.f, 3.f * ddt);
            V3 tgt = bk.pos + ax.u * 0.5f, cp = tgt + V3{-std::sin(camHead) * 11.f, 4.5f, std::cos(camHead) * 11.f}; camPos = camPos + (cp - camPos) * std::fmin(1.f, 8.f * ddt);
            { auto gq = gcol.groundQuery(camPos.x, camPos.y + 60.f, camPos.z, 0.f, 400.f); if (gq.hit && camPos.y < gq.height + 3.f) camPos.y = gq.height + 3.f; }   // la cámara no baja del suelo
            V3 d = tgt - camPos; px = camPos.x; py = camPos.y; pz = camPos.z; yaw = std::atan2(d.x, -d.z); pitch = std::atan2(d.y, std::sqrt(d.x * d.x + d.z * d.z));
            rx0 = bk.pos.x; ry0 = bk.pos.y - 2.05f; rz0 = bk.pos.z; rh = bk.heading();
            if (frame % 10 == 0) { char t[200]; std::snprintf(t, sizeof t, "dhview JUGAR  [%s%s]  %s  %.0f km/h  puertas %zu/%zu  %s  reinicios %u", raceStateName(race.state), race.state == RaceState::Countdown ? (" " + std::to_string(race.countdownDigit())).c_str() : "", formatTime(race.displayTime()).c_str(), 1.0973f * bk.speed(), prun.gs.counter, gts.size(), bk.grounded ? "suelo" : "aire", prun.respawns); SDL_SetWindowTitle(w, t); }
        } else if (ride && useCol && overlay.size() >= 6) {
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
        if (riderApprox && riderSk.j.size() >= 15) {
            if (play && !riderCadenceExplicit) riderCadence = std::fmax(0.f, prun.bike.speed() * 0.12f); // HIPÓTESIS: relación velocidad-cadencia; sustituir por transmisión medida.
            riderPhase += std::fmin(dt, 0.05f);
            riderApproxResult = rider::approxPose(riderSk, riderPhase, riderPelvisZ, riderCadence, false, riderPelvisRot, {}, riderTorso);
            std::copy(riderApproxResult.pose.begin(), riderApproxResult.pose.end(), riderPose.begin());
            if (riderDebug && frame % 30 == 0) {
                std::printf("rider frame=%d wristR=%.4f wristL=%.4f ankleR=%.4f ankleL=%.4f\n", frame, riderApproxResult.errors[0], riderApproxResult.errors[1], riderApproxResult.errors[2], riderApproxResult.errors[3]);
                for (float error : riderApproxResult.errors) if (error > 1e-2f) std::fprintf(stderr, "aviso: rider IK error %.4f u supera 1e-2 u\n", error);
            }
        }
        int ww, hh; SDL_GetWindowSizeInPixels(w, &ww, &hh);
        glViewport(0, 0, ww, hh); { static float bg[3] = {0.05f, 0.06f, 0.09f}; static bool init = false; if (!init && !M.chunks.empty() && sky.mi.empty() && dome.mi.empty()) { bg[0] = 0.58f; bg[1] = 0.70f; bg[2] = 0.86f; } /* nivel sin cúpula de cielo: azul claro por defecto */ if (!init) { init = true; if (const char* b = std::getenv("DH_BG")) std::sscanf(b, "%f %f %f", &bg[0], &bg[1], &bg[2]); } glClearColor(bg[0], bg[1], bg[2], 1); }
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);   // DH_BG="r g b": color de fondo (p. ej. magenta para ver huecos) glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LEQUAL);   // LEQUAL: las capas superpuestas del juego comparten posición con el suelo
        glMatrixMode(GL_PROJECTION); glLoadIdentity();
        float asp = (float)ww / hh, nr = mdl ? ((ride || play || walk || std::getenv("DH_CAM")) ? std::fmax(0.3f, 0.03f * U) : std::fmax(0.05f, 0.002f * scale)) : 5, fr = 60000, t = nr * std::tan(0.5f * (std::getenv("DH_FOV") ? (float)std::atof(std::getenv("DH_FOV")) : 1.1f));   // DH_FOV: ángulo vertical en rad
        glFrustum(-t*asp, t*asp, -t, t, nr, fr);
        glMatrixMode(GL_MODELVIEW); glLoadIdentity();
        { static float fg[5]; static const bool fog = std::getenv("DH_FOG") && std::sscanf(std::getenv("DH_FOG"), "%f %f %f %f %f", fg, fg+1, fg+2, fg+3, fg+4) == 5;   // DH_FOG="r g b inicio fin" (unidades del mundo): niebla lineal, HIPÓTESIS sin evidencia del motor (FOGCOL del GS no localizado); el panorama (cielo) se dibuja sin niebla
          if (fog) { glEnable(GL_FOG); glFogi(GL_FOG_MODE, GL_LINEAR); glFogfv(GL_FOG_COLOR, fg); glFogf(GL_FOG_START, fg[3]); glFogf(GL_FOG_END, fg[4]); } else glDisable(GL_FOG); }
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
            if (!dome.mi.empty() && !std::getenv("DH_NOSKY")) {   // sigue a la cámara; DH_DOMEDY desplaza el centro en vertical (afinado)
                static const float dy = std::getenv("DH_DOMEDY") ? (float)std::atof(std::getenv("DH_DOMEDY")) : 0.f;
                glDisable(GL_DEPTH_TEST); glDisable(GL_FOG); glPushMatrix(); glTranslatef(px, py + dy, pz); drawModel(dome); glPopMatrix(); glEnable(GL_DEPTH_TEST); if (std::getenv("DH_FOG")) glEnable(GL_FOG);
            }
            if (!sky.mi.empty() && !std::getenv("DH_NOSKY")) { glDepthMask(GL_FALSE); drawModel(sky); glDepthMask(GL_TRUE); }   // telón de fondo/cielo: primero y sin escribir profundidad
            drawModel(M);
            if (play && !bike.mi.empty()) {                 // la bici ensamblada orientada con el cuerpo de la simulación (modelo: adelante = -Z, arriba = +Y)
                static float minY = [&] { float m = 1e30f; for (size_t i = 1; i < bike.mv.size(); i += 10) m = std::fmin(m, bike.mv[i]); return m; }();
                const Bike& bk = prun.bike; Axes ax = bk.axes(); V3 r = rotateAbout(ax.r, ax.f, leanVis), u = rotateAbout(ax.u, ax.f, leanVis); float bs = 0.28f * U;
                V3 t = bk.pos + ax.u * (-2.05f - minY * bs);   // la rueda más baja a ras de la superficie de contacto (rueda -1.2-0.75 / -1.4-0.75 bajo el centro: promedio, hipótesis)
                GLfloat m[16] = {r.x, r.y, r.z, 0, u.x, u.y, u.z, 0, -ax.f.x, -ax.f.y, -ax.f.z, 0, t.x, t.y, t.z, 1};
                glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE, 2.6f); glPushMatrix(); glMultMatrixf(m); glScalef(bs, bs, bs); drawModel(bike); glPopMatrix();
                if (riderMesh >= 0) {                   // piloto: espacio del modelo (x der., y adelante, z arriba) -> espacio de la bici ensamblada (adelante = -Z, arriba = +Y); posición de la pelvis = HIPÓTESIS (DH_RIDER_AT)
                    rider::applySkin(riderSk.skin(riderPose.data(), riderPose.size()), riderSkin, riderBind, riderVerts); gr.updateVertices(riderMesh, riderVerts);
                    // Modelo del piloto (X der., Y adelante, Z arriba) -> bici ensamblada (Y arriba, adelante = -Z): (x, y, z) -> (x, z, -y).
                    float conv[16] = {1, 0, 0, 0,  0, 0, -1, 0,  0, 1, 0, 0,  riderAt[0], riderAt[1] - 3.548f, riderAt[2], 1};   // la pelvis del modelo está a z = 3.548
                    if (riderApprox) {                  // pelvis optimizada en el marco del modelo -> desplazamiento de la raíz en el marco de la bici ensamblada
                        const auto root = riderSk.worldPositions(riderPose.data(), riderPose.size())[0];
                        const float tx = riderPelvisZ[0] - root[0], ty = riderPelvisZ[1] - root[1], tz = riderPelvisZ[2] - root[2];
                        conv[12] = tx; conv[13] = tz; conv[14] = -ty;
                    }
                    glPushMatrix(); glMultMatrixf(m); glScalef(bs, bs, bs); gr.draw(riderMesh, conv, 2.6f); glPopMatrix();
                }
            }
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
        if ((ride || play) && bike.mi.empty()) {                 // marcador del ciclista: pirámide roja orientada al rumbo (si no hay DH_BIKE)
            glPushMatrix(); glTranslatef(rx0, ry0, rz0); glRotatef(-rh * 57.2958f, 0, 1, 0);
            float b = 0.35f * U, hgt = 1.2f * U, l = 0.9f * U;
            glBegin(GL_TRIANGLES); glColor3f(0.9f, 0.1f, 0.1f);
            glVertex3f(-b, 0, l); glVertex3f(b, 0, l); glVertex3f(0, hgt, 0);
            glColor3f(0.7f, 0.05f, 0.05f); glVertex3f(-b, 0, l); glVertex3f(0, hgt, 0); glVertex3f(0, 0, -l);
            glVertex3f(b, 0, l); glVertex3f(0, 0, -l); glVertex3f(0, hgt, 0);
            glColor3f(0.4f, 0.f, 0.f); glVertex3f(-b, 0, l); glVertex3f(0, 0, -l); glVertex3f(b, 0, l);
            glEnd(); glPopMatrix();
        }
        if (!colLines.empty() && std::getenv("DH_COLDRAW")) { glEnableClientState(GL_VERTEX_ARRAY); glColor3f(0.1f, 1.f, 0.3f); glVertexPointer(3, GL_FLOAT, 0, colLines.data()); glDrawArrays(GL_LINES, 0, (GLsizei)(colLines.size() / 3)); }
        if (!overlay.empty()) {
            glDisable(GL_DEPTH_TEST); glEnableClientState(GL_VERTEX_ARRAY); glColor3f(1, 0.2f, 0.9f); glPointSize(5);
            glVertexPointer(3, GL_FLOAT, 0, overlay.data()); glDrawArrays(GL_POINTS, 0, (GLsizei)(overlay.size() / 3)); glEnable(GL_DEPTH_TEST);
        }
        if (play) {                                          // HUD: barra de velocidad (amarilla, 100 u/s a tope) y una casilla por puerta (verde = cruzada; todas verdes + barra verde = meta)
            glDisable(GL_TEXTURE_2D); glDisable(GL_DEPTH_TEST); glDisable(GL_FOG); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity(); glOrtho(0, ww, 0, hh, -1, 1); glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
            auto rect = [&](float x, float y, float rw, float rh2, float r, float g, float b, float a) { glColor4f(r, g, b, a); glBegin(GL_QUADS); glVertex2f(x, y); glVertex2f(x + rw, y); glVertex2f(x + rw, y + rh2); glVertex2f(x, y + rh2); glEnd(); };
            float bw = ww * 0.28f, bh = hh * 0.03f, x0 = ww * 0.03f, y0 = hh * 0.05f; size_t ng = gts.size();
            rect(x0 - 3, y0 - 3, bw + 6, bh + 6, 0, 0, 0, 0.55f); rect(x0, y0, bw * std::fmin(1.f, prun.bike.speed() / 100.f), bh, prun.gs.finished ? 0.2f : 0.95f, prun.gs.finished ? 0.9f : 0.8f, 0.15f, 0.95f);
            for (size_t i = 0; i < ng; i++) { float cw = bw / (float)ng; bool done = i < prun.gs.counter; rect(x0 + cw * i + 1, y0 + bh * 1.8f, cw - 2, bh * 0.7f, done ? 0.2f : 0.35f, done ? 0.9f : 0.35f, done ? 0.3f : 0.35f, 0.85f); }
            glPopMatrix(); glMatrixMode(GL_PROJECTION); glPopMatrix(); glMatrixMode(GL_MODELVIEW); glEnable(GL_DEPTH_TEST); glDisable(GL_BLEND);
        }
        if (play) {                                          // texto provisional (src/text.hpp): cronómetro, velocidad, puertas, cuenta atrás, aviso de meta y pantalla de resultados
            static int hudTex[hud::kTexCount]; static int hudState = 0;   // 0 = sin intentar, 1 = cargado, -1 = no disponible (se usa el texto provisional)
            if (hudState == 0) { std::vector<uint8_t> hd; std::vector<gfx::Texture> ht; const char* hp = std::getenv("DH_HUD") ? std::getenv("DH_HUD") : "out/hud/hud.dat";
                if (readFile(hp, hd) && hud::load(hd, ht)) { for (int i = 0; i < hud::kTexCount; i++) hudTex[i] = gr.createTexture(ht[i]); hudState = 1; } else { hudState = -1; std::fprintf(stderr, "aviso: sin texturas de HUD (%s): python3 tools/hud_export.py unpacked/LVL/ALP2\n", hp); } }
            std::vector<gfx::Quad> tq; const float S = std::fmax(2.f, std::floor(ww / 320.f)); char b[64];
            auto line = [&](const std::string& t, float x, float y, float sc, float r, float g2, float b2) { gfx::textQuads(t, x + sc, y + sc, sc, 0, 0, 0, 0.7f, tq); gfx::textQuads(t, x, y, sc, r, g2, b2, 1.f, tq); };   // con sombra
            auto mid = [&](const std::string& t, float y, float sc, float r, float g2, float b2) { float x = ww * 0.5f - gfx::textWidth(t, sc) * 0.5f; line(t, x, y, sc, r, g2, b2); };
            if (race.state == RaceState::Results) {
                tq.push_back({0, 0, (float)ww, (float)hh, 0, 0, 0, 0.6f});
                mid("TIME TRIAL RESULTS", hh * 0.18f, S * 2.f, 1.f, 0.85f, 0.2f);                     // título: texto real del ELF (0x2774DC)
                std::snprintf(b, sizeof b, "TIME  %s", formatTime(race.res.time).c_str()); mid(b, hh * 0.34f, S * 2.f, 1, 1, 1);               // etiquetas provisionales (no son del ELF)
                std::snprintf(b, sizeof b, "TOP SPEED  %.0f KM/H", 1.0973f * race.res.maxSpeed); mid(b, hh * 0.46f, S * 1.5f, 1, 1, 1);
                std::snprintf(b, sizeof b, "GATES  %zu/%zu", race.res.gates, race.res.totalGates); mid(b, hh * 0.54f, S * 1.5f, 1, 1, 1);
                std::snprintf(b, sizeof b, "RESETS  %u", race.res.respawns); mid(b, hh * 0.62f, S * 1.5f, 1, 1, 1);
                for (size_t i = 0; i < race.res.splits.size() && i < 3; i++) { std::snprintf(b, sizeof b, "SPLIT %zu  %s", i + 1, formatTime(race.res.splits[i]).c_str()); mid(b, hh * (0.72f + 0.05f * i), S, 0.8f, 0.8f, 0.8f); }
            } else {
                if (hudState == 1) {                                                                      // ordenador de la bici con las texturas del juego (reloj mm:ss; tercera cifra = puertas: hipótesis)
                    std::vector<gfx::TexQuad> hq, lq, kq; std::string clk = formatTime(race.displayTime()).substr(0, 5), third = std::to_string(prun.gs.counter);
                    hud::bikeComputer((int)ww, (int)hh, (int)std::lround(1.0973f * prun.bike.speed()), clk, third, hq, lq, kq);
                    gr.drawTexQuads2D(hudTex[hud::kHousing], hq, (int)ww, (int)hh); gr.drawTexQuads2D(hudTex[hud::kLcdFont], lq, (int)ww, (int)hh); gr.drawTexQuads2D(hudTex[hud::kKph], kq, (int)ww, (int)hh);
                    { std::vector<gfx::Quad> ledq; hud::bikeComputerLeds((int)ww, (int)hh, (int)std::lround(1.0973f * prun.bike.speed()), ledq); gr.drawQuads2D(ledq, (int)ww, (int)hh); }
                } else {
                    line(formatTime(race.displayTime()), ww * 0.03f, hh * 0.04f, S * 2.f, 1, 1, 1);
                    std::snprintf(b, sizeof b, "%.0f KM/H", 1.0973f * prun.bike.speed()); line(b, ww * 0.97f - gfx::textWidth(b, S * 1.5f), hh * 0.04f, S * 1.5f, 1, 0.85f, 0.2f);
                    std::snprintf(b, sizeof b, "GATES %zu/%zu", prun.gs.counter, gts.size()); line(b, ww * 0.03f, hh * 0.04f + 10 * S * 2.f, S, 0.8f, 1, 0.8f);
                }
                if (race.state == RaceState::Countdown) { std::snprintf(b, sizeof b, "%d", race.countdownDigit()); mid(b, hh * 0.3f, S * 8.f, 1, 0.85f, 0.2f); }
                else if (race.state == RaceState::Riding && race.displayTime() < 1.0) mid("GO!", hh * 0.3f, S * 8.f, 0.3f, 1, 0.3f);                       // provisional (la cadena real del juego no está localizada)
                else if (race.state == RaceState::Finished) mid("GREAT FINISH!", hh * 0.3f, S * 4.f, 1, 0.85f, 0.2f);                                       // texto real del ELF (0x277614); su uso aquí es hipótesis
            }
            gr.drawQuads2D(tq, (int)ww, (int)hh);
        }
        static const int shotFrame = std::getenv("DH_FRAMES") ? std::atoi(std::getenv("DH_FRAMES")) : 3;
        if (shot && frame == shotFrame - 1 && play) std::printf("play: estado=%s cuenta=%d reloj=%s velocidad=%.1f u/s puertas=%zu/%zu\n", raceStateName(race.state), race.countdownDigit(), formatTime(race.displayTime()).c_str(), prun.bike.speed(), prun.gs.counter, gts.size());
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
    if (const char* lp = std::getenv("DH_INPUT_LOG")) if (!g_inLogSaved && !g_inLog.empty()) { g_inLogSaved = saveInputs(lp, g_inLog); std::fprintf(stderr, "registro de entradas (al salir): %zu ticks -> %s (%s)\n", g_inLog.size(), lp, g_inLogSaved ? "ok" : "error"); }
    SDL_GL_DestroyContext(gl); SDL_DestroyWindow(w); SDL_Quit();
}
