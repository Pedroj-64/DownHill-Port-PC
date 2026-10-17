// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Oráculo por repetición del port (hito M1; no va a CI, usa datos del juego): replay_demo nivel.col archivo.replay [ventana_s=2] [paso_s=1]
// Alimenta a la moto de src/bike.hpp con las ENTRADAS grabadas del juego (tools/telemetry/export_replay.py) a 50 Hz, parte del estado grabado en el inicio de cada ventana
// y compara la posición/velocidad al cabo de 0.5 s, 1 s y la ventana completa con las grabadas. Línea base: velocidad constante (sin modelo).
// Env: DH_NOENGINE=1 usa el giro del arnés en vez del controlador del motor; DH_LEAN=-1 invierte el cabeceo; DH_SHOWWORST=n imprime las n peores ventanas.
#include "../src/bike.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
static bool rd(const char* p, std::vector<uint8_t>& r) { FILE* f = std::fopen(p, "rb"); if (!f) return false; std::fseek(f, 0, SEEK_END); r.resize(std::ftell(f)); std::rewind(f); bool ok = std::fread(r.data(), 1, r.size(), f) == r.size(); std::fclose(f); return ok; }

constexpr int NF = 32;
struct Rec { float t; V3 p, v, f, u; float steer, lean, pedal, charge, release, air, surf, trick, sprint, b796, roll, kDrag, vtermP, sprintT, sprintIdx, rflags; V3 gn; };
static Rec parse(const float* r) { return {r[0], {r[1], r[2], r[3]}, {r[4], r[5], r[6]}, {r[7], r[8], r[9]}, {r[10], r[11], r[12]}, r[13], r[14], r[15], r[16], r[17], r[18], r[19], r[20], r[21], r[22], r[23], r[24], r[25], r[26], r[27], r[28], {r[29], r[30], r[31]}}; }
static float dist(V3 a, V3 b) { V3 d = a - b; return std::sqrt(dot(d, d)); }
static float pct(std::vector<float> v, float q) { if (v.empty()) return 0; std::sort(v.begin(), v.end()); return v[std::min(v.size() - 1, (size_t)(q * (v.size() - 1) + 0.5f))]; }

