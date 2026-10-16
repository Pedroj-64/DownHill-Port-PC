// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Verificación con datos reales (no va a CI): ngp_markers_check nivel.NGP nivel.PTS salida_dir -> escribe gates.bin, start.bin y chain0.bin con src/markers.hpp; tools/ngp_markers_check.py los compara byte a byte con tools/markers.py y tools/pts_path.py.
#include "../src/markers.hpp"
#include <cstdio>
#include <string>
static bool rd(const char* p, std::vector<uint8_t>& r) { FILE* f = std::fopen(p, "rb"); if (!f) return false; std::fseek(f, 0, SEEK_END); r.resize(std::ftell(f)); std::rewind(f); bool ok = std::fread(r.data(), 1, r.size(), f) == r.size(); std::fclose(f); return ok; }
static bool wr(const std::string& p, const std::vector<uint8_t>& v) { FILE* f = std::fopen(p.c_str(), "wb"); if (!f) return false; bool ok = std::fwrite(v.data(), 1, v.size(), f) == v.size(); std::fclose(f); return ok; }
int main(int argc, char** argv) {
    std::vector<uint8_t> n, p; if (argc < 4 || !rd(argv[1], n) || !rd(argv[2], p)) { std::fprintf(stderr, "uso: ngp_markers_check nivel.NGP nivel.PTS salida_dir\\n"); return 2; }
    const std::string d = argv[3];
    return !(wr(d + "/gates.bin", markers::gatesFile(n)) && wr(d + "/start.bin", markers::startPointsFile(n)) && wr(d + "/chain0.bin", markers::ptsChainFile(p, 0)));
}
