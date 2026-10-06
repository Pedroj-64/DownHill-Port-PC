// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Máquina de estados de la partida (rebanada vertical, fase 1; contrarreloj de una moto, sin rivales): Carga -> Cuenta atrás -> Conducción -> Meta -> Resultados.
// Sin SDL/GL. Reutiliza Run (bike.hpp: moto + puertas + último punto bueno) y la regla de meta de Run::update (última puerta del recorrido, hipótesis: bike-physics.md "Meta").
// HIPÓTESIS: la duración de la cuenta atrás (3 s) y de la pausa de «meta» (2 s) NO salen del ELF (el gestor de salida decomp/startmgr.c no se ha leído para esto); son parámetros.
#pragma once
#include <cstdio>
#include <string>
#include "bike.hpp"

enum class RaceState { Loading, Countdown, Riding, Finished, Results };
inline const char* raceStateName(RaceState s) { static const char* n[] = {"carga", "cuenta atrás", "conducción", "meta", "resultados"}; return n[(int)s]; }

// Tiempo como mm:ss.cc (centésimas, truncadas como un cronómetro).
inline std::string formatTime(double t) {
    if (t < 0) t = 0; long c = (long)(t * 100.0 + 1e-9); char b[24]; std::snprintf(b, sizeof b, "%02ld:%02ld.%02ld", c / 6000, (c / 100) % 60, c % 100); return b;
}

struct RaceResult { bool finished = false; double time = 0; size_t gates = 0, totalGates = 0; unsigned respawns = 0; float maxSpeed = 0; std::vector<float> splits; };

struct Race {
    Run run; const Gates* gates = nullptr; RaceState state = RaceState::Loading;
    bool settle = true;                                   // asienta la moto en la parrilla al cargar (false = deja la caída de 3.8 u de Run::start: reproduce la trayectoria validada de bike_demo)
    float countdownLen = 3.f, finishHold = 2.f;           // HIPÓTESIS (véase arriba)
    float countdown = 0, hold = 0; double raceTime = 0; float maxSpeed = 0; RaceResult res;

    void load(const Ground& g, const Gates* gts, V3 start, float heading) {   // Carga -> Cuenta atrás: coloca la moto en la parrilla
        gates = gts; run.gates = gts; run.start(g, start, heading);
        if (settle) { for (int i = 0; i < 300; i++) run.bike.step(g, BikeInput{}, 1.f / 60.f);   // asienta la moto en la parrilla (Run::start la deja 3.8 u sobre el suelo) y la para
            run.bike.rb.vel = run.bike.rb.omega = run.bike.rb.linMom = run.bike.rb.angMom = V3{}; }
        countdown = countdownLen; hold = 0; raceTime = 0; maxSpeed = 0; res = RaceResult(); state = RaceState::Countdown;
    }
    // Un paso de dt segundos. Devuelve el evento de puerta (+1/-1/0) como Run::update. Durante la cuenta atrás la moto se asienta sin mandos y el cronómetro no corre.
    int update(const Ground& g, const BikeInput& in, float dt) {
        int ev = 0;
        switch (state) {
        case RaceState::Loading: break;
        case RaceState::Countdown:
            countdown -= dt;   // la moto espera parada en la parrilla (como la retiene el gestor de salida: hipótesis)
            if (countdown <= 0.f) { run.gs = Gates::State(); run.t = 0; run.lastSave = 0; run.goodT = 0; run.goodPos = run.bike.pos; run.goodHeading = run.bike.heading(); state = RaceState::Riding; }   // «¡Ya!»: puertas y reloj a cero
            break;
        case RaceState::Riding:
            ev = run.update(g, in, dt); raceTime = run.t; maxSpeed = std::fmax(maxSpeed, run.bike.speed());
            if (run.gs.finished) { raceTime = run.gs.finishTime; hold = finishHold; res.finished = true; res.time = run.gs.finishTime; res.gates = run.gs.counter; res.totalGates = gates ? gates->size() : 0; res.respawns = run.respawns; res.maxSpeed = maxSpeed; res.splits = run.gs.times; state = RaceState::Finished; }
            break;
        case RaceState::Finished:                          // la moto sigue rodando sin mandos mientras dura el aviso de meta
            run.bike.step(g, BikeInput{}, dt); hold -= dt; if (hold <= 0.f) state = RaceState::Results;
            break;
        case RaceState::Results: break;
        }
        return ev;
    }
    double displayTime() const { return state == RaceState::Countdown || state == RaceState::Loading ? 0.0 : raceTime; }   // lo que muestra el cronómetro
    int countdownDigit() const { return state == RaceState::Countdown ? (int)std::ceil(countdown) : 0; }                    // 3, 2, 1 (0 fuera de la cuenta atrás)
};