static std::string gAutoRst;   // unpacked/RST/<nivel>.RST derivado del nombre del .col (DH_WHEELFRIC=1)
// DH_SET="groundShare=0.5,grip=10": sustituye parámetros de BikeParams (ajuste empírico con el oráculo; los nombres son los del struct)
static void applySet(BikeParams& P) {
    if (const char* rst = std::getenv("DH_WHEELFRIC")) {   // DH_WHEELFRIC=<nivel>.RST: rozamiento de rueda del motor con la tabla del nivel (RST+0xCA4, 32x8 f32)
        if (std::string(rst) == "1") rst = gAutoRst.c_str();
        FILE* f = std::fopen(rst, "rb"); float t[32][8];
        if (f && !std::fseek(f, 0xCA4, SEEK_SET) && std::fread(t, sizeof t, 1, f) == 1) { P.wheelFric = true; for (int c = 0; c < 32; c++) P.fricCoef[c] = t[c][0]; } else std::fprintf(stderr, "DH_WHEELFRIC: no se puede leer %s\n", rst);
        if (f) std::fclose(f); }
    if (std::getenv("DH_REAL")) { P.groundShare = 1.f; P.rollConst = 0.f; P.pedalMax = 1e9f; P.realDrag = true; P.pedalKick = 0.1f; P.pedalKickV = 29.333f; }   // T6: fuerzas de suelo del motor (peso, arrastre g/vterm², rodadura por clase vía DH_ROLLTAB=1, pedal constante), sin términos empíricos
    const char* e = std::getenv("DH_SET"); if (!e) return; std::string str(e); size_t a = 0;
    while (a < str.size()) { size_t b = str.find(',', a); if (b == std::string::npos) b = str.size(); std::string kv = str.substr(a, b - a); a = b + 1; size_t q = kv.find('='); if (q == std::string::npos) continue;
        std::string k = kv.substr(0, q); float v = (float)std::atof(kv.c_str() + q + 1);
        if (k == "groundShare") P.groundShare = v; else if (k == "rolling") P.rolling = v; else if (k == "rollConst") P.rollConst = v; else if (k == "dragK") P.dragK = v; else if (k == "dragKAir") P.dragKAir = v;
        else if (k == "grip") P.grip = v; else if (k == "pedalAccel") P.pedalAccel = v; else if (k == "pedalMax") P.pedalMax = v; else if (k == "comLift") P.comLift = v; else if (k == "brakeDecel") P.brakeDecel = v;
        else if (k == "gravityGround") P.gravityGround = v; else if (k == "gravity") P.gravity = v; else if (k == "pedalKick") P.pedalKick = v; else if (k == "pedalKickV") P.pedalKickV = v; else if (k == "sprintMul") P.sprintMul = v; else if (k == "steerAccelGround") P.steerAccelGround = v; else if (k == "steerMaxGround") P.steerMaxGround = v; else if (k == "restitution") P.restitution = v; else if (k == "friction") P.friction = v;
        else if (k == "realDrag") P.realDrag = v != 0.f; else if (k == "vterm0") P.vterm0 = v; else if (k == "sprintRamp") P.sprintRamp = v;
        else if (k == "wheelFricLong") P.wheelFricLong = v != 0.f; else if (k == "fricMaxK") P.fricMaxK = v; else if (k == "fricGate") P.fricGate = (int)v; else if (k == "fricApps") P.fricApps = (int)v; else if (k == "fricE") P.fricE = v;
        else std::fprintf(stderr, "DH_SET: parámetro desconocido %s\n", k.c_str()); }
}

struct Win { size_t i; float e05, e10, eW, b05, b10, bW, ev, evB; bool allGround; bool g05; float v05; float al, la; bool hasLean; float pitchErr; };

