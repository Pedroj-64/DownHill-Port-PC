// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Oráculo del rozamiento lateral de rueda (src/wheelfric.hpp) contra una repetición exportada (tools/telemetry/export_replay.py, NF = 47; no va a CI: usa datos del juego).
// Uso: wheelfric_check archivo.replay    Para cada par de muestras consecutivas con acumulador presente (ctrl+0xE0 != 0) y contacto (|ctrl+0x250| > 1):
//   v_pre = v_i + E·dt/m;  v_pred = wheelfric::lateral(v_pre, n_{i+1}, fila0_{i+1}, ramp_i, coef0, ruedas_{i+1});  se compara con v_{i+1}.
// Criterio (PROPUESTA, no contrastado con docs/formats/): componente LATERAL del error |lat·(v_pred - v_{i+1})| <= 1 % de |lat·(v_{i+1} - v_pre)| + 0.002 u/s en >= 85 % de los ticks
// (el modelo solo afirma el eje lateral; los ejes normal y de avance se informan aparte).
#include "../src/wheelfric.hpp"
#include <cstdio>
#include <cstring>
#include <vector>
#include <algorithm>
constexpr int NF = 47;
struct Rec { float t; V3 v, gn, e0, x250, lat; float mass, ramp, w0, w1, coef0, k430; };
int main(int argc, char** argv) {
    if (argc < 2) { std::fprintf(stderr, "uso: wheelfric_check archivo.replay\n"); return 2; }
    FILE* f = std::fopen(argv[1], "rb"); if (!f) { std::fprintf(stderr, "no se puede abrir %s\n", argv[1]); return 2; }
    char hd[16]; uint32_t nf, n; if (std::fread(hd, 1, 16, f) != 16 || std::memcmp(hd, "DHRPL1\0\0", 8)) return 2; std::memcpy(&nf, hd + 8, 4); std::memcpy(&n, hd + 12, 4);
    if (nf != NF) { std::fprintf(stderr, "formato %u floats (se espera %d): reexporta con tools/telemetry/export_replay.py\n", nf, NF); return 2; }
    std::vector<Rec> R(n); float r[NF];
    for (uint32_t i = 0; i < n; i++) { if (std::fread(r, 4, NF, f) != NF) return 2; R[i] = {r[0], {r[4], r[5], r[6]}, {r[29], r[30], r[31]}, {r[32], r[33], r[34]}, {r[35], r[36], r[37]}, {r[40], r[41], r[42]}, r[39], r[43], r[44], r[45], r[46], r[38]}; }
    std::fclose(f);
    size_t tot = 0, ok = 0, okRamp = 0; std::vector<float> errs, eLat, eN, eF;
    for (size_t i = 0; i + 1 < R.size(); i++) {
        const Rec &a = R[i], &b = R[i + 1]; const float dt = b.t - a.t; if (dt < 0.015f || dt > 0.025f) continue;
        if (dot(b.e0, b.e0) == 0.f || dot(b.x250, b.x250) < 1.f || dot(b.gn, b.gn) < 0.25f) continue;
        const V3 vpre = a.v + b.e0 * (0.02f / b.mass); const int w = (int)(b.w0 + b.w1 + 0.5f);
        const V3 vp = wheelfric::lateral(vpre, b.gn, b.lat, std::fmin(a.ramp, 1.f), b.coef0, w);
        const V3 d = vp - b.v, J = b.v - vpre; const float e = std::sqrt(dot(d, d)), tol = 0.01f * std::sqrt(dot(J, J)) + 0.01f;
        { V3 nn = d; const float nl = std::sqrt(dot(b.gn, b.gn)); const V3 n = b.gn * (1.f / nl); V3 lt = b.lat - n * dot(b.lat, n); lt = lt * (1.f / std::sqrt(dot(lt, lt))); const V3 fw = cross(lt, n);
          eLat.push_back(std::fabs(dot(nn, lt))); eN.push_back(std::fabs(dot(nn, n))); eF.push_back(std::fabs(dot(nn, fw))); }
        const V3 nrm = b.gn * (1.f / std::sqrt(dot(b.gn, b.gn))); V3 lt = b.lat - nrm * dot(b.lat, nrm); lt = lt * (1.f / std::sqrt(dot(lt, lt)));
        const float eL = std::fabs(dot(d, lt)), tolL = 0.01f * std::fabs(dot(J, lt)) + 0.002f; (void)tol;
        tot++; ok += eL <= tolL; errs.push_back(e);
        okRamp += std::fabs(wheelfric::nextRamp(a.ramp, b.w0 + b.w1 > 0.f) - b.ramp) < 1e-3f || (a.ramp >= 1.f && b.ramp >= 1.f);
    }
    std::sort(errs.begin(), errs.end()); std::sort(eLat.begin(), eLat.end()); std::sort(eN.begin(), eN.end()); std::sort(eF.begin(), eF.end());
    if (tot) std::printf("  error por eje (p50/p90): lateral %.4f/%.4f  normal %.4f/%.4f  avance %.4f/%.4f u/s\n", eLat[tot / 2], eLat[tot * 9 / 10], eN[tot / 2], eN[tot * 9 / 10], eF[tot / 2], eF[tot * 9 / 10]);
    std::printf("%s: %zu ticks con contacto y acumulador; componente lateral dentro de tolerancia en %zu (%.1f %%); error mediana %.4f p90 %.4f u/s; rampa siguiente correcta %zu (%.1f %%)\n", argv[1], tot, ok, tot ? 100.0 * ok / tot : 0.0, tot ? errs[tot / 2] : 0.f, tot ? errs[tot * 9 / 10] : 0.f, okRamp, tot ? 100.0 * okRamp / tot : 0.0);
    return tot && ok * 100 >= tot * 85 ? 0 : 1;
}
