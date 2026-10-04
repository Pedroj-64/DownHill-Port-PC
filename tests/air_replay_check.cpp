// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Oráculo de vuelo libre en C++: repite una captura (tools/air_dump_ref.py; números, fuera del repo) con engine::preStep (FUN_00134060) + integ::integrate (FUN_00238818).
// Predice cada tick desde el estado grabado del anterior y compara con el siguiente bajo los umbrales aprobados: 3 x suelo float32 (pos: ulp del mayor |pos|; R, P, L relativos: ulp(1.0)).
// Sin argumento o sin archivo: SKIP (sale 0). Uso: air_replay_check captura.txt
#include "../src/bike.hpp"
#include <cstdio>
#include <vector>
using namespace integ;
struct Rec { float pos[3], R[9], P[3], L[3]; };
int main(int argc, char** argv) {
    FILE* f = argc > 1 ? std::fopen(argv[1], "r") : nullptr;
    if (!f) { std::printf("air_replay_check: SKIP (sin captura; genera una con tools/air_dump_ref.py)\n"); return 0; }
    float invm, com[3], d[3]; if (std::fscanf(f, "%f %f %f %f %f %f %f", &invm, com, com + 1, com + 2, d, d + 1, d + 2) != 7) return 2;
    std::vector<Rec> T; Rec r; for (;;) { int n = 0; float* p = r.pos; for (; n < 18; n++) { if (n == 3) p = r.R; if (n == 12) p = r.P; if (n == 15) p = r.L; if (std::fscanf(f, "%f", p + (n < 3 ? n : n < 12 ? n - 3 : n < 15 ? n - 12 : n - 15)) != 1) break; } if (n < 18) break; T.push_back(r); }
    std::fclose(f); if (T.size() < 2) return 2;
    float maxPos = 0; for (const Rec& t : T) for (float v : t.pos) maxPos = std::fmax(maxPos, std::fabs(v));
    const float eps = std::nextafter(1.f, 2.f) - 1.f, ulpPos = std::nextafter(maxPos, 1e30f) - maxPos, thPos = 3 * ulpPos, thR = 3 * eps, thP = 3 * eps, thL = 3 * eps;
    auto make = [&](const Rec& t) { Body b; b.invMass = invm; b.com = {com[0], com[1], com[2], 0}; b.invInertia = {d[0], d[1], d[2], 0}; b.pos = {t.pos[0], t.pos[1], t.pos[2], 1};
        for (int i = 0; i < 3; i++) { at(b.nodeRot.r[i], 0) = t.R[3 * i]; at(b.nodeRot.r[i], 1) = t.R[3 * i + 1]; at(b.nodeRot.r[i], 2) = t.R[3 * i + 2]; }
        b.P = {t.P[0], t.P[1], t.P[2], 1}; b.L = {t.L[0], t.L[1], t.L[2], 1}; b.vel = {t.P[0] * invm, t.P[1] * invm, t.P[2] * invm, 1}; b.omega = {d[0] * t.L[0], d[1] * t.L[1], d[2] * t.L[2], 1}; return b; };
    // la masa del peso (módulo+0x110 = 100) = 1/invMass en las bicis medidas; se usa la del motor
    double mP = 0, mR = 0, mPo = 0, mL = 0; float maxP = 0, maxL = 0; for (const Rec& t : T) { for (float v : t.P) maxP = std::fmax(maxP, std::fabs(v)); for (float v : t.L) maxL = std::fmax(maxL, std::fabs(v)); }
    for (size_t k = 0; k + 1 < T.size(); k++) {
        Body b = make(T[k]); engine::preStep(b); integrate(b, 1.f / engine::kTickHz); const Rec& e = T[k + 1];
        for (int j = 0; j < 3; j++) { mPo = std::fmax(mPo, std::fabs(at(b.pos, j) - e.pos[j])); mP = std::fmax(mP, std::fabs(at(b.P, j) - e.P[j])); mL = std::fmax(mL, std::fabs(at(b.L, j) - e.L[j]));
            for (int i = 0; i < 3; i++) mR = std::fmax(mR, std::fabs(at(b.nodeRot.r[i], j) - e.R[3 * i + j])); }
    }
    mP /= maxP; mL /= maxL;
    std::printf("air_replay_check: %zu ticks\n  |dpos| %.3e <= %.3e (%.2f ulp)\n  |dR|   %.3e <= %.3e\n  relP   %.3e <= %.3e\n  relL   %.3e <= %.3e\n", T.size() - 1, mPo, thPos, mPo / ulpPos, mR, thR, mP, thP, mL, thL);
    bool ok = mPo <= thPos && mR <= thR && mP <= thP && mL <= thL; std::printf(ok ? "PASA\n" : "FALLA\n"); return ok ? 0 : 1;
}
