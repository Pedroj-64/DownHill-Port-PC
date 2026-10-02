// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Demo sin ventana (datos reales, no va a CI): bike_demo nivel.col linea.pts nivel.gates nivel.start.pts [nivel.mdl [traza.pts]]
// La moto de src/bike.hpp con un piloto automático de PRUEBAS (acelera, gira hacia el punto de la línea 5 por delante, frena en bajada fuerte, endereza el cabeceo en el aire) recorre el nivel
// de la rejilla de salida a la meta (plano 8052). Con nivel.mdl cuenta los cruces de la rueda delantera con la malla visual (a doble cara), clasificados como en ride_demo.cpp.
// linea.pts: f32 x,y,z en espacio NGP (Z arriba, tools/pts_path.py --chain); traza.pts (opcional): trayectoria en el mismo espacio para DH_PTS de dhview.
#include "../src/bike.hpp"
#include "../src/mdltris.hpp"
#include <cstdio>
static bool rd(const char* p, std::vector<uint8_t>& r) { FILE* f = std::fopen(p, "rb"); if (!f) return false; std::fseek(f, 0, SEEK_END); r.resize(std::ftell(f)); std::rewind(f); bool ok = std::fread(r.data(), 1, r.size(), f) == r.size(); std::fclose(f); return ok; }
static float wrap(float a) { while (a > 3.14159f) a -= 6.28318f; while (a < -3.14159f) a += 6.28318f; return a; }
// Normal del suelo donde aterrizará la moto si sigue la trayectoria balística actual (gravedad + velocidad), o nullptr-equivalente (hit=false). Piloto automático de PRUEBAS: no es lógica del juego.
static SweepHit predictLanding(const Ground& g, const Bike& b) {
    V3 p = b.pos, v = b.vel(); const float h = 0.05f;
    for (int i = 0; i < 100; i++) {
        V3 v2 = v; v2.y -= b.P.gravity * h; V3 q = p + (v + v2) * (0.5f * h);
        SweepHit s = g.sweep(p, q, b.P.radius); if (s.hit) return s; p = q; v = v2;
    }
    return SweepHit();
}
int main(int argc, char** argv) {
    std::vector<uint8_t> a, b, gr, sp; Ground g; Gates gates;
    if (argc < 5 || !rd(argv[1], a) || !rd(argv[2], b) || !rd(argv[3], gr) || !rd(argv[4], sp) || !g.load(a) || !gates.load(gr) || sp.size() < 12) { std::fprintf(stderr, "uso: bike_demo nivel.col linea.pts nivel.gates nivel.start.pts [nivel.mdl [traza.pts]]\n"); return 1; }
    openStartGate(g);
    const float* p = (const float*)b.data(); size_t n = b.size() / 12; std::vector<V3> line; for (size_t i = 0; i < n; i++) line.push_back({p[3*i], p[3*i+2], -p[3*i+1]});   // Z arriba -> Y arriba
    for (size_t i = 1; i < line.size(); i++) { float dx = line[i].x - line[i-1].x, dz = line[i].z - line[i-1].z; if (dx * dx + dz * dz > 400.f * 400.f) {   // la cadena .PTS acaba y siguen puntos de otra zona (ALP2: tras el 438): se corta ahí y se prolonga hacia delante para cruzar el plano de meta
        V3 a = line[i-1], b = line[i-2]; V3 d = unit(V3{a.x - b.x, 0, a.z - b.z}); line.resize(i); line.push_back(V3{a.x + d.x * 150.f, a.y - 20.f, a.z + d.z * 150.f}); n = line.size(); break; } }
    const float* s0 = (const float*)sp.data(); V3 start{s0[0], s0[2], -s0[1]};
    Ground vis; bool haveVis = false; if (argc > 5) { std::vector<uint8_t> m; std::vector<Ground::Tri> vt; if (rd(argv[5], m) && mdlTris(m, vt, true)) { vis.set(vt); haveVis = true; } }
    Run run; run.gates = &gates; const Gate* g0 = gates.courseGate(0); float h0 = g0 ? std::atan2(g0->n.x, -g0->n.z) : 0.f;
    run.start(g, start, h0); Bike& bike = run.bike;
    const float dt = 1.f / 60.f; size_t idx = 0, bestIdx = 0; for (size_t q = 0; q < n; q++) if ((line[q].x - start.x) * (line[q].x - start.x) + (line[q].z - start.z) * (line[q].z - start.z) < (line[idx].x - start.x) * (line[idx].x - start.x) + (line[idx].z - start.z) * (line[idx].z - start.z)) idx = q;
    bestIdx = idx; std::printf("salida (%.0f %.0f %.0f) rumbo %.2f rad, punto de la línea %zu de %zu\n", start.x, start.y, start.z, h0, idx, n);
    // Piloto de PRUEBAS que 'aprende' como un jugador: la velocidad de crucero depende de la zona (tramos de 20 puntos de la línea) y, al atascarse, esa zona y las dos anteriores prueban la siguiente velocidad de la tabla. DH_CRUISE=v fija una velocidad única.
    static const float cruiseTab[6] = {80.f, 60.f, 100.f, 45.f, 70.f, 90.f}; std::vector<unsigned> tries(n / 20 + 1, 0);
    FILE* trace6 = std::getenv("DH_TRACE6") ? std::fopen(std::getenv("DH_TRACE6"), "wb") : nullptr;
    unsigned decals = 0, deep = 0, crossings = 0; double progressT = 0, tGround = 0, tAir = 0; float maxSpeed = 0; unsigned stuckResets = 0; std::vector<float> trace; V3 firstDeep{}; double firstDeepT = -1;
    for (; run.t < 900;) {
        size_t lo = idx > 30 ? idx - 30 : 0, hi = std::min(n - 1, idx + 60); float bd = 1e30f; size_t bi = idx;
        for (size_t q = lo; q <= hi; q++) { float d = (line[q].x - bike.pos.x) * (line[q].x - bike.pos.x) + (line[q].z - bike.pos.z) * (line[q].z - bike.pos.z); if (d < bd) { bd = d; bi = q; } }
        idx = bi; static const float look = std::getenv("DH_LOOK") ? (float)std::atof(std::getenv("DH_LOOK")) : 25.f;   // lookahead por DISTANCIA (no por índice): el tramo 187->188 de ALP2 mide 116 u y apuntar 5 puntos más allá hace salir del borde por otro sitio
        size_t ti = std::min(n - 1, idx + 1); while (ti + 1 < n && (line[ti].x - bike.pos.x) * (line[ti].x - bike.pos.x) + (line[ti].z - bike.pos.z) * (line[ti].z - bike.pos.z) < look * look) ti++; V3 tg = line[ti];
        BikeInput in; float want = std::atan2(tg.x - bike.pos.x, -(tg.z - bike.pos.z)); in.steer = std::fmax(-1.f, std::fmin(1.f, 2.f * wrap(want - bike.heading()))); in.throttle = 1.f;
        float sp2 = bike.speed(); static const float cruiseEnv = std::getenv("DH_CRUISE") ? (float)std::atof(std::getenv("DH_CRUISE")) : 0.f; float cruise0 = cruiseEnv > 0 ? cruiseEnv : cruiseTab[tries[std::min(tries.size() - 1, idx / 20)] % 6]; static const float dropV = std::getenv("DH_DROPV") ? (float)std::atof(std::getenv("DH_DROPV")) : 1e9f;   // DH_DROPV=u/s: velocidad de entrada a caídas fuertes (por defecto sin límite: las caídas de la línea son saltos que necesitan velocidad)
        float cruise = cruise0; { float dz = line[idx].y - line[std::min(n - 1, idx + 8)].y; if (dz > 40.f) cruise = std::fmin(cruise0, dropV); }   // caída fuerte por delante en la línea: entra despacio
        if (sp2 > cruise) in.brake = std::fmin(1.f, (sp2 - cruise) / 10.f);                            // frena en bajada fuerte
        if (!bike.grounded) {                                                   // en el aire: cabeceo paralelo a la pendiente donde va a aterrizar (trayectoria balística); sin impacto previsto, con la trayectoria
            V3 v = bike.vel(); float hz = std::sqrt(v.x * v.x + v.z * v.z), want_p = std::atan2(v.y, hz), cur = std::asin(std::fmax(-1.f, std::fmin(1.f, bike.fwd.y)));
            SweepHit gq = predictLanding(g, bike);
            if (gq.hit && gq.normal.y > 0.2f) { V3 fh = unit(V3{bike.fwd.x, 0, bike.fwd.z}); V3 t = fh - gq.normal * dot(fh, gq.normal); if (dot(t, t) > 1e-6f) want_p = std::asin(std::fmax(-1.f, std::fmin(1.f, unit(t).y))); }
            float wp = dot(bike.rb.omega, bike.axes().r);
            in.lean = std::fmax(-1.f, std::fmin(1.f, 3.f * (want_p - cur) - 0.5f * wp));
        }
        V3 wf0 = bike.point(0, bike.axes(), bike.pos);
        int ev = run.update(g, in, dt);
        V3 wf1 = bike.point(0, bike.axes(), bike.pos);
        if (haveVis) { SweepHit vh = vis.sweep(wf0, wf1, 0.f); if (vh.hit) { auto hs = g.verticalHeights(wf1.x, wf1.z); float nb = -1e30f; for (float h : hs) if (h <= vh.point.y + 0.5f) nb = std::fmax(nb, h); crossings++;
            if (nb > -1e29f && vh.point.y - nb <= 20.f) decals++; else { deep++; if (firstDeepT < 0) { firstDeep = wf1; firstDeepT = run.t; } } } }
        if (std::getenv("DH_DEBUG") && (size_t)(run.t * 60) % 30 == 0 && run.t > (std::getenv("DH_T0") ? std::atof(std::getenv("DH_T0")) : 0.0)) std::printf("t=%.1f idx=%zu pos=(%.0f %.0f %.0f) vel=(%.0f %.0f %.0f) |v|=%.0f fwd=(%.2f %.2f %.2f) gr=%d F=%d R=%d steer=%.2f want=%.2f head=%.2f\n", run.t, idx, bike.pos.x, bike.pos.y, bike.pos.z, bike.vel().x, bike.vel().y, bike.vel().z, bike.speed(), bike.fwd.x, bike.fwd.y, bike.fwd.z, bike.grounded, bike.wheelF, bike.wheelR, in.steer, want, bike.heading());
        if (ev) std::printf("  t=%6.1f s  puerta %zu %s  (punto %zu, %.0f u/s)\n", run.t, ev > 0 ? run.gs.counter : run.gs.counter + 1, ev > 0 ? "cruzada" : "retrocedida", idx, bike.speed());
        if (run.gs.finished) { std::printf("META (última puerta) en t=%.1f s tras %zu/%zu puertas\n", run.gs.finishTime, run.gs.counter, gates.size()); break; }
        maxSpeed = std::fmax(maxSpeed, bike.speed()); (bike.grounded ? tGround : tAir) += dt;
        if (idx > bestIdx + 1) { bestIdx = idx; progressT = run.t; } else if (run.t - progressT > 8.0) {                      // sin progreso 8 s: el jugador reiniciaría desde el último punto bueno
            if (stuckResets >= 60) { std::printf("ATASCADO en el punto %zu/%zu (t=%.1f s) pos=(%.0f %.0f %.0f) tras %u reinicios\n", idx, n, run.t, bike.pos.x, bike.pos.y, bike.pos.z, stuckResets); break; }
            std::printf("  reinicio %u en t=%.1f s (punto %zu, pos %.0f %.0f %.0f)\n", stuckResets + 1, run.t, idx, bike.pos.x, bike.pos.y, bike.pos.z); { size_t b = idx / 20; for (size_t k = 0; k < 3 && k <= b; k++) tries[b - k]++; } run.respawn(); stuckResets++; progressT = run.t; }
        if (trace6 && (size_t)(run.t * 60) % 6 == 0) { float f6[6] = {bike.pos.x, bike.pos.y, bike.pos.z, bike.fwd.x, bike.fwd.y, bike.fwd.z}; std::fwrite(f6, 4, 6, trace6); }   // DH_TRACE6=archivo: muestras para DH_REPLAY de dhview
        if ((size_t)(run.t * 60) % 6 == 0) { trace.push_back(bike.pos.x); trace.push_back(-bike.pos.z); trace.push_back(bike.pos.y); }
    }
    std::printf("línea: %zu puntos; progreso máximo %zu (%.0f%%); t=%.1f s; velocidad máx %.0f u/s (%.0f km/h)\n", n, bestIdx, 100.0 * bestIdx / (n - 1), run.t, maxSpeed, maxSpeed * 1.0973f);
    std::printf("barridos %u, impactos %u, solapes corregidos %u; reinicios del reglamento (caída/atasco) %u + %u por atasco\n", bike.sweeps, bike.hits, bike.overlaps, run.respawns - stuckResets, stuckResets);
    if (haveVis) std::printf("cruces de la rueda delantera con la malla visual (doble cara): %u de %.0f pasos: %u a <= 20 u sobre el suelo de colisión (capas decorativas), %u PROFUNDOS%s\n", crossings, run.t / dt, decals, deep, firstDeepT >= 0 ? "" : "");
    if (firstDeepT >= 0) std::printf("  primer cruce profundo en t=%.1f s pos=(%.0f %.0f %.0f)\n", firstDeepT, firstDeep.x, firstDeep.y, firstDeep.z);
    std::printf("tiempo en suelo %.0f%%, en el aire %.0f%%\n", 100 * tGround / (tGround + tAir), 100 * tAir / (tGround + tAir));
    if (trace6) std::fclose(trace6);
    if (argc > 6) { FILE* f = std::fopen(argv[6], "wb"); std::fwrite(trace.data(), 4, trace.size(), f); std::fclose(f); std::printf("traza: %zu puntos -> %s\n", trace.size() / 3, argv[6]); }
    return 0;
}
