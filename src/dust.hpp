// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Polvo de la rueda trasera (HIPÓTESIS visual): bocanadas translúcidas que nacen tras la rueda al rodar y se disipan. El juego define sus partículas por superficie en RST/<NIVEL>.RST
// (nombres «Dust3_s/m/l», «GrassDust», «Snow», «Water»…, docs/formats/gs-dump.md); aquí sólo se imita el aspecto: tamaño, vida y rumbo son valores de ajuste, no del motor.
#pragma once
#include <algorithm>
#include <cstdint>
#include <vector>
#include "ground.hpp"

namespace dust {
struct Puff { V3 pos, vel; float age = 0.f, life = 1.f, s0 = 0.5f, s1 = 3.f, r = 0.5f, g = 0.4f, b = 0.3f, a0 = 0.3f; };
struct Emitter { std::vector<Puff> puffs; float carry = 0.f; uint32_t rng = 0x9E3779B9u; };
inline float rnd(uint32_t& s) { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (s & 0xFFFFFF) / 16777216.f; }
// Color del polvo por clase física de la superficie (bits 0-4 de la superficie del triángulo): nieve 13-17, agua 2, hierba 3, resto tierra (HIPÓTESIS: mismas clases que el audio).
inline void tint(unsigned cls, float& r, float& g, float& b) {
    if (cls >= 13 && cls <= 17) { r = 0.93f; g = 0.96f; b = 1.f; } else if (cls == 2) { r = 0.7f; g = 0.8f; b = 0.9f; } else if (cls == 3) { r = 0.42f; g = 0.5f; b = 0.3f; } else { r = 0.55f; g = 0.43f; b = 0.33f; }
}
// ~45 bocanadas/s a partir de 65 u/s en el suelo, proporcional a la velocidad; `at` = punto bajo la rueda trasera, `vel` = velocidad de la moto.
inline void emit(Emitter& e, V3 at, V3 vel, float speed, bool grounded, float dt, unsigned cls) {
    if (!grounded || speed < 5.f) { e.carry = 0.f; return; }
    e.carry += std::min(1.f, (speed - 5.f) / 60.f) * 45.f * dt;
    while (e.carry >= 1.f && e.puffs.size() < 200) {
        e.carry -= 1.f; Puff p; p.pos = {at.x + (rnd(e.rng) - 0.5f) * 0.8f, at.y + 0.2f, at.z + (rnd(e.rng) - 0.5f) * 0.8f};
        p.vel = {vel.x * 0.12f + (rnd(e.rng) - 0.5f) * 5.f, vel.y * 0.05f + 1.5f + rnd(e.rng) * 3.5f, vel.z * 0.12f + (rnd(e.rng) - 0.5f) * 5.f};
        p.life = 0.9f + rnd(e.rng) * 0.7f; p.s0 = 1.0f + rnd(e.rng) * 0.6f; p.s1 = 4.5f + rnd(e.rng) * 2.5f; p.a0 = 0.75f + rnd(e.rng) * 0.2f; tint(cls, p.r, p.g, p.b);
        e.puffs.push_back(p);
    }
}
inline void update(Emitter& e, float dt) {
    for (Puff& p : e.puffs) { p.age += dt; p.pos = {p.pos.x + p.vel.x * dt, p.pos.y + p.vel.y * dt, p.pos.z + p.vel.z * dt}; const float k = std::max(0.f, 1.f - 1.8f * dt); p.vel = {p.vel.x * k, p.vel.y * k, p.vel.z * k}; }
    e.puffs.erase(std::remove_if(e.puffs.begin(), e.puffs.end(), [](const Puff& p) { return p.age >= p.life; }), e.puffs.end());
}
}  // namespace dust
