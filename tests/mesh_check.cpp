// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Verificación con datos reales (no va a CI): mesh_check base_sin_extension salida.mdl [--level] -> src/mesh.hpp: modelo suelto (.mdl DHM2) o, con --level, nivel (salida.mdl DHM3 + salida.sky.mdl + salida.dome.mdl si existen);
// tools/mesh_check.py compara los bytes con tools/extract_model.py.
#include "../src/mesh.hpp"
#include <cstdio>
#include <string>
static bool rd(const std::string& p, std::vector<uint8_t>& r) { FILE* f = std::fopen(p.c_str(), "rb"); if (!f) return false; std::fseek(f, 0, SEEK_END); r.resize(std::ftell(f)); std::rewind(f); bool ok = std::fread(r.data(), 1, r.size(), f) == r.size(); std::fclose(f); return ok; }
int main(int argc, char** argv) {
    std::vector<uint8_t> n, p, t, c; const std::string b = argc > 1 ? argv[1] : "";
    if (argc < 3 || !rd(b + ".NGP", n) || !rd(b + ".PTR", p) || !rd(b + ".TEX", t) || !rd(b + ".RTX", c)) { std::fprintf(stderr, "uso: mesh_check base salida.mdl\n"); return 2; }
    tex::Level L; if (!tex::load(n, p, t, c, L)) { std::fprintf(stderr, "PTR/TEX inválido\n"); return 1; }
    auto wr = [](const std::string& path, const std::vector<uint8_t>& v) { FILE* o = std::fopen(path.c_str(), "wb"); if (!o) return false; std::fwrite(v.data(), 1, v.size(), o); std::fclose(o); return true; };
    if (argc > 3 && std::string(argv[3]) == "--level") {
        mesh::LevelFiles lf; if (!mesh::extractLevel(n, L, lf)) return 1; const std::string out = argv[2], stem = out.size() > 4 ? out.substr(0, out.size() - 4) : out;
        bool ok = wr(out, lf.main); if (lf.hasSky) ok = wr(stem + ".sky.mdl", lf.sky) && ok; if (lf.hasDome) ok = wr(stem + ".dome.mdl", lf.dome) && ok; return ok ? 0 : 2;
    }
    std::vector<uint8_t> mdl; if (!mesh::extractStatic(n, L, mdl)) return 1; return wr(argv[2], mdl) ? 0 : 2;
}
