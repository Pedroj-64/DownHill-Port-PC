// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Verificación con datos reales (no va a CI): ngp_col_check nivel.NGP nivel.col [--instances]  -> el lector nativo (src/ngp.hpp) debe dar EXACTAMENTE el .col de tools/collision.py [--instances]
#include "../src/ngp.hpp"
#include <cstdio>
static bool rd(const char* p, std::vector<uint8_t>& r) { FILE* f = std::fopen(p, "rb"); if (!f) return false; std::fseek(f, 0, SEEK_END); r.resize(std::ftell(f)); std::rewind(f); bool ok = std::fread(r.data(), 1, r.size(), f) == r.size(); std::fclose(f); return ok; }
int main(int argc, char** argv) {
    std::vector<uint8_t> n, c; std::vector<ngp::ColTri> t;
    if (argc < 3 || !rd(argv[1], n) || !rd(argv[2], c) || !(argc > 3 ? ngp::collisionTrisInstanced(n, t) : ngp::collisionTris(n, t))) { std::fprintf(stderr, "uso: ngp_col_check nivel.NGP nivel.col\n"); return 2; }
    uint32_t cn; std::memcpy(&cn, c.data(), 4); size_t bad = 0;
    if (cn != t.size()) { std::printf("DISTINTO: %zu triángulos nativos vs %u en el .col\n", t.size(), cn); return 1; }
    for (size_t i = 0; i < t.size(); i++) { uint8_t rec[40] = {}; std::memcpy(rec, t[i].v, 36); std::memcpy(rec + 36, &t[i].surface, 2); if (std::memcmp(rec, c.data() + 4 + 40 * i, 40)) bad++; }
    std::printf("%zu triángulos; %zu registros distintos del .col de Python\n", t.size(), bad); return bad != 0;
}
