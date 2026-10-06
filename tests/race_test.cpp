// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Máquina de estados de la partida (src/race.hpp) sobre terreno y puertas sintéticos (sin datos del juego): cuenta atrás con reloj parado, arranque a 0, puertas en orden, meta, resultados y formato del tiempo.
#include "../src/race.hpp"
#include <cstdio>
#include <cstdlib>
#define CHECK(c) do { if (!(c)) { std::printf("FALLO %s:%d %s\n", __FILE__, __LINE__, #c); std::exit(1); } } while (0)
using Tri = Ground::Tri;
static Tri tri(V3 a, V3 b, V3 c, uint16_t s) { Tri t{{a.x, a.y, a.z, b.x, b.y, b.z, c.x, c.y, c.z}, s}; return t; }
int main() {
    { CHECK(formatTime(0) == "00:00.00"); CHECK(formatTime(61.239) == "01:01.23"); CHECK(formatTime(431.0) == "07:11.00"); CHECK(formatTime(-3) == "00:00.00"); CHECK(formatTime(0.999) == "00:00.99"); }   // truncado como cronómetro
    Ground g; { std::vector<Tri> T; float a = -4000, b = 4000; T.push_back(tri({a, 0, a}, {a, 0, b}, {b, 0, a}, 1)); T.push_back(tri({b, 0, b}, {b, 0, a}, {a, 0, b}, 1)); g.set(T); }
    Gates gts; { std::vector<Gate> c; for (int i = 0; i < 3; i++) { Gate q; q.kind = 8050; q.idx = i; q.n = {0, 0, -1}; q.d = 100.f * (i + 1); c.push_back(q); } gts.set(c, {}); }   // puerta i: dist = -z - 100(i+1) > 0 al pasar z < -100(i+1)
    Race r; CHECK(r.state == RaceState::Loading); r.load(g, &gts, {0, 0, 0}, 0.f); CHECK(r.state == RaceState::Countdown && r.countdownDigit() == 3);
    BikeInput go; go.throttle = 1;
    // cuenta atrás: 3 s con mandos pulsados; el reloj no corre, la moto no avanza y la puerta 1 no cuenta
    float z0 = r.run.bike.pos.z; int guard = 0; int seen[4] = {0, 0, 0, 0};
    while (r.state == RaceState::Countdown && guard++ < 600) { r.update(g, go, 1.f / 60.f); seen[r.countdownDigit() & 3]++; CHECK(r.displayTime() == 0.0); }
    CHECK(r.state == RaceState::Riding); CHECK(std::fabs(r.run.bike.pos.z - z0) < 1.f); CHECK(seen[3] > 0 && seen[2] > 0 && seen[1] > 0); CHECK(guard >= 179 && guard <= 182); CHECK(r.run.gs.counter == 0 && r.run.t == 0);
    // conducción: el reloj corre, se cruzan las 3 puertas en orden y se llega a la meta (última puerta)
    double last = 0; guard = 0; size_t maxc = 0;
    while (r.state == RaceState::Riding && guard++ < 60 * 120) { r.update(g, go, 1.f / 60.f); CHECK(r.displayTime() >= last); last = r.displayTime(); maxc = std::fmax(maxc, r.run.gs.counter); }
    CHECK(r.state == RaceState::Finished); CHECK(r.res.finished && r.res.gates == 3 && r.res.totalGates == 3 && r.res.splits.size() == 3);
    CHECK(r.res.time > 5.0 && r.res.time < 90.0); CHECK(r.res.splits[0] < r.res.splits[1] && r.res.splits[1] < r.res.splits[2] && std::fabs(r.res.splits[2] - r.res.time) < 0.05);   // el tiempo de meta es el de la última puerta
    CHECK(r.res.maxSpeed > 5.f); CHECK(r.res.respawns == 0);
    // meta -> resultados tras 2 s; el tiempo del resultado queda congelado
    double tf = r.res.time; guard = 0; while (r.state == RaceState::Finished && guard++ < 600) r.update(g, go, 1.f / 60.f);
    CHECK(r.state == RaceState::Results && guard >= 119 && guard <= 122); for (int i = 0; i < 120; i++) r.update(g, go, 1.f / 60.f); CHECK(r.displayTime() == tf && r.res.time == tf);
    // volver a cargar reinicia todo
    r.load(g, &gts, {0, 0, 0}, 0.f); CHECK(r.state == RaceState::Countdown && !r.res.finished && r.displayTime() == 0.0);
    std::printf("race_test OK (tiempo sintético %s, %zu puertas)\n", formatTime(tf).c_str(), (size_t)3); return 0;
}
