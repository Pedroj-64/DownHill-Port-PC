// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Oráculo de impacto: repite UN paso físico completo del motor (FUN_00133FA8: FUN_00134060 + FUN_001340D8) con el port: engine::preStep, integ::tick (FUN_00238818 + bucle de sub-pasos),
// barrido de los puntos de contacto contra la malla del nivel (Ground::sweepHits, FUN_001348A0 para los puntos), despenetración y contactResponse (FUN_00134630). Predice cada tick desde
// el estado grabado del anterior y lo compara con el siguiente. Datos: nivel.col (datos del juego, fuera del repo) y un volcado numérico de tools/impact_dump_ref.py.
// Uso: impact_replay_check nivel.col volcado.txt [tick_inicial] [tick_final]   (sin argumentos: SKIP). Informa los residuos por tick; no fija umbrales posteriores al contacto.
#include "../src/bike.hpp"
#include <cstdio>
#include <vector>
using namespace integ;
struct Rec { float node[3], R[9], pos[3], P[3], L[3]; };
static bool rd(const char* p, std::vector<uint8_t>& r) { FILE* f = std::fopen(p, "rb"); if (!f) return false; std::fseek(f, 0, SEEK_END); r.resize(std::ftell(f)); std::rewind(f); bool ok = std::fread(r.data(), 1, r.size(), f) == r.size(); std::fclose(f); return ok; }
static V3 toYup(const V4& v) { return {v.x, v.z, -v.y}; }                 // espacio del juego (z arriba) -> Ground (y arriba), como bike_demo
static V4 fromYup(V3 v, float w = 0) { return {v.x, -v.z, v.y, w}; }
int main(int argc, char** argv) {
    if (argc < 3) { std::printf("impact_replay_check: SKIP\n"); return 0; }
    std::vector<uint8_t> colb; Ground g; if (!rd(argv[1], colb) || !g.load(colb)) { std::fprintf(stderr, "no se puede cargar %s\n", argv[1]); return 2; }
    FILE* f = std::fopen(argv[2], "r"); if (!f) return 2;
    float invm, com[3], d[3], e, mu; int np; if (std::fscanf(f, "%f %f %f %f %f %f %f %f %f %d", &invm, com, com + 1, com + 2, d, d + 1, d + 2, &e, &mu, &np) != 10) return 2;
    std::vector<V4> loc(np); std::vector<float> rad(np); for (int i = 0; i < np; i++) { float a, b, c, r; if (std::fscanf(f, "%f %f %f %f", &a, &b, &c, &r) != 4) return 2; loc[i] = {a, b, c, 0}; rad[i] = r; }
    std::vector<Rec> T; for (;;) { Rec r; float* q = r.node; int n = 0; for (; n < 21; n++) { float* p = n < 3 ? r.node + n : n < 12 ? r.R + n - 3 : n < 15 ? r.pos + n - 12 : n < 18 ? r.P + n - 15 : r.L + n - 18; if (std::fscanf(f, "%f", p) != 1) break; } (void)q; if (n < 21) break; T.push_back(r); }
    std::fclose(f); size_t k0 = argc > 3 ? std::atoi(argv[3]) : 0, k1 = argc > 4 ? std::atoi(argv[4]) : T.size() - 2;
    auto make = [&](const Rec& t) { Body b; b.invMass = invm; b.com = {com[0], com[1], com[2], 0}; b.invInertia = {d[0], d[1], d[2], 0}; b.pos = {t.pos[0], t.pos[1], t.pos[2], 1}; b.nodePos = {t.node[0], t.node[1], t.node[2], 1};
        for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) at(b.nodeRot.r[i], j) = t.R[3 * i + j];
        b.P = {t.P[0], t.P[1], t.P[2], 1}; b.L = {t.L[0], t.L[1], t.L[2], 1}; b.vel = {t.P[0] * invm, t.P[1] * invm, t.P[2] * invm, 1}; b.omega = {d[0] * t.L[0], d[1] * t.L[1], d[2] * t.L[2], 1};
        M4 sp = identity(); for (int i = 0; i < 3; i++) at(sp.r[i], i) = d[i]; b.iw = sp; return b; };   // iw = diag(invInertia): medido (integrator.md)
    auto worldPts = [&](const Body& b, std::vector<V3>& out) { out.clear(); for (int i = 0; i < np; i++) {   // FUN_001348A0: world = nodePos + local * R (filas = ejes)
        V4 w; for (int j = 0; j < 3; j++) at(w, j) = (at(loc[i], 0) * at(b.nodeRot.r[0], j) + at(loc[i], 1) * at(b.nodeRot.r[1], j)) + at(loc[i], 2) * at(b.nodeRot.r[2], j);
        w.x += b.nodePos.x; w.y += b.nodePos.y; w.z += b.nodePos.z; out.push_back(toYup(w)); } };
    std::printf("tick | contactos(inicio) iter hits | dpos  dnodo  dR  relP  relL | respuestas |J|\n");
    for (size_t k = k0; k <= k1 && k + 1 < T.size(); k++) {
        Body b = make(T[k]); engine::preStep(b); std::vector<std::vector<SweepHit>> L; std::vector<SweepHit> side; std::vector<float> jmag; int nhit = 0;
        auto sweep = [&](const Body& s, const Body& en) { std::vector<V3> p0, p1; worldPts(s, p0); worldPts(en, p1); L.assign(np, {}); for (int i = 0; i < np; i++) L[i] = g.sweepHits(p0[i], p1[i], rad[i]);
            if (std::getenv("DH_DEBUG")) for (int i = 0; i < np; i++) { std::printf("\n   sweep pt%d (%.1f %.1f %.1f)->(%.1f %.1f %.1f) r=%.2f:", i, p0[i].x, p0[i].y, p0[i].z, p1[i].x, p1[i].y, p1[i].z, rad[i]); for (const SweepHit& q : L[i]) std::printf(" [frac %.3f tri %u n=(%.2f %.2f %.2f) pen %.2f]", q.frac, q.tri, q.normal.x, q.normal.y, q.normal.z, q.pen); }
            std::vector<Hit> hs; side.clear(); HitRef r = nextHit(L); while (r.valid()) { SweepHit sh = L[r.point][r.index]; hs.push_back(Hit{sh.frac, r.point}); side.push_back(sh); r = nextHit(L, r); } nhit += (int)hs.size(); return hs; };
        auto depen = [&](Body& bb, const std::vector<Hit>&) { Depenetration dp = depenetration(L); if (dp.apply) translate(bb, fromYup(dp.move)); };   // FUN_001344F0: solo si |mover| <= 1
        auto respond = [&](Body& bb, const Hit& h) {
            const SweepHit* sh = nullptr; for (const SweepHit& c : side) if (c.frac == h.frac) { sh = &c; break; } if (!sh) return false;
            RigidBody rb; rb.invMass = bb.invMass; rb.com = {bb.pos.x, bb.pos.y, bb.pos.z}; rb.linMom = {bb.P.x, bb.P.y, bb.P.z}; rb.angMom = {bb.L.x, bb.L.y, bb.L.z}; rb.vel = {bb.vel.x, bb.vel.y, bb.vel.z}; rb.omega = {bb.omega.x, bb.omega.y, bb.omega.z};
            for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) rb.iInv[3 * i + j] = at(bb.iw.r[i], j); rb.restitution = std::getenv("DH_E") ? (float)std::atof(std::getenv("DH_E")) : e; rb.friction = std::getenv("DH_MU") ? (float)std::atof(std::getenv("DH_MU")) : mu;
            V4 pt = fromYup(sh->point), nr = fromYup(sh->normal); V3 pg{pt.x, pt.y, pt.z}, ng{nr.x, nr.y, nr.z}, ax{bb.nodeRot.r[0].x, bb.nodeRot.r[0].y, bb.nodeRot.r[0].z};
            if (std::getenv("DH_DEBUG")) { V3 vr = contactVelocity(rb, pg, {}); std::printf("\n   hit pt#%d frac=%.4f normal=(%.3f %.3f %.3f) point=(%.1f %.1f %.1f) pen=%.3f tri=%u surf=%u | v=(%.1f %.1f %.1f) vn=%.1f | P(antes)=(%.0f %.0f %.0f)", h.point, h.frac, ng.x, ng.y, ng.z, pg.x, pg.y, pg.z, sh->pen, sh->tri, sh->surface, rb.vel.x, rb.vel.y, rb.vel.z, dot(ng, vr), rb.linMom.x, rb.linMom.y, rb.linMom.z); }
            jmag.push_back(contactResponse(rb, pg, ng, ax, h.point));
            bb.P = {rb.linMom.x, rb.linMom.y, rb.linMom.z, 1}; bb.L = {rb.angMom.x, rb.angMom.y, rb.angMom.z, 1}; bb.vel = {rb.vel.x, rb.vel.y, rb.vel.z, 1}; bb.omega = {rb.omega.x, rb.omega.y, rb.omega.z, 1}; return true; };
        Body b0 = b; int it = tick(b, 1.f / engine::kTickHz, sweep, depen, respond); const Rec& t = T[k + 1];
        double dpos = 0, dnode = 0, dR = 0, dP = 0, dL = 0, mP = 1e-9, mL = 1e-9; for (int j = 0; j < 3; j++) { dpos = std::fmax(dpos, std::fabs(at(b.pos, j) - t.pos[j])); dnode = std::fmax(dnode, std::fabs(at(b.nodePos, j) - t.node[j]));
            dP = std::fmax(dP, std::fabs(at(b.P, j) - t.P[j])); dL = std::fmax(dL, std::fabs(at(b.L, j) - t.L[j])); mP = std::fmax(mP, std::fabs(t.P[j])); mL = std::fmax(mL, std::fabs(t.L[j]));
            for (int i = 0; i < 3; i++) dR = std::fmax(dR, std::fabs(at(b.nodeRot.r[i], j) - t.R[3 * i + j])); }
        if (std::getenv("DH_DEBUG")) std::printf("\n   pos real-modelo=(%.3f %.3f %.3f) vel modelo=(%.1f %.1f %.1f) real=(%.1f %.1f %.1f)", t.pos[0] - b.pos.x, t.pos[1] - b.pos.y, t.pos[2] - b.pos.z, b.vel.x, b.vel.y, b.vel.z, t.P[0] * invm, t.P[1] * invm, t.P[2] * invm);
        if (std::getenv("DH_DEBUG")) std::printf("\n   P real(k)=(%.0f %.0f %.0f) real(k+1)=(%.0f %.0f %.0f) modelo=(%.0f %.0f %.0f) L real(k+1)=(%.0f %.0f %.0f) modelo=(%.0f %.0f %.0f)\n", T[k].P[0], T[k].P[1], T[k].P[2], t.P[0], t.P[1], t.P[2], b.P.x, b.P.y, b.P.z, t.L[0], t.L[1], t.L[2], b.L.x, b.L.y, b.L.z);
        std::printf("%4zu | it=%d hits=%d | %.2e %.2e %.2e %.2e %.2e |", k, it, nhit, dpos, dnode, dR, dP / mP, dL / mL); for (float j : jmag) std::printf(" %.1f", j); std::printf("\n");
    }
    return 0;
}
