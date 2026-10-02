// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Demo sin ventana (datos reales, no va a CI): ride_demo nivel.col linea.pts nivel.gates [traza.pts [nivel.mdl]]
// Con nivel.mdl comprueba además que el centro de la esfera nunca cruza la malla visual (a doble cara): `cruces` = intentos de atravesar el suelo visible.
// Una esfera con gravedad (src/ride.hpp) recorre la línea de carrera de salida a meta sobre la malla de colisión. Informa de progreso, atascos y desviación respecto a la línea.
// linea.pts: f32 x,y,z en espacio NGP (Z arriba, tools/pts_path.py --chain); traza.pts (opcional): trayectoria en el mismo espacio, para DH_PTS de dhview.
#include "../src/gates.hpp"
#include "../src/mdltris.hpp"
#include "../src/ride.hpp"
#include <cstdio>
static bool rd(const char* p, std::vector<uint8_t>& r) { FILE* f = std::fopen(p, "rb"); if (!f) return false; std::fseek(f, 0, SEEK_END); r.resize(std::ftell(f)); std::rewind(f); bool ok = std::fread(r.data(), 1, r.size(), f) == r.size(); std::fclose(f); return ok; }
int main(int argc, char** argv) {
    std::vector<uint8_t> a, b, gr; Ground g; Gates gates; Gates::State gs;
    if (argc < 4 || !rd(argv[1], a) || !rd(argv[2], b) || !rd(argv[3], gr) || !g.load(a) || !gates.load(gr)) { std::fprintf(stderr, "uso: ride_demo nivel.col linea.pts nivel.gates [traza.pts]\n"); return 1; }
    const float* p = (const float*)b.data(); size_t n = b.size() / 12; std::vector<V3> line;
    for (size_t i = 0; i < n; i++) line.push_back({p[3*i], p[3*i+2], -p[3*i+1]});          // Z arriba -> Y arriba
    Ground vis; bool haveVis = false; if (argc > 5) { std::vector<uint8_t> m; std::vector<Ground::Tri> vt; if (rd(argv[5], m) && mdlTris(m, vt, true)) { vis.set(vt); haveVis = true; } }
    unsigned crossings = 0, decals = 0, deep = 0; V3 firstCross{}; double firstCrossT = -1;
    RideParams P; RideBody body; double tGround = 0, tAir = 0;
    // salida: primer punto de la línea con suelo debajo (la línea queda ~±12 u del suelo), centro de la esfera a 1 m por encima
    // DH_START (por defecto 3): índice de la línea donde empieza; la verja de salida está cerrada en la malla estática (superficie 0x681D, 0.3 u de solape con el punto 0), el juego la abre al empezar
    size_t s0 = std::getenv("DH_START") ? (size_t)std::atoi(std::getenv("DH_START")) : 3;
    auto gi = g.groundQuery(line[s0].x, line[s0].y + 10.f, line[s0].z, P.radius, 66.f); if (!gi.hit) { std::fprintf(stderr, "sin suelo en la salida\n"); return 2; }
    body.pos = {line[s0].x, gi.height + P.radius + 3.f, line[s0].z}; body.vel = {}; size_t idx = s0;
    const float dt = 1.f / 60.f; double t = 0; float maxDev = 0, maxDy = 0, progressT = 0; size_t bestIdx = 0; std::vector<float> trace; int stuck = 0; unsigned resets = 0;
    for (; t < 900; t += dt) {
        size_t lo = idx > 30 ? idx - 30 : 0, hi = std::min(n - 1, idx + 60); float bd = 1e30f; size_t bi = idx;
        for (size_t q = lo; q <= hi; q++) { float d = (line[q].x - body.pos.x) * (line[q].x - body.pos.x) + (line[q].z - body.pos.z) * (line[q].z - body.pos.z); if (d < bd) { bd = d; bi = q; } }
        idx = bi; size_t tq = std::min(n - 1, idx + 5);
        V3 prev = body.pos;
        body.step(g, P, line[tq], dt);
        if (haveVis) { SweepHit vh = vis.sweep(prev, body.pos, 0.f); if (vh.hit) { { auto hs0 = g.verticalHeights(body.pos.x, body.pos.z); float nb = -1e30f; for (float h : hs0) if (h <= vh.point.y + 0.5f) nb = std::fmax(nb, h); if (nb > -1e29f && vh.point.y - nb <= 20.f) decals++; else deep++; }
            if (!crossings++) { firstCross = body.pos; firstCrossT = t; }
            if (std::getenv("DH_DEBUG")) { auto hs = g.verticalHeights(body.pos.x, body.pos.z); std::sort(hs.begin(), hs.end()); std::printf("CRUCE t=%.2f prev=(%.0f %.0f %.0f) new=(%.0f %.0f %.0f) pt=(%.0f %.0f %.0f) frac=%.2f colHeights:", t, prev.x, prev.y, prev.z, body.pos.x, body.pos.y, body.pos.z, vh.point.x, vh.point.y, vh.point.z, vh.frac); for (float h : hs) std::printf(" %.0f", h); std::printf("\n"); } } } (body.grounded ? tGround : tAir) += dt;
        { int ev = gates.update(gs, body.pos, (float)t); if (ev) std::printf("  t=%6.1f s  puerta %zu %s  (punto de la línea %zu)\n", t, ev > 0 ? gs.counter : gs.counter + 1, ev > 0 ? "cruzada" : "retrocedida", idx); }
        if (gs.finished) { std::printf("META (plano 8052) cruzada en t=%.1f s tras %zu/%zu puertas\n", gs.finishTime, gs.counter, gates.size()); break; }
        float dev = std::sqrt(bd), dy = std::fabs(body.pos.y - line[idx].y); maxDev = std::fmax(maxDev, dev); if (dev < 100.f) maxDy = std::fmax(maxDy, dy);
        if (idx > bestIdx) { bestIdx = idx; progressT = (float)t; stuck = 0; } else if (++stuck > 60 * 8) {   // sin progreso 8 s: la demo no salta, así que coloca la esfera 3 puntos más adelante (como el reinicio al último punto bueno del modo previo de dhview)
            if (resets >= 20 || bestIdx + 3 >= n) { std::printf("ATASCADO en el punto %zu/%zu (t=%.1f s) pos=(%.0f %.0f %.0f) tras %u reinicios\n", idx, n, t, body.pos.x, body.pos.y, body.pos.z, resets); break; }
            size_t k = bestIdx + 3; auto g2 = g.groundQuery(line[k].x, line[k].y + 10.f, line[k].z, P.radius, 66.f); body.pos = {line[k].x, (g2.hit ? g2.height : line[k].y) + P.radius + 3.f, line[k].z}; body.vel = {}; resets++; stuck = 0; idx = k;
            std::printf("  reinicio %u en t=%.1f s -> punto %zu\n", resets, t, k); }
        if (std::getenv("DH_DEBUG") && (size_t)(t * 60) % 30 == 0) std::printf("t=%.1f idx=%zu pos=(%.0f %.0f %.0f) vel=(%.0f %.0f %.0f) dev=%.0f dy=%.0f gr=%d\n", t, idx, body.pos.x, body.pos.y, body.pos.z, body.vel.x, body.vel.y, body.vel.z, dev, body.pos.y - line[idx].y, (int)body.grounded);
        if ((size_t)(t * 60) % 6 == 0) { trace.push_back(body.pos.x); trace.push_back(-body.pos.z); trace.push_back(body.pos.y); }   // vuelta a Z arriba
        if (idx + 1 >= n && bd < 50.f * 50.f) { std::printf("META alcanzada en t=%.1f s\n", t); break; }
    }
    std::printf("línea: %zu puntos; progreso máximo %zu (%.0f%%) ; t=%.1f s\n", n, bestIdx, 100.0 * bestIdx / (n - 1), t);
    std::printf("barridos %u, impactos %u, solapes corregidos %u; máx desviación XZ a la línea %.0f u; máx |dy| respecto a la línea (a < 100 u): %.0f u\n", body.sweeps, body.hits, body.overlaps, maxDev, maxDy);
    if (haveVis) std::printf("cruces del centro con la malla visual (doble cara): %u pasos de %.0f: %u a <= 20 u sobre el suelo de colisión (capas decorativas flotantes), %u PROFUNDOS\n", crossings, t / dt, decals, deep); if (crossings) std::printf("  primero en t=%.1f s pos=(%.0f %.0f %.0f)\n", firstCrossT, firstCross.x, firstCross.y, firstCross.z);
    std::printf("reinicios de la demo (sin progreso 8 s): %u\n", resets);
    std::printf("tiempo en suelo %.0f%%, en el aire %.0f%%\n", 100 * tGround / (tGround + tAir), 100 * tAir / (tGround + tAir));
    if (argc > 4) { FILE* f = std::fopen(argv[4], "wb"); std::fwrite(trace.data(), 4, trace.size(), f); std::fclose(f); std::printf("traza: %zu puntos -> %s\n", trace.size() / 3, argv[4]); }
}
