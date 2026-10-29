// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Peatones / espectadores (HIPÓTESIS, docs/formats/gs-dump.md): puntos de aparición de PTS/<NIVEL>.PED + modelos de personaje del propio NGP.
//  * Registro PED de 32 B: x y z f32, u32 +16 = variante, u16 +20 = {bit 0 del byte bajo = es un peatón (FUN_001963f8: «(u16 +0x14) & 1»), byte alto = tipo (FUN_00127398 lee +0x15)}.
//  * Modelos: toda cadena de malla con kind 4000..4495 múltiplo de 5 (salvo 4400) es una plantilla de personaje (FUN_00195b80 -> FUN_00195fb8) y su tipo sale de FUN_001957c0
//    (4090-4110 -> 4, 4120-4130 -> 6, ...). Misma cadena VIF que el piloto (rider_mesh.hpp). Se dibujan en pose de reposo del modelo (sin animación), de cara a la línea de carrera.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <array>
#include <map>
#include <vector>
#include "renderer.hpp"
#include "rider.hpp"
#include "rider_mesh.hpp"
#include "tex.hpp"

namespace peds {
// Tipo de peatón de una plantilla por su kind (FUN_001957c0).
inline int typeOfKind(uint32_t k) {
    if (k < 4000 || k - 4000 < 0x1a) return 0;
    auto in = [&](uint32_t lo, uint32_t n) { return k >= lo && k - lo < n; };
    if (in(0xfbe, 0x15)) return 1; if (in(0xfe1, 0x15)) return 3; if (in(0xffa, 0x15)) return 4; if (k == 0x1013) return 5; if (in(0x1018, 0xb)) return 6;
    if (in(0x1027, 0xb)) return 7; if (k == 0x1036) return 8; if (in(0x103b, 6)) return 9; if (in(0x104a, 0x10)) return 10; if (in(0x105e, 0x15)) return 0xb;
    if (in(0x1077, 0x15)) return 0xc; if (in(0x1090, 0x15)) return 0xd; if (in(0x10a9, 0x15)) return 0xe; if (in(0x10c2, 0x15)) return 0xf; if (k == 0x10db) return 0x10;
    if (in(0x10e0, 0x15)) return 2;
    return 0;
}

struct Point { float x, y, z; uint8_t type; uint32_t sub; };   // x y z ya en el espacio del visor (Y arriba): (x, y, z) del archivo -> (x, z, -y)
struct Inst { size_t model; float x, y, z, tx, tz; bool cheer; };             // posición y punto de la línea de carrera hacia el que mira (espacio del visor)

inline std::vector<Point> parse(const std::vector<uint8_t>& d) {
    std::vector<Point> out; uint32_t magic = 0, n = 0;
    if (d.size() < 8) return out;
    std::memcpy(&magic, d.data(), 4); std::memcpy(&n, d.data() + 4, 4);
    if (magic != 0x10182001u || d.size() != 8 + static_cast<size_t>(n) * 32) return out;
    for (uint32_t i = 0; i < n; i++) {
        const uint8_t* r = d.data() + 8 + static_cast<size_t>(i) * 32; float p[3]; uint32_t sub = 0; uint16_t u = 0;
        std::memcpy(p, r, 12); std::memcpy(&sub, r + 16, 4); std::memcpy(&u, r + 20, 2);
        if (!(u & 1) || !std::isfinite(p[0] + p[1] + p[2])) continue;   // bit 0 = peatón; el resto (bit 0 = 0) son otros puntos de la lista
        out.push_back({p[0], p[2], -p[1], static_cast<uint8_t>(u >> 8), sub});
    }
    return out;
}

// Malla de personaje (rider_mesh) de la plantilla `kind` del NGP con sus texturas ya creadas; root = nodo raíz del esqueleto. false si ese kind no existe en el nivel.
inline bool loadKindMesh(gfx::Renderer& gr, const std::vector<uint8_t>& ngp, const tex::Level& TL, const rider_mesh::TextureResolver& base, uint32_t kind,
                         rider_mesh::Mesh& m, std::vector<gfx::TextureId>& ids, size_t& root) {
    const size_t none = std::numeric_limits<size_t>::max(); size_t off = none, end = none; root = none;
    if (!rider_mesh::findChain(ngp, off, kind, &end, &root) || off == none) return false;
    rider_mesh::TextureResolver rs = base; rs.firstKeys.clear(); m = rider_mesh::Mesh{};
    if (!(end != none ? rider_mesh::loadRange(ngp, off, end, m, &rs) : rider_mesh::load(ngp, off, m, &rs)) || m.indices.empty()) return false;
    if (!std::getenv("DH_PED_NOFLIP")) for (size_t i = 4; i < m.vertices.size(); i += 10) m.vertices[i] = 1.f - m.vertices[i];   // HIPÓTESIS: las UV de los personajes no llevan la inversión de V de las mallas de nivel (el rostro cae en las filas inferiores del atlas)
    ids.clear();
    if (std::getenv("DH_PED_DEBUG")) for (const auto& key : rs.firstKeys) for (const tex::Material& mt : TL.mats) if (mt.tid == key.first && mt.cbp == key.second) { const auto it = TL.texs.find(mt.tid);
        std::fprintf(stderr, "  kind %u material tid %u cbp %u psm %#x %ux%u | registro %ux%u fmt %u datos %zu\n", kind, mt.tid, mt.cbp, mt.psm, mt.w, mt.h, it == TL.texs.end() ? 0 : it->second.w, it == TL.texs.end() ? 0 : it->second.h, it == TL.texs.end() ? 0 : it->second.fmt, it == TL.texs.end() ? (size_t)0 : it->second.data.size()); break; }
    for (const auto& key : rs.firstKeys) { gfx::TextureId id = -1; for (const tex::Material& mt : TL.mats) if (mt.tid == key.first && mt.cbp == key.second) { uint32_t W, H; std::vector<uint8_t> px; if (tex::makeTexture(TL, mt, W, H, px)) { id = gr.createTexture({static_cast<int>(W), static_cast<int>(H), px});
            if (const char* dd = std::getenv("DH_PED_DUMPTEX")) { char fn[512]; std::snprintf(fn, sizeof fn, "%s/k%u_t%d.ppm", dd, kind, id); if (FILE* f = std::fopen(fn, "wb")) { std::fprintf(f, "P6 %u %u 255\n", W, H); for (size_t i = 0; i < px.size(); i += 4) std::fwrite(&px[i], 1, 3, f); std::fclose(f); } } } break; } ids.push_back(id); }
    return true;
}
inline bool loadTex(const std::vector<uint8_t>& ngp, const std::vector<uint8_t>& ptr, const std::vector<uint8_t>& texd, const std::vector<uint8_t>& rtx, tex::Level& TL, rider_mesh::TextureResolver& base) {
    if (!tex::load(ngp, ptr, texd, rtx, TL)) return false;
    for (const tex::Material& m : TL.mats) { uint32_t W, H; std::vector<uint8_t> px; base.materials.push_back({m.tex0Off, static_cast<uint16_t>(m.tid), m.cbp, tex::makeTexture(TL, m, W, H, px)}); }
    return true;
}

// Piloto dinámico (piel por CPU con la pose compartida del piloto del jugador): para los rivales fantasma.
struct Racer { gfx::MeshId mesh = -1; rider::Skeleton sk; std::vector<rider::SkinVertex> skin; std::vector<float> bind, verts; };
inline bool loadRacers(gfx::Renderer& gr, const std::vector<uint8_t>& ngp, const std::vector<uint8_t>& ptr, const std::vector<uint8_t>& texd, const std::vector<uint8_t>& rtx,
                       const std::vector<uint32_t>& kinds, std::vector<Racer>& out) {
    tex::Level TL; rider_mesh::TextureResolver base; if (!loadTex(ngp, ptr, texd, rtx, TL, base)) return false;
    for (uint32_t k : kinds) {
        rider_mesh::Mesh m; std::vector<gfx::TextureId> ids; size_t root; if (!loadKindMesh(gr, ngp, TL, base, k, m, ids, root)) continue;
        Racer r; const size_t nv = m.vertices.size() / 10;
        if (root == std::numeric_limits<size_t>::max() || !r.sk.load(ngp, root) || m.boneIds.size() != 3 * nv) continue;
        r.skin.resize(nv); r.bind.resize(3 * nv); r.verts = m.vertices;
        for (size_t i = 0; i < nv; i++) for (int c = 0; c < 3; c++) { r.skin[i].bone[c] = m.boneIds[3 * i + c]; r.skin[i].w[c] = m.weights[3 * i + c]; r.bind[3 * i + c] = m.vertices[10 * i + c]; }
        r.mesh = gr.createMesh(r.verts, m.indices, ids, true); out.push_back(std::move(r));
    }
    return !out.empty();
}

// FUN_00127c60 (medido con un savestate: DAT_00329658/5C = Z del jugador): un grupo se instancia si la altura del jugador cae en [min Z, max Z) de su caja +200 u; X e Y no cuentan.
// shortcut: la caja del juego incluye los puntos del grupo que no son peatones (aquí ya descartados), así que la nuestra puede ser algo más estrecha; con 1 jugador basta.
inline bool groupActive(const std::array<float, 6>& b, float y) { return y >= b[1] - 200.f && y < b[4] + 200.f; }

// Ovación medida con pedtrace.py en 2 carreras (165 peatones, state+8 >= 2 = vitorea, FUN_001277e0): tipo 2 = vitorea con un piloto a <= 200 u, tipo 6 = solo con uno a ~40 u, tipo 0 casi nunca (9 de 140).
// shortcut: el disparador de los pocos tipo 0 que vitorean no se ve en esos datos (se omite), y los umbrales salen de pocas decenas de transiciones; d2 = distancia^2 al jugador, no al piloto más cercano.
inline bool cheers(uint8_t type, float d2) { return (type == 2 && d2 < 200.f * 200.f) || (type == 6 && d2 < 40.f * 40.f); }

struct Set {
    struct Model { gfx::MeshId mesh, meshUp; int type; float height; };   // meshUp = la misma malla con los brazos en alto (ovación); -1 si no hay esqueleto
    std::vector<Model> models; std::vector<Point> pts; std::map<int, std::vector<size_t>> byType; std::map<uint32_t, std::array<float, 6>> gbox;   // caja (min xyz, max xyz) de cada grupo
    bool empty() const { return models.empty() || pts.empty(); }

