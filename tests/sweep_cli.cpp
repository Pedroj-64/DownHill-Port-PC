// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// sweep_cli nivel.col < segmentos.txt : una línea "x0 y0 z0 x1 y1 z1 r" por barrido, en el espacio del juego (Z arriba, el de los registros de PCSX2).
// Salida por línea: "HIT frac px py pz nx ny nz surface tri" o "NOHIT". Lo usa tools/pcsx2/compare_trace.py.
#include "../src/ground.hpp"
#include <cstdio>
static V3 toY(V3 a) { return {a.x, a.z, -a.y}; }       // Z arriba -> Y arriba
static V3 toZ(V3 a) { return {a.x, -a.z, a.y}; }       // Y arriba -> Z arriba
int main(int argc, char** argv) {
    if (argc < 2) { std::fprintf(stderr, "uso: sweep_cli nivel.col < segmentos\n"); return 1; }
    FILE* f = std::fopen(argv[1], "rb"); if (!f) return 1; std::vector<uint8_t> raw; std::fseek(f, 0, SEEK_END); raw.resize(std::ftell(f)); std::rewind(f);
    if (std::fread(raw.data(), 1, raw.size(), f) != raw.size()) return 1; std::fclose(f); Ground g; if (!g.load(raw)) return 1;
    float v[7];
    while (std::scanf("%f %f %f %f %f %f %f", v, v + 1, v + 2, v + 3, v + 4, v + 5, v + 6) == 7) {
        SweepHit h = g.sweep(toY({v[0], v[1], v[2]}), toY({v[3], v[4], v[5]}), v[6]);
        if (!h.hit) { std::printf("NOHIT\n"); continue; }
        V3 p = toZ(h.point), n = toZ(h.normal);
        std::printf("HIT %g %g %g %g %g %g %g %u %u\n", h.frac, p.x, p.y, p.z, n.x, n.y, n.z, (unsigned)h.surface, h.tri);
    }
}
