// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Verificación con datos reales (no va a CI): bikeasm_check dir_BIKE salida.mdl [cuadro manillar rueda] -> extrae las 3 piezas con src/mesh.hpp y las ensambla con src/bikeasm.hpp (por defecto CANFIELD MAN100 WHBRIAN, como tools/dh.py);
// tools/bikeasm_check.py compara los bytes con out/play/bike1.mdl y con tools/assemble_bike.py.
#include "../src/bikeasm.hpp"
#include "../src/mesh.hpp"
#include <cstdio>
#include <string>
static bool rd(const std::string& p, std::vector<uint8_t>& r) { FILE* f = std::fopen(p.c_str(), "rb"); if (!f) return false; std::fseek(f, 0, SEEK_END); r.resize(std::ftell(f)); std::rewind(f); bool ok = std::fread(r.data(), 1, r.size(), f) == r.size(); std::fclose(f); return ok; }
static bool part(const std::string& base, std::vector<uint8_t>& mdl) {
    std::vector<uint8_t> n, p, t, c; if (!rd(base + ".NGP", n) || !rd(base + ".PTR", p) || !rd(base + ".TEX", t) || !rd(base + ".RTX", c)) return false;
    tex::Level L; return tex::load(n, p, t, c, L) && mesh::extractStatic(n, L, mdl);
}
int main(int argc, char** argv) {
    if (argc < 3) { std::fprintf(stderr, "uso: bikeasm_check dir_BIKE salida.mdl [cuadro manillar rueda]\n"); return 2; }
    const std::string d = argv[1]; const char* nm[3] = {argc > 3 ? argv[3] : "CANFIELD", argc > 4 ? argv[4] : "MAN100", argc > 5 ? argv[5] : "WHBRIAN"};
    std::vector<uint8_t> m[3]; for (int k = 0; k < 3; k++) if (!part(d + "/" + nm[k], m[k])) { std::fprintf(stderr, "no se puede leer %s\n", nm[k]); return 1; }
    std::vector<uint8_t> out; if (!bikeasm::assemble(m[0], m[1], m[2], {}, out)) return 1;
    FILE* o = std::fopen(argv[2], "wb"); if (!o) return 2; std::fwrite(out.data(), 1, out.size(), o); std::fclose(o); return 0;
}