    // Carga todas las plantillas de personaje del NGP del nivel (cada una con su textura) y los puntos PED. Devuelve false si no hay nada que dibujar.
    bool load(gfx::Renderer& gr, const std::vector<uint8_t>& ngp, const std::vector<uint8_t>& ptr, const std::vector<uint8_t>& texd, const std::vector<uint8_t>& rtx, const std::vector<uint8_t>& ped) {
        pts = parse(ped); if (pts.empty()) return false;
        // FUN_001963f8: cada grupo (campo `sub`) guarda como mucho 0x18 = 24 puntos (los primeros del archivo; en ALPINEMX el grupo 0 son 84 sueltos y solo valen 24) y su caja englobante crece 200 u por lado;
        // FUN_00127d70 instancia el grupo (a lo sumo 24 grupos activos) cuando se activa. HIPÓTESIS: se activa con el jugador dentro de la caja.
        { std::map<uint32_t, int> cnt; std::vector<Point> keep; for (const Point& q : pts) if (cnt[q.sub]++ < 24) keep.push_back(q); pts.swap(keep);
          gbox.clear(); for (const Point& q : pts) { auto it = gbox.find(q.sub); if (it == gbox.end()) it = gbox.emplace(q.sub, std::array<float, 6>{q.x, q.y, q.z, q.x, q.y, q.z}).first; auto& b = it->second;
              b[0] = std::fmin(b[0], q.x); b[1] = std::fmin(b[1], q.y); b[2] = std::fmin(b[2], q.z); b[3] = std::fmax(b[3], q.x); b[4] = std::fmax(b[4], q.y); b[5] = std::fmax(b[5], q.z); } }
        tex::Level TL; rider_mesh::TextureResolver base; if (!loadTex(ngp, ptr, texd, rtx, TL, base)) return false;
        const size_t none = std::numeric_limits<size_t>::max();
        for (uint32_t kind = 4000; kind < 4500; kind += 5) {
            if (kind == 4400) continue;
            rider_mesh::Mesh m; std::vector<gfx::TextureId> ids; size_t root = none; if (!loadKindMesh(gr, ngp, TL, base, kind, m, ids, root)) continue;
            if (std::getenv("DH_PED_UV") && kind == 4090) { const size_t nv = m.vertices.size() / 10; float zmin = 1e9f, zmax = -1e9f; for (size_t i = 0; i < nv; i++) { zmin = std::fmin(zmin, m.vertices[10*i+2]); zmax = std::fmax(zmax, m.vertices[10*i+2]); }
                FILE* f = std::fopen(std::getenv("DH_PED_UV"), "w"); if (f) { for (size_t i = 0; i < nv; i++) std::fprintf(f, "%.3f %.3f %.3f %.4f %.4f\n", m.vertices[10*i], m.vertices[10*i+1], m.vertices[10*i+2], m.vertices[10*i+3], m.vertices[10*i+4]); std::fclose(f); } std::fprintf(stderr, "uv dump z %.2f..%.2f\n", zmin, zmax); }
            if (std::getenv("DH_PED_DEBUG")) { std::fprintf(stderr, "kind %u: %zu vértices, texturas:", kind, m.vertices.size() / 10); for (auto id : ids) std::fprintf(stderr, " %d", id); std::fprintf(stderr, " | lotes %zu\n", m.batchMaterial.size()); }
            float lo = 1e30f, hi = -1e30f; for (size_t i = 2; i < m.vertices.size(); i += 10) { lo = std::fmin(lo, m.vertices[i]); hi = std::fmax(hi, m.vertices[i]); }
            // Pose de pie: el modelo en reposo tiene los brazos en cruz (T). Se bajan los hombros (joints 3 y 6, canales de rider.md; ángulos en DH_PED_ARM="a b c", HIPÓTESIS) y se hace una
            // segunda malla con los brazos en alto (canal 1 = -1,2 rad) para las ovaciones.
            gfx::MeshId down = -1, up = -1; std::vector<float> upVerts;
            if (root != none && m.boneIds.size() == 3 * (m.vertices.size() / 10)) {
                rider::Skeleton sk;
                if (sk.load(ngp, root) && sk.j.size() > 6) {
                    float arm[3] = {0.f, 1.45f, 0.f}; if (const char* e = std::getenv("DH_PED_ARM")) std::sscanf(e, "%f %f %f", arm, arm + 1, arm + 2);
                    const size_t nv = m.vertices.size() / 10; std::vector<rider::SkinVertex> skin(nv); std::vector<float> bind(3 * nv);
                    for (size_t i = 0; i < nv; i++) { for (int k = 0; k < 3; k++) { skin[i].bone[k] = m.boneIds[3 * i + k]; skin[i].w[k] = m.weights[3 * i + k]; bind[3 * i + k] = m.vertices[10 * i + k]; } }
                    auto posed = [&](const float a[3], std::vector<float>& out) { std::vector<float> pose(40, 0.f); for (int jn : {3, 6}) for (int k = 0; k < 3; k++) if (sk.j[jn].chan[k] >= 0 && sk.j[jn].chan[k] < 40) pose[sk.j[jn].chan[k]] = a[k];
                        out = m.vertices; rider::applySkin(sk.skin(pose.data(), pose.size()), skin, bind, out); };
                    const float cheer[3] = {0.f, -1.2f, 0.f}; std::vector<float> dv; posed(arm, dv); posed(cheer, upVerts);
                    m.vertices = dv; up = gr.createMesh(upVerts, m.indices, ids, false);
                }
            }
            down = gr.createMesh(m.vertices, m.indices, ids, false);
            const int t = typeOfKind(kind); byType[t].push_back(models.size());
            models.push_back({down, up, t, hi - lo});
        }
        return !models.empty();
    }

