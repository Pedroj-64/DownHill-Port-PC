// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Validación con datos reales (no va a CI): ground_pts_check nivel.col linea.pts  (linea.pts = f32 x,y,z de tools/pts_path.py --chain)
// Muestrea Ground::query sobre los puntos de la línea de carrera y reporta el error de altura (la línea queda a ~±12 u del suelo, no es la verdad exacta).
#include "../src/ground.hpp"
#include <algorithm>
#include <cstdio>
#include <vector>
static bool rd(const char* p, std::vector<uint8_t>& r) { FILE* f = std::fopen(p, "rb"); if (!f) return false; std::fseek(f, 0, SEEK_END); r.resize(std::ftell(f)); std::rewind(f); bool ok = std::fread(r.data(), 1, r.size(), f) == r.size(); std::fclose(f); return ok; }
int main(int argc, char** argv) {
    std::vector<uint8_t> a, b; Ground g;
    if (argc < 3 || !rd(argv[1], a) || !rd(argv[2], b) || !g.load(a)) { std::fprintf(stderr, "uso: ground_pts_check nivel.col linea.pts\n"); return 1; }
    const float* p = (const float*)b.data(); size_t n = b.size() / 12; std::vector<float> e; size_t miss = 0; float prev = 0; std::vector<float> jump;
    for (size_t i = 0; i < n; i++) {
        GroundHit h = g.query(p[3*i], p[3*i+1], p[3*i+2]); if (!h.hit) { miss++; continue; }
        e.push_back(std::fabs(h.height - p[3*i+1])); if (i && !e.empty()) { float j = std::fabs(h.height - prev); jump.push_back(j); if (j > 150) std::printf("  salto %.0f u en punto %zu (y=%.0f h=%.0f prev=%.0f)\n", j, i, p[3*i+1], h.height, prev); } prev = h.height;
    }
    std::sort(e.begin(), e.end()); double s = 0; for (float x : e) s += x;
    std::printf("%zu puntos, %zu sin suelo; |h - y_punto|: media %.1f u, p95 %.1f u, max %.1f u\n", n, miss, s / e.size(), e[e.size() * 95 / 100], e.back());
    std::sort(jump.begin(), jump.end()); std::printf("salto de altura entre puntos consecutivos: mediana %.1f u, p95 %.1f u, max %.1f u\n", jump[jump.size() / 2], jump[jump.size() * 95 / 100], jump.back());
}
