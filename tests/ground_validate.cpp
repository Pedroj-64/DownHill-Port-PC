// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Validación con datos reales (no va a CI): ground_validate nivel.col nivel.mdl [paso=100]
// Barrido vertical desde arriba (r=0) sobre una malla XZ y comparación con las alturas de la malla visual (.mdl, ambas en Z-arriba -> Y-arriba).
#include "../src/mdltris.hpp"
#include <array>
#include <cstdio>
static bool rd(const char* p, std::vector<uint8_t>& r) { FILE* f = std::fopen(p, "rb"); if (!f) return false; std::fseek(f, 0, SEEK_END); r.resize(std::ftell(f)); std::rewind(f); bool ok = std::fread(r.data(), 1, r.size(), f) == r.size(); std::fclose(f); return ok; }
int main(int argc, char** argv) {
    std::vector<uint8_t> a, m; Ground col, vis;
    if (argc < 3 || !rd(argv[1], a) || !rd(argv[2], m) || !col.load(a)) { std::fprintf(stderr, "uso: ground_validate nivel.col nivel.mdl [paso]\n"); return 1; }
    float step = argc > 3 ? (float)std::atof(argv[3]) : 100.f;
    std::vector<Ground::Tri> vt; if (!mdlTris(m, vt)) { std::fprintf(stderr, ".mdl no válido\n"); return 1; }
    vis.set(vt);
    V3 lo = col.boundsMin(), hi = col.boundsMax(); std::vector<float> err; std::vector<std::array<float, 4>> bad; size_t samples = 0, novis = 0;
    for (float z = lo.z; z <= hi.z; z += step) for (float x = lo.x; x <= hi.x; x += step) {
        auto g = col.groundQuery(x, hi.y + 1, z, 0.f, hi.y - lo.y + 2);
        if (!g.hit) continue; samples++;
        auto hv = vis.verticalHeights(x, z); if (hv.empty()) { novis++; continue; }
        float best = 1e30f; for (float h : hv) best = std::fmin(best, std::fabs(h - g.height));
        err.push_back(best); if (best > 1) bad.push_back({x, g.height, z, best});
    }
    std::sort(err.begin(), err.end()); double s = 0; for (float e : err) s += e;
    std::printf("muestras con suelo de colisión: %zu (sin malla visual debajo: %zu); distancia a la lámina visual más cercana: media %.2f u, p95 %.2f u, máx %.2f u; > 1 u: %zu\n",
                samples, novis, s / err.size(), err[err.size() * 95 / 100], err.back(), bad.size());
    std::sort(bad.begin(), bad.end(), [](auto& p, auto& q) { return p[3] > q[3]; });
    for (size_t i = 0; i < bad.size() && i < 8; i++) std::printf("  outlier x=%.0f y=%.0f z=%.0f (Y arriba) dist=%.0f\n", bad[i][0], bad[i][1], bad[i][2], bad[i][3]);
}
