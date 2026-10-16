// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Nivel completo desde el .NGP/.PTR/.TEX/.RTX (y el .PTS) SIN pasar por las herramientas Python: usa los pasos 1-5 del loader nativo (docs/native-loader-plan.md). Devuelve «archivos virtuales» con los mismos nombres
// y bytes que dejaría tools/export_all.sh (<base>.mdl, .sky.mdl, .dome.mdl, .col, .gates, .start.pts, .chain0.pts) para que dhview los lea con el mismo código que los de disco.
#pragma once
#include "mesh.hpp"
#include "markers.hpp"
#include "bikeasm.hpp"
#include <cstdio>
#include <string>

namespace nativelevel {
inline bool readAll(const std::string& p, std::vector<uint8_t>& r) { FILE* f = std::fopen(p.c_str(), "rb"); if (!f) return false; std::fseek(f, 0, SEEK_END); long n = std::ftell(f); if (n < 0) { std::fclose(f); return false; } r.resize((size_t)n); std::rewind(f); bool ok = std::fread(r.data(), 1, r.size(), f) == r.size(); std::fclose(f); return ok; }

struct Level { std::string base; std::map<std::string, std::vector<uint8_t>> files; };

// `ngpPath` = ruta del .NGP (p. ej. .../LVL/ALP2.NGP). La línea de carrera sale de <dir de LVL>/../PTS/<nombre>.PTS si existe.
inline bool load(const std::string& ngpPath, Level& out, std::string& err) {
    if (ngpPath.size() < 5) { err = "ruta inválida"; return false; }
    out.base = ngpPath.substr(0, ngpPath.size() - 4); const std::string& b = out.base;
    std::vector<uint8_t> n, p, t, c;
    if (!readAll(b + ".NGP", n) || !readAll(b + ".PTR", p) || !readAll(b + ".TEX", t) || !readAll(b + ".RTX", c)) { err = "faltan .NGP/.PTR/.TEX/.RTX junto a " + ngpPath; return false; }
    tex::Level L; if (!tex::load(n, p, t, c, L)) { err = "PTR/TEX inválido"; return false; }
    mesh::LevelFiles lf; if (!mesh::extractLevel(n, L, lf)) { err = "NGP inválido (geometría)"; return false; }
    out.files[b + ".mdl"] = std::move(lf.main); if (lf.hasSky) out.files[b + ".sky.mdl"] = std::move(lf.sky); if (lf.hasDome) out.files[b + ".dome.mdl"] = std::move(lf.dome);
    std::vector<ngp::ColTri> tris; if (!ngp::collisionTrisInstanced(n, tris)) { err = "NGP inválido (colisión)"; return false; }
    std::vector<uint8_t> col(4 + 40 * tris.size(), 0); const uint32_t nt = (uint32_t)tris.size(); std::memcpy(col.data(), &nt, 4);
    for (size_t i = 0; i < tris.size(); i++) { std::memcpy(&col[4 + 40 * i], tris[i].v, 36); std::memcpy(&col[4 + 40 * i + 36], &tris[i].surface, 2); }
    out.files[b + ".col"] = std::move(col);
    out.files[b + ".gates"] = markers::gatesFile(n); out.files[b + ".start.pts"] = markers::startPointsFile(n);
    const size_t slash = b.find_last_of("/\\"); const std::string stem = slash == std::string::npos ? b : b.substr(slash + 1);
    const std::string dir = slash == std::string::npos ? "." : b.substr(0, slash); const size_t up = dir.find_last_of("/\\"); const std::string root = up == std::string::npos ? "." : dir.substr(0, up);
    std::vector<uint8_t> pts; if (readAll(root + "/PTS/" + stem + ".PTS", pts)) out.files[b + ".chain0.pts"] = markers::ptsChainFile(pts, 0);
    return true;
}

// Bici ensamblada (cuadro + manillar + rueda, las de tools/dh.py) desde <raíz>/BIKE/*.NGP; `levelBase` = base del nivel (.../LVL/N). false si no hay datos de BIKE.
inline bool loadBike(const std::string& levelBase, std::vector<uint8_t>& mdl) {
    const size_t slash = levelBase.find_last_of("/\\"); if (slash == std::string::npos) return false; const std::string dir = levelBase.substr(0, slash); const size_t up = dir.find_last_of("/\\"); const std::string root = up == std::string::npos ? "." : dir.substr(0, up);
    std::vector<uint8_t> parts[3]; const char* names[3] = {"CANFIELD", "MAN100", "WHBRIAN"};
    for (int k = 0; k < 3; k++) {
        const std::string b = root + "/BIKE/" + names[k]; std::vector<uint8_t> n, p, t, c; if (!readAll(b + ".NGP", n) || !readAll(b + ".PTR", p) || !readAll(b + ".TEX", t) || !readAll(b + ".RTX", c)) return false;
        tex::Level L; if (!tex::load(n, p, t, c, L) || !mesh::extractStatic(n, L, parts[k])) return false;
    }
    return bikeasm::assemble(parts[0], parts[1], parts[2], {}, mdl);
}
}  // namespace nativelevel
