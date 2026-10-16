// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Nivel completo desde el .NGP/.PTR/.TEX/.RTX (y el .PTS) SIN pasar por las herramientas Python: usa los pasos 1-5 del loader nativo (docs/native-loader-plan.md). Devuelve «archivos virtuales» con los mismos nombres
// y bytes que dejaría tools/export_all.sh (<base>.mdl, .sky.mdl, .dome.mdl, .col, .gates, .start.pts, .chain0.pts) para que dhview los lea con el mismo código que los de disco.
#pragma once
#include "mesh.hpp"
#include "markers.hpp"
#include "bikeasm.hpp"
#include "iso9660.hpp"
#include "ie.hpp"
#include <functional>
#include <map>
#include <cstdio>
#include <string>

namespace nativelevel {
inline bool readAll(const std::string& p, std::vector<uint8_t>& r) { FILE* f = std::fopen(p.c_str(), "rb"); if (!f) return false; std::fseek(f, 0, SEEK_END); long n = std::ftell(f); if (n < 0) { std::fclose(f); return false; } r.resize((size_t)n); std::rewind(f); bool ok = std::fread(r.data(), 1, r.size(), f) == r.size(); std::fclose(f); return ok; }

struct Level { std::string base; std::map<std::string, std::vector<uint8_t>> files; };
using Reader = std::function<bool(const std::string&, std::vector<uint8_t>&)>;   // ruta relativa a la raíz del juego (p. ej. "LVL/ALP2.NGP") -> bytes ya descomprimidos

// Nivel `name` leído a través de `rd` (carpeta extraída o imagen ISO): LVL/<name>.NGP|PTR|TEX|RTX y PTS/<name>.PTS. `lvlDir` = directorio del nivel dentro de la raíz ("LVL"); `base` = prefijo de los nombres virtuales.
inline bool loadFrom(const Reader& rd, const std::string& lvlDir, const std::string& name, const std::string& base, Level& out, std::string& err) {
    out.base = base; const std::string b = base;
    std::vector<uint8_t> n, p, t, c; const std::string f = lvlDir + "/" + name;
    if (!rd(f + ".NGP", n) || !rd(f + ".PTR", p) || !rd(f + ".TEX", t) || !rd(f + ".RTX", c)) { err = "faltan .NGP/.PTR/.TEX/.RTX de " + name; return false; }
    tex::Level L; if (!tex::load(n, p, t, c, L)) { err = "PTR/TEX inválido"; return false; }
    mesh::LevelFiles lf; if (!mesh::extractLevel(n, L, lf)) { err = "NGP inválido (geometría)"; return false; }
    out.files[b + ".mdl"] = std::move(lf.main); if (lf.hasSky) out.files[b + ".sky.mdl"] = std::move(lf.sky); if (lf.hasDome) out.files[b + ".dome.mdl"] = std::move(lf.dome);
    std::vector<ngp::ColTri> tris; if (!ngp::collisionTrisInstanced(n, tris)) { err = "NGP inválido (colisión)"; return false; }
    std::vector<uint8_t> col(4 + 40 * tris.size(), 0); const uint32_t nt = (uint32_t)tris.size(); std::memcpy(col.data(), &nt, 4);
    for (size_t i = 0; i < tris.size(); i++) { std::memcpy(&col[4 + 40 * i], tris[i].v, 36); std::memcpy(&col[4 + 40 * i + 36], &tris[i].surface, 2); }
    out.files[b + ".col"] = std::move(col);
    out.files[b + ".gates"] = markers::gatesFile(n); out.files[b + ".start.pts"] = markers::startPointsFile(n);
    std::vector<uint8_t> pts; if (rd("PTS/" + name + ".PTS", pts)) out.files[b + ".chain0.pts"] = markers::ptsChainFile(pts, 0);
    return true;
}
// Lector sobre una carpeta extraída (raíz del juego).
inline Reader dirReader(const std::string& root) { return [root](const std::string& rel, std::vector<uint8_t>& o) { return readAll(root + "/" + rel, o); }; }
// `ngpPath` = ruta del .NGP (p. ej. .../LVL/ALP2.NGP); la raíz del juego es el directorio padre de LVL. Los nombres virtuales cuelgan de la ruta sin extensión.
inline bool load(const std::string& ngpPath, Level& out, std::string& err) {
    if (ngpPath.size() < 5) { err = "ruta inválida"; return false; }
    const std::string b = ngpPath.substr(0, ngpPath.size() - 4); const size_t slash = b.find_last_of("/\\"); const std::string name = slash == std::string::npos ? b : b.substr(slash + 1);
    const std::string dir = slash == std::string::npos ? "." : b.substr(0, slash); const size_t up = dir.find_last_of("/\\"); const std::string lvlDir = up == std::string::npos ? dir : dir.substr(up + 1), root = up == std::string::npos ? "." : dir.substr(0, up);
    return loadFrom(dirReader(root), lvlDir, name, b, out, err);
}
// Imagen ISO del jugador: rutas sin distinguir mayúsculas; cada archivo pasa por los contenedores IE (src/ie.hpp). Si una capa IE falla se usan los bytes originales, como tools/unpack_ie.py.
class IsoSource {
public:
    bool open(const std::string& isoPath) {
        if (!img_.open(isoPath)) return false; std::vector<iso9660::Entry> es; if (!img_.list(es)) return false;
        for (auto& e : es) if (!e.dir) { std::string k = e.path; for (char& ch : k) ch = (char)std::toupper((unsigned char)ch); idx_[k] = e; } return true;
    }
    bool read(const std::string& rel, std::vector<uint8_t>& out) const {
        std::string k = rel; for (char& ch : k) ch = (char)std::toupper((unsigned char)ch); auto it = idx_.find(k); if (it == idx_.end()) return false;
        std::vector<uint8_t> raw; if (!img_.read(it->second, raw)) return false; if (!ie::unpack(raw, out)) out = std::move(raw); return true;
    }
    Reader reader() const { return [this](const std::string& rel, std::vector<uint8_t>& o) { return read(rel, o); }; }
    bool has(const std::string& rel) const { std::string k = rel; for (char& ch : k) ch = (char)std::toupper((unsigned char)ch); return idx_.count(k) != 0; }
private:
    iso9660::Image img_; std::map<std::string, iso9660::Entry> idx_;
};

// Bici ensamblada (cuadro + manillar + rueda, las de tools/dh.py) desde BIKE/*.NGP leídos con `rd`. false si no hay datos de BIKE.
inline bool loadBikeFrom(const Reader& rd, std::vector<uint8_t>& mdl) {
    std::vector<uint8_t> parts[3]; const char* names[3] = {"CANFIELD", "MAN100", "WHBRIAN"};
    for (int k = 0; k < 3; k++) {
        const std::string b = std::string("BIKE/") + names[k]; std::vector<uint8_t> n, p, t, c; if (!rd(b + ".NGP", n) || !rd(b + ".PTR", p) || !rd(b + ".TEX", t) || !rd(b + ".RTX", c)) return false;
        tex::Level L; if (!tex::load(n, p, t, c, L) || !mesh::extractStatic(n, L, parts[k])) return false;
    }
    return bikeasm::assemble(parts[0], parts[1], parts[2], {}, mdl);
}
// Variante antigua: `levelBase` = base del nivel (.../LVL/N); la raíz es el padre de LVL.
inline bool loadBike(const std::string& levelBase, std::vector<uint8_t>& mdl) {
    const size_t slash = levelBase.find_last_of("/\\"); if (slash == std::string::npos) return false; const std::string dir = levelBase.substr(0, slash); const size_t up = dir.find_last_of("/\\"); const std::string root = up == std::string::npos ? "." : dir.substr(0, up);
    return loadBikeFrom(dirReader(root), mdl);
}
}  // namespace nativelevel
