// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Puertas de control del nivel: planos (kinds 8050/8051 = puertas del recorrido, 8052 = meta [hipótesis]) y el contador de cruce del juego.
// Citas: FUN_001A33D0 (carga), FUN_002279E8 (distancia = n.p - d), FUN_001A2738 (contador por jugador). Layout .gates: tools/markers.py write_gates.
#pragma once
#include "ground.hpp"

struct Gate { uint32_t kind = 0, idx = 0; V3 n; float d = 0; };
inline float gateDist(const Gate& g, V3 p) { return dot(g.n, p) - g.d; }      // FUN_002279E8

class Gates {
public:
    struct State { size_t counter = 0; std::vector<float> times; bool finished = false; float finishTime = 0; };
    bool load(const std::vector<uint8_t>& raw) {          // u32 n; n * {u32 kind, u32 idx, f32 nx ny nz d} en espacio NGP (Z arriba) -> Y arriba
        if (raw.size() < 4) return false;
        uint32_t n; std::memcpy(&n, raw.data(), 4); if (raw.size() != 4 + (size_t)n * 24) return false;
        for (uint32_t i = 0; i < n; i++) {
            uint32_t k[2]; float f[4]; std::memcpy(k, raw.data() + 4 + i * 24, 8); std::memcpy(f, raw.data() + 12 + i * 24, 16);
            Gate g; g.kind = k[0]; g.idx = k[1]; g.n = {f[0], f[2], -f[1]}; g.d = f[3];
            if (g.kind == 8052) finish_.push_back(g); else course_.push_back(g);
        }
        std::sort(course_.begin(), course_.end(), [](const Gate& a, const Gate& b) { return a.idx < b.idx; });
        return true;
    }
    void set(std::vector<Gate> course, std::vector<Gate> finish) { course_ = std::move(course); finish_ = std::move(finish); }
    size_t size() const { return course_.size(); }
    const Gate* courseGate(size_t i) const { return i < course_.size() ? &course_[i] : nullptr; }   // puerta i del recorrido (orden por índice); sirve para el rumbo de salida
    // Una puerta por llamada, igual que FUN_001A2738: avanza si el jugador está del lado positivo de la puerta `counter`; retrocede si está del lado negativo de la anterior.
    // Devuelve +1 / -1 / 0 (cruce hacia delante / hacia atrás / nada).
    int update(State& s, V3 pos, float t) const {
        if (s.counter < course_.size() && gateDist(course_[s.counter], pos) > 0.f) { s.counter++; s.times.push_back(t); return 1; }
        if (s.counter > 0 && gateDist(course_[s.counter - 1], pos) < 0.f) { s.counter--; s.times.pop_back(); return -1; }
        if (!s.finished && s.counter == course_.size() && !finish_.empty() && gateDist(finish_[0], pos) > 0.f) { s.finished = true; s.finishTime = t; }   // meta = 8052 (hipótesis)
        return 0;
    }
private:
    std::vector<Gate> course_, finish_;
};