int main(int argc, char** argv) {
    std::vector<uint8_t> col, raw; Ground g;
    if (argc > 1) { std::string a1 = argv[1]; size_t sl = a1.find_last_of('/'); size_t dt = a1.rfind('.'); gAutoRst = "unpacked/RST/" + a1.substr(sl == std::string::npos ? 0 : sl + 1, dt - (sl == std::string::npos ? 0 : sl + 1)) + ".RST"; }
    if (argc < 3 || !rd(argv[1], col) || !rd(argv[2], raw) || !g.load(col) || raw.size() < 16 || std::memcmp(raw.data(), "DHRPL1\0\0", 8) != 0) { std::fprintf(stderr, "uso: replay_demo nivel.col archivo.replay [ventana_s=2] [paso_s=1]\n"); return 1; }
    uint32_t nf, n; std::memcpy(&nf, raw.data() + 8, 4); std::memcpy(&n, raw.data() + 12, 4);
    if (nf != NF || raw.size() < 16 + (size_t)n * NF * 4) { std::fprintf(stderr, "formato inesperado\n"); return 1; }
    std::vector<Rec> R(n); for (uint32_t i = 0; i < n; i++) { float tmp[NF]; std::memcpy(tmp, raw.data() + 16 + (size_t)i * NF * 4, sizeof tmp); R[i] = parse(tmp); }
    const float winS = argc > 3 ? (float)std::atof(argv[3]) : 2.f, stepS = argc > 4 ? (float)std::atof(argv[4]) : 1.f; const float DT = 0.02f;
    const int steps = (int)std::lround(winS / DT);
    const int sign = std::getenv("DH_LEAN") ? std::atoi(std::getenv("DH_LEAN")) : 1;
    std::vector<Win> W; const double traceT = std::getenv("DH_TRACEWIN") ? std::atof(std::getenv("DH_TRACEWIN")) : -1;
    // índice de la última muestra con t <= x (R está ordenado por tiempo)
    auto at = [&](double x, size_t from) { size_t k = from; while (k + 1 < R.size() && R[k + 1].t <= x + 1e-4) k++; return k; };
    double nextStart = R.empty() ? 0 : R[0].t;
    if (std::getenv("DH_FULL")) {       // carrera entera sin resincronizar: DH_FULL=<segundos entre informes>; el error es la distancia a la posición grabada
        const double rep = std::atof(std::getenv("DH_FULL")); size_t i0 = 0; while (i0 < R.size() && std::sqrt(dot(R[i0].v, R[i0].v)) < 8.f) i0++; if (i0 >= R.size()) return 1;
        Bike b; if (!std::getenv("DH_NOENGINE")) useEngineProfile(b.P); b.P.hop = 32.f; applySet(b.P); b.rb.restitution = b.P.restitution; b.rb.friction = b.P.friction;
        b.pos = R[i0].p; b.fwd = unit(R[i0].f); b.rb.vel = R[i0].v; b.rb.linMom = R[i0].v * b.P.mass; b.rb.com = b.pos; b.grounded = R[i0].air < 0.5f; b.coyote = b.grounded ? 0.1f : 0.f; b.groundN = unit(R[i0].u);
        double next = rep, firstBad = -1; float worst = 0; size_t k = i0; const double tEnd = R.back().t - R[i0].t;
        for (double te = 0; te + DT <= tEnd; te += DT) {
            k = at(R[i0].t + te + 1e-4, k); const Rec& in = R[k]; BikeInput bi; bi.steer = in.steer; bi.lean = (float)sign * in.lean; bi.throttle = in.pedal; bi.sprint = in.sprint > 0.5f; if (std::getenv("DH_BRAKE796")) bi.brake = in.b796;
            if (in.release > 0.5f && b.grounded) { b.P.hop = hopSpeed((int)in.charge, 0); bi.hop = true; }
            if (std::getenv("DH_ROLLTAB")) b.P.rolling = 0.025f * in.roll * (float)std::atof(std::getenv("DH_ROLLTAB")); if (b.P.realDrag) b.sprintT = in.sprintT * DT; b.step(g, bi, DT); size_t m = at(R[i0].t + te + DT + 1e-4, k); float e = dist(b.pos, R[m].p); worst = std::fmax(worst, e); if (e > 50.f && firstBad < 0) firstBad = te;
            if (te >= next) { std::printf("  t=%.0f s: error %.1f u, ref |v| %.0f, port |v| %.0f\n", te, e, std::sqrt(dot(R[m].v, R[m].v)), b.speed()); next += rep; }
        }
        std::printf("carrera entera %.0f s: primera vez > 50 u a los %.1f s, error máx %.0f u\n", tEnd, firstBad, worst); return 0;
    }

    if (std::getenv("DH_STEP1")) {      // oráculo de UN tick (T6): estado grabado -> un paso del port con las mismas entradas -> dv frente al grabado, por clase de superficie y pedal (solo ticks en suelo en ambos extremos)
        struct Acc { std::vector<float> refA, portA, diffA, refP, portP, predRoll, codeA, codeL, portTA; };
        std::map<std::pair<int, int>, Acc> M; const float minSp1 = std::getenv("DH_MINSPEED") ? (float)std::atof(std::getenv("DH_MINSPEED")) : 8.f;
        for (size_t i = 0; i + 1 < R.size(); i++) {
            const float dtR = R[i + 1].t - R[i].t; if (dtR < 0.015f || dtR > 0.025f) continue; if (R[i].air > 0.5f || R[i + 1].air > 0.5f) continue;
            const float sp = std::sqrt(dot(R[i].v, R[i].v)); if (sp < minSp1) continue;
            Bike b; if (!std::getenv("DH_NOENGINE")) useEngineProfile(b.P); b.P.hop = 32.f; applySet(b.P); b.rb.restitution = b.P.restitution; b.rb.friction = b.P.friction;
            b.pos = R[i].p; b.fwd = unit(R[i].f); b.rb.vel = R[i].v; b.rb.linMom = R[i].v * b.P.mass; b.rb.omega = b.rb.angMom = V3{}; b.rb.com = b.pos; b.grounded = true; b.coyote = 0.1f; b.groundN = unit(R[i].u); b.surface = (uint16_t)R[i].surf;
            const Rec& in = R[i]; BikeInput bi; bi.steer = in.steer; bi.lean = (float)sign * in.lean; bi.throttle = in.pedal; bi.sprint = in.sprint > 0.5f;
            if (std::getenv("DH_ROLLTAB")) b.P.rolling = 0.025f * in.roll * (float)std::atof(std::getenv("DH_ROLLTAB"));
            b.sprintT = in.sprintT * DT;   // temporizador de esfuerzo extra real (rider+0x7964): la rampa del arrastre no empieza de cero
            b.step(g, bi, DT);
            if (std::getenv("DH_STEP1_EVENTS")) { const float r0 = std::sqrt(dot(R[i].v, R[i].v)), rr = std::sqrt(dot(R[i + 1].v, R[i + 1].v)) / r0, rp = b.speed() / r0; if (rr < 0.93f && rp > 0.97f && R[i + 1].gn.y * R[i + 1].gn.y < 0.36f * dot(R[i + 1].gn, R[i + 1].gn)) { const V3 nr = unit(R[i + 1].gn); auto H = g.sweepHits(R[i].p, R[i].p - nr * 4.f, b.P.radius); std::printf("W t=%.2f pared del juego n=(%.2f %.2f %.2f) v=%.0f ref=%.2f: barrido de %.1f u hacia la pared -> %zu golpes", R[i].t, nr.x, nr.y, nr.z, r0, rr, 4.f, H.size()); if (!H.empty()) std::printf(", 1º n=(%.2f %.2f %.2f) frac=%.2f clase=%d", H[0].normal.x, H[0].normal.y, H[0].normal.z, H[0].frac, (int)(H[0].surface & 0x1F)); std::printf("\n"); }
                if (rr < 0.93f) std::printf("E t=%.2f v=%.0f cls=%d ref=%.2f port=%.2f hits=%u overlaps=%u normal_ref=(%.2f %.2f %.2f) normal_port=(%.2f %.2f %.2f)\n", R[i].t, r0, (int)R[i].surf, rr, rp, b.hits, b.overlaps, unit(R[i + 1].gn).x, unit(R[i + 1].gn).y, unit(R[i + 1].gn).z, b.groundN.x, b.groundN.y, b.groundN.z); }
            const V3 u0 = unit(R[i].v), dref = R[i + 1].v - R[i].v, dport = b.rb.vel - R[i].v; const float ra = dot(dref, u0), pa = dot(dport, u0);
            if (dot(dref, dref) < 1e-10f) continue;          // muestra repetida de la telemetría (sin cambio): no es un tick de física
            // predicción analítica con las fuerzas del motor, proyectada al plano tangente del suelo (el contacto solo quita la componente normal): peso 79.53, arrastre k|v|v (k = 0.0063), rodadura 0.025·coef2·v, pedal 25·pedal a lo largo del avance
            const V3 n = (std::getenv("DH_STEP1_UP") || dot(R[i].gn, R[i].gn) < 0.25f) ? unit(R[i].u) : unit(R[i].gn), vv = R[i].v, fT = unit(R[i].f - n * dot(R[i].f, n)), ut = unit(vv - n * dot(vv, n)), lat = unit(cross(n, ut));
            auto tan = [&](V3 x) { return x - n * dot(x, n); };
            const float kUse = (std::getenv("DH_STEP1_K") && in.kDrag > 0.f) ? in.kDrag : 0.0063f; const float pedMul = (std::getenv("DH_STEP1_K") && in.sprint > 0.5f) ? 1.5f : 1.f;   // DH_STEP1_K=1: k exacta del juego (ctrl+0x490) y pedal x1.5 con esfuerzo extra
            V3 aCode = tan(V3{0, -79.53f, 0}) - tan(vv) * (kUse * sp) - tan(vv) * (0.025f * in.roll) + fT * (25.f * in.pedal * pedMul);
            V3 dT = tan(dref) * (1.f / DT) - aCode; V3 dTp = tan(dport) * (1.f / DT);
            V3 rp = dref - u0 * ra, pp = dport - u0 * pa; Acc& a = M[{(int)R[i].surf, (in.pedal > 0.5f || in.sprint > 0.5f) ? 1 : 0}];
            a.refA.push_back(ra / DT); a.portA.push_back(pa / DT); a.diffA.push_back((pa - ra) / DT); a.refP.push_back(std::sqrt(dot(rp, rp)) / DT); a.portP.push_back(std::sqrt(dot(pp, pp)) / DT); a.predRoll.push_back(-0.025f * in.roll * sp);
            if (std::getenv("DH_STEP1_DUMP") && in.pedal > 0.f) std::printf("D %d %.3f %.3f %.0f %.3f %.3f %.3f %.0f %.0f %.3f %u\n", (int)in.surf, sp, in.pedal, in.sprint, dot(dT, ut), n.y, dot(fT, ut), in.roll, in.trick, in.steer, (unsigned)in.rflags);
            a.codeA.push_back(dot(dT, ut)); a.codeL.push_back(dot(dT, lat)); a.portTA.push_back(dot(dTp - tan(dref) * (1.f / DT), ut));
        }
        std::printf("clase pedal     n | a_along ref  port  dif(port-ref) | a_perp ref  port | rodadura código || residuo (ref - fuerzas de código) tangente: along lateral | port-ref tangente along\n");
        for (auto& kv : M) { Acc& a = kv.second; if (a.refA.size() < 8) continue; std::printf("%5d %5d %5zu | %10.2f %6.2f %8.2f | %10.2f %6.2f | %8.2f || %8.2f %8.2f | %8.2f\n", kv.first.first, kv.first.second, a.refA.size(), pct(a.refA, .5f), pct(a.portA, .5f), pct(a.diffA, .5f), pct(a.refP, .5f), pct(a.portP, .5f), pct(a.predRoll, .5f), pct(a.codeA, .5f), pct(a.codeL, .5f), pct(a.portTA, .5f)); }
        return 0;
    }
    for (size_t i = 0; i < R.size(); i++) {
        if (R[i].t < nextStart) continue;
        double tEnd = R[i].t + winS; if (R.back().t < tEnd) break;
        size_t j = at(tEnd, i); if (std::fabs(R[j].t - tEnd) > 0.015) { continue; }       // hueco de muestreo al final
        float sp0 = std::sqrt(dot(R[i].v, R[i].v)); static const float minSp = std::getenv("DH_MINSPEED") ? (float)std::atof(std::getenv("DH_MINSPEED")) : 8.f, maxSp = std::getenv("DH_MAXSPEED") ? (float)std::atof(std::getenv("DH_MAXSPEED")) : 1e9f; if (sp0 < minSp || sp0 > maxSp) { nextStart = R[i].t + 0.2; continue; }
        // sin huecos grandes dentro de la ventana (muestras perdidas)
        bool gap = false; for (size_t k = i; k < j; k++) if (R[k + 1].t - R[k].t > 0.045f) { gap = true; break; } if (gap) continue;
        if (std::getenv("DH_LAND")) {       // solo ventanas que empiezan en el aire y aterrizan en los primeros 0.3 s (aire->suelo en lo grabado)
            bool landing = R[i].air > 0.5f; size_t k2 = i; bool touched = false; while (landing && k2 < j && R[k2].t < R[i].t + 0.3) { if (R[k2].air < 0.5f) { touched = true; break; } k2++; } if (!landing || !touched) continue; }
        if (const char* pf = std::getenv("DH_PEDALFILTER")) { bool want = std::atoi(pf) != 0; bool all = true; for (size_t k2 = i; k2 <= j && R[k2].t < R[i].t + 0.5; k2++) if ((R[k2].pedal > 0.5f || R[k2].sprint > 0.5f) != want) { all = false; break; } if (!all) continue; }   // 1 = pedal/esfuerzo durante los primeros 0.5 s, 0 = sin pedal (rodando)
        nextStart = R[i].t + stepS;
        Bike b; if (!std::getenv("DH_NOENGINE")) useEngineProfile(b.P); b.P.hop = 32.f; applySet(b.P); b.rb.restitution = b.P.restitution; b.rb.friction = b.P.friction;
        b.pos = R[i].p; b.fwd = unit(R[i].f); b.rb.vel = R[i].v; b.rb.linMom = R[i].v * b.P.mass; b.rb.omega = b.rb.angMom = V3{}; b.rb.com = b.pos;
        const bool ground0 = R[i].air < 0.5f; b.grounded = ground0; b.coyote = ground0 ? 0.1f : 0.f; b.groundN = unit(R[i].u);
        bool allGround = true, g05 = true; float v05 = 0, al = 0, la = 0; bool hasLean = false; size_t k = i; int airTicks = 0; float e05 = 0, e10 = 0, eW = 0, b05 = 0, b10 = 0, bW = 0;
        for (int s = 0; s < steps; s++) {
            double ts = R[i].t + s * DT; k = at(ts + 1e-4, k); const Rec& in = R[k];
            BikeInput bi; bi.steer = in.steer; bi.lean = (float)sign * in.lean; bi.throttle = in.pedal; bi.sprint = in.sprint > 0.5f; if (std::getenv("DH_BRAKE796")) bi.brake = in.b796;
            if (in.release > 0.5f && b.grounded) { b.P.hop = hopSpeed((int)in.charge, 0); bi.hop = true; }
            if (std::getenv("DH_ROLLTAB")) b.P.rolling = 0.025f * in.roll * (float)std::atof(std::getenv("DH_ROLLTAB")); if (bi.lean != 0.f && !b.grounded) hasLean = true; if (b.P.realDrag) b.sprintT = in.sprintT * DT; b.step(g, bi, DT);
            if (traceT >= 0 && R[i].t >= traceT && R[i].t < traceT + (std::getenv("DH_LAND") ? 5.0 : stepS * 1.5) && (s % (std::getenv("DH_TRACESTRIDE") ? std::atoi(std::getenv("DH_TRACESTRIDE")) : 5) == (std::getenv("DH_TRACESTRIDE") ? std::atoi(std::getenv("DH_TRACESTRIDE")) - 1 : 4))) { size_t m = at(ts + DT + 1e-4, k); std::printf("  s=%d port p=(%.1f %.1f %.1f) v=(%.1f %.1f %.1f) gr=%d | ref p=(%.1f %.1f %.1f) v=(%.1f %.1f %.1f) air=%.0f in(st %.2f ln %.0f pd %.0f) fwd.y port %.2f ref %.2f\n", s + 1, b.pos.x, b.pos.y, b.pos.z, b.rb.vel.x, b.rb.vel.y, b.rb.vel.z, (int)b.grounded, R[m].p.x, R[m].p.y, R[m].p.z, R[m].v.x, R[m].v.y, R[m].v.z, R[m].air, in.steer, in.lean, in.pedal, b.fwd.y, R[m].f.y); }
            if (R[at(ts + DT + 1e-4, k)].air > 0.5f) { allGround = false; if (s < 25) g05 = false; }
            const double te = (s + 1) * DT;
            if (s + 1 == (int)std::lround(0.5 / DT) || s + 1 == (int)std::lround(1.0 / DT) || s + 1 == steps) {
                size_t m = at(R[i].t + te + 1e-4, k); float e = dist(b.pos, R[m].p), bl = dist(R[i].p + R[i].v * (float)te, R[m].p);
                if (s + 1 == (int)std::lround(0.5 / DT)) { e05 = e; b05 = bl; v05 = std::fabs(std::sqrt(dot(b.rb.vel, b.rb.vel)) - std::sqrt(dot(R[m].v, R[m].v))); V3 d = b.pos - R[m].p, u0 = unit(R[i].v); al = dot(d, u0); V3 pr = d - u0 * al; la = std::sqrt(dot(pr, pr)); } if (s + 1 == (int)std::lround(1.0 / DT)) { e10 = e; b10 = bl; } if (s + 1 == steps) { eW = e; bW = bl; }
            }
            (void)airTicks;
        }
        float vend = std::sqrt(dot(b.rb.vel, b.rb.vel)), vref = std::sqrt(dot(R[j].v, R[j].v));
        W.push_back({i, e05, e10, eW, b05, b10, bW, std::fabs(vend - vref), std::fabs(sp0 - vref), allGround, g05, v05, al, la, hasLean, std::fabs(b.fwd.y - R[j].f.y)});
    }
    auto report = [&](const char* name, bool onlyGround, bool onlyAir) {
        std::vector<float> a05, a10, aW, c10, cW, ev; size_t ok = 0, cnt = 0;
        for (const Win& w : W) { if (onlyGround && !w.allGround) continue; if (onlyAir && w.allGround) continue; a05.push_back(w.e05); a10.push_back(w.e10); aW.push_back(w.eW); c10.push_back(w.b10); cW.push_back(w.bW); ev.push_back(w.ev); cnt++; if (w.eW <= 0.05f * winS * 60.f) ok++; }
        if (!cnt) { std::printf("%-8s sin ventanas\n", name); return; }
        std::printf("%-8s %4zu ventanas | err pos mediana 0.5s %.2f 1s %.2f %.0fs %.2f u (p90 %.2f) | línea base v-cte 1s %.2f %.0fs %.2f | err |v| mediana %.1f u/s | %zu%% dentro de 0.05·|Δ|\n",
                    name, cnt, pct(a05, .5f), pct(a10, .5f), winS, pct(aW, .5f), pct(aW, .9f), pct(c10, .5f), winS, pct(cW, .5f), pct(ev, .5f), 100 * ok / cnt);
    };
    std::printf("%s: %zu pasos, ventana %.1f s\n", argv[2], R.size(), winS);
    { std::vector<float> e, v; for (const Win& w : W) if (w.g05) { e.push_back(w.e05); v.push_back(w.v05); } std::printf("G05 n=%zu e05_med=%.3f e05_p90=%.3f v05_med=%.3f\n", e.size(), pct(e, .5f), pct(e, .9f), pct(v, .5f)); std::vector<float> A, Aa, L; for (const Win& w : W) if (w.g05) { A.push_back(w.al); Aa.push_back(std::fabs(w.al)); L.push_back(w.la); } std::printf("G05 along_signed_med=%.3f along_abs_med=%.3f lateral_med=%.3f\n", pct(A, .5f), pct(Aa, .5f), pct(L, .5f)); }
    { std::vector<float> pe; for (const Win& w : W) if (w.hasLean) pe.push_back(w.pitchErr); std::printf("LEAN n=%zu pitch_err_med=%.3f p90=%.3f\n", pe.size(), pct(pe, .5f), pct(pe, .9f)); }
    report("todas", false, false); report("suelo", true, false); report("con aire", false, true);
    if (const char* ws = std::getenv("DH_SHOWWORST")) { std::sort(W.begin(), W.end(), [](const Win& a, const Win& b) { return a.eW > b.eW; }); for (int q = 0; q < std::atoi(ws) && q < (int)W.size(); q++) std::printf("  peor t=%.1f s: err %.1f u (base %.1f), %s\n", R[W[q].i].t, W[q].eW, W[q].bW, W[q].allGround ? "suelo" : "con aire"); }
    return 0;
}
