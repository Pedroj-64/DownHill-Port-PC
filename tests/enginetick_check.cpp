// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Valida src/enginetick.hpp contra la referencia Python (tools/enginetick_export.py): enginetick_check vectores.bin malla.col [--assert]. Los vectores salen de grabaciones del usuario (fuera del repo, regla 4).
#include "../src/enginetick.hpp"
#include <cstdio>
#include <string>
using namespace etick;
static std::vector<uint8_t> slurp(const char* p) { std::vector<uint8_t> v; FILE* f = std::fopen(p, "rb"); if (!f) return v; std::fseek(f, 0, SEEK_END); v.resize(std::ftell(f)); std::fseek(f, 0, SEEK_SET); if (std::fread(v.data(), 1, v.size(), f) != v.size()) v.clear(); std::fclose(f); return v; }
struct Rd { const double* p; D3 v3() { D3 a{p[0], p[1], p[2]}; p += 3; return a; } double d() { return *p++; } M3 m3() { M3 m; for (int i = 0; i < 3; i++) m.r[i] = v3(); return m; } };
int main(int argc, char** argv) {
    if (argc < 3) { std::fprintf(stderr, "uso: enginetick_check vectores.bin malla.col [--assert]\n"); return 2; }
    bool as = argc > 3 && std::string(argv[3]) == "--assert";
    auto raw = slurp(argv[1]); Col col; if (!col.load(slurp(argv[2]))) { std::fprintf(stderr, "malla no válida\n"); return 2; }
    if (raw.size() < 4) return 2; uint32_t n; std::memcpy(&n, raw.data(), 4); const size_t rec = 124 * 8; if (raw.size() != 4 + n * rec) { std::fprintf(stderr, "tamaño de vectores inesperado\n"); return 2; }
    double worst = 0; int bad = 0, imp = 0, impBad = 0; double worstJ = 0;
    for (uint32_t i = 0; i < n; i++) {
        double buf[124]; std::memcpy(buf, raw.data() + 4 + i * rec, rec); Rd r{buf}; In in;
        in.S.im = r.d(); in.S.cml = r.v3(); in.S.cm = r.v3(); in.S.P = r.v3(); in.S.L = r.v3(); in.S.w = r.v3(); in.S.M = r.m3();
        for (int k = 0; k < 3; k++) in.S.loc[k] = r.v3();
        for (int k = 0; k < 2; k++) { in.S.su[k].e = r.d(); in.S.su[k].off = r.d(); in.S.su[k].s = r.d(); }
        in.E = r.v3(); in.Fq = r.v3(); in.ramp = r.d(); for (int k = 0; k < 32; k++) in.coefs[k] = r.d(); in.state[0] = (int)r.d(); in.state[1] = (int)r.d();
        for (int k = 0; k < 3; k++) in.rad[k] = r.d(); for (int k = 0; k < 3; k++) in.scl[k] = r.v3();
        D3 ocm = r.v3(), oP = r.v3(), oL = r.v3(); M3 oM = r.m3(); D3 ow = r.v3(); double os[6]; for (double& x : os) x = r.d(); double oj = r.d(); int on = (int)r.d(); r.d(); bool oerr = r.d() != 0;
        Out o = tick(in, col);
        double e = std::max({norm(o.cm - ocm), norm(o.P - oP), norm(o.L - oL), norm(o.w - ow)});
        for (int a = 0; a < 3; a++) e = std::max(e, norm(o.M.r[a] - oM.r[a]));
        e = std::max({e, std::fabs(o.su[0].e - os[0]), std::fabs(o.su[0].s - os[2]), std::fabs(o.su[1].e - os[3]), std::fabs(o.su[1].s - os[5])});
        double ej = std::fabs(o.j - oj); worstJ = std::max(worstJ, ej);
        bool isImp = on > 0 || o.nimp > 0; if (isImp) imp++;
        if (e > 1e-9 || ej > 1e-9 || o.nimp != on || o.err != oerr) { bad++; if (isImp) impBad++; if (bad <= 5) std::printf("tick %u: err %.3g, dj %.3g, nimp C++ %d / Py %d, err C++ %d / Py %d\n", i, e, ej, o.nimp, on, (int)o.err, (int)oerr); }
        worst = std::max(worst, e);
    }
    std::printf("%u ticks (%d con impacto); fuera de 1e-9: %d (%d con impacto); peor error de estado %.3g, peor |dj| %.3g\n", n, imp, bad, impBad, worst, worstJ);
    return as && bad ? 1 : 0;
}