    // Peatones a dibujar: los del radio `radius` alrededor de (cx, cy, cz) más cercanos (hasta `budget`: el juego tiene un máximo de objetos de personaje a la vez, 21 del tipo 4 en ALPINEMX),
    // con modelo estable por punto y de cara al punto más cercano de la línea de carrera `line` (x y z sueltos en el espacio del visor).
    std::vector<Inst> select(float cx, float cy, float cz, const std::vector<float>& line, float radius, size_t budget, float time = 0.f) const {
        std::vector<std::pair<float, size_t>> cand;
        for (size_t i = 0; i < pts.size(); i++) {
            if (!byType.count(pts[i].type)) continue;
            const float dx = pts[i].x - cx, dy = pts[i].y - cy, dz = pts[i].z - cz, d2 = dx * dx + dy * dy + dz * dz;
            const auto gb = gbox.find(pts[i].sub); if (!std::getenv("DH_PED_NOGROUPS") && gb != gbox.end()) { const auto& b = gb->second; if (!groupActive(b, cy)) continue; }   // grupo activo solo por altura (FUN_00127c60)
            if (d2 < radius * radius) cand.push_back({d2, i});
        }
        std::sort(cand.begin(), cand.end()); if (cand.size() > budget) cand.resize(budget);
        std::vector<Inst> out;
        for (const auto& c : cand) {
            const Point& p = pts[c.second]; const auto& pool = byType.at(p.type);
            Inst in{pool[(c.second * 2654435761u >> 7) % pool.size()], p.x, p.y, p.z, p.x, p.z, cheers(p.type, c.first)};
            float best = 1e30f, tx = p.x, tz = p.z;
            for (size_t k = 0; k + 2 < line.size(); k += 3) { const float dx = line[k] - p.x, dz = line[k + 2] - p.z, d = dx * dx + dz * dz; if (d < best) { best = d; tx = line[k]; tz = line[k + 2]; } }
            in.tx = tx; in.tz = tz;
            out.push_back(in);
        }
        return out;
    }
};
}  // namespace peds
