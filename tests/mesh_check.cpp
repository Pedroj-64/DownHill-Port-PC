// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Verificación con datos reales (no va a CI): mesh_check base_sin_extension salida.mdl -> modelo suelto con src/mesh.hpp (mismo .mdl DHM2 que tools/extract_model.py fuera de /LVL/); tools/mesh_check.py compara los bytes.
#include "../src/mesh.hpp"
#include <cstdio>
#include <string>
static bool rd(const std::string& p, std::vector<uint8_t>& r) { FILE* f = std::fopen(p.c_str(), "rb"); if (!f) return false; std::fseek(f, 0, SEEK_END); r.resize(std::ftell(f)); std::rewind(f); bool ok = std::fread(r.data(), 1, r.size(), f) == r.size(); std::fclose(f); return ok; }
int main(int argc, char** argv) {
    std::vector<uint8_t> n, p, t, c; const std::string b = argc > 1 ? argv[1] : "";
    if (argc < 3 || !rd(b + ".NGP", n) || !rd(b + ".PTR", p) || !rd(b + ".TEX", t) || !rd(b + ".RTX", c)) { std::fprintf(stderr, "uso: mesh_check base salida.mdl\n"); return 2; }
    tex::Level L; if (!tex::load(n, p, t, c, L)) { std::fprintf(stderr, "PTR/TEX inválido\n"); return 1; }
    std::vector<uint8_t> mdl; if (!mesh::extractStatic(n, L, mdl)) return 1;
    FILE* o = std::fopen(argv[2], "wb"); if (!o) return 2; std::fwrite(mdl.data(), 1, mdl.size(), o); std::fclose(o); return 0;
}
