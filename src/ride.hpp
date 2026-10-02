// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Cuerpo cinemático mínimo para la demo de bici: una esfera con gravedad que desliza por la malla de colisión (Ground::sweep) con la estructura de
// sub-pasos de FUN_001340D8 (como mucho 2 barridos por paso; el tiempo restante se reparte tras el primer impacto). NO es la física de la bici del juego.
// Unidades: Y arriba (ver docs/formats/coordinates.md); 10 u = 1 m es una hipótesis (docs).
#pragma once
#include "ground.hpp"

struct RideParams {
    float radius = 3.f;          // 0.3 m: parámetro de la demo (en el juego es el radio de cada punto de contacto, body+0x15C+16*i)
    float gravity = 98.f;        // 9.8 m/s^2 * 10 u/m
    float pedal = 15.f;          // empuje de la demo hacia el objetivo cuando va lento (el juego tiene pedaleo del jugador)
    float pedalBelow = 80.f;     // u/s por debajo de los cuales empuja
    float rolling = 0.04f;       // rodadura 1/s (mismo valor que el modo bici previo de dhview)
    float turnRate = 2.5f;       // rad/s máx. que el piloto automático gira la velocidad horizontal
    float airTurn = 0.6f;        // rad/s de control en el aire (la demo no modela saltos del jugador)
    float airPull = 1.5f;        // 1/s: en el aire la velocidad horizontal tiende hacia el objetivo (piloto automático de la demo; no existe en el juego)
    float maxSpeed = 400.f;      // 40 m/s horizontales (la caída vertical no se limita)
};

struct RideBody {
    V3 pos, vel; bool grounded = false; V3 groundNormal{0, 1, 0}; uint16_t surface = 0; uint32_t sweeps = 0, hits = 0, overlaps = 0;

    // target: punto hacia el que dirige el piloto automático (plano XZ). dt en segundos.
    void step(const Ground& g, const RideParams& P, V3 target, float dt) {
        vel.y -= P.gravity * dt;
        {                                                         // dirección horizontal hacia el objetivo, giro limitado (como DH_AUTOSTEER del modo previo)
            float hx = vel.x, hz = vel.z, sp = std::sqrt(hx * hx + hz * hz), wx = target.x - pos.x, wz = target.z - pos.z, wl = std::sqrt(wx * wx + wz * wz);
            if (wl > 1e-3f) {
                wx /= wl; wz /= wl;
                if (sp < 1e-3f) { hx = wx; hz = wz; sp = 1e-3f; }
                float cur = std::atan2(hz, hx), want = std::atan2(wz, wx), df = want - cur;
                while (df > 3.14159f) df -= 6.28318f; while (df < -3.14159f) df += 6.28318f;
                float tr = grounded ? P.turnRate : P.airTurn;
                float a = cur + std::fmax(-tr * dt, std::fmin(tr * dt, df));
                if (grounded && sp < P.pedalBelow) sp += P.pedal * dt;
                vel.x = std::cos(a) * sp; vel.z = std::sin(a) * sp;
                if (!grounded) { float want = std::fmax(sp, P.pedalBelow), k = std::fmin(1.f, P.airPull * dt); vel.x += (wx * want - vel.x) * k; vel.z += (wz * want - vel.z) * k; }
            }
            if (grounded) { float k = 1.f - P.rolling * dt; vel.x *= k; vel.z *= k; }
        }
        float sh = std::sqrt(vel.x * vel.x + vel.z * vel.z); if (sh > P.maxSpeed) { vel.x *= P.maxSpeed / sh; vel.z *= P.maxSpeed / sh; }
        grounded = false; float rem = 1.f;
        for (int it = 0; it < 2 && rem > 1e-4f; it++) {           // FUN_001340D8: `if (iVar13 == 2) ... 1 < iVar13 -> break`: dos sub-pasos
            V3 p1 = pos + vel * (dt * rem); SweepHit h = g.sweep(pos, p1, P.radius); sweeps++;
            if (!h.hit) { pos = p1; break; }
            hits++;
            if (h.frac < 0.f) {                                    // solapaba: sacar a lo largo de la normal (FUN_001344F0 hace la depenetración en el juego)
                overlaps++; pos = pos + h.normal * h.pen; if (dot(vel, h.normal) < 0) vel = vel - h.normal * dot(vel, h.normal);
            } else {
                pos = pos + (p1 - pos) * h.frac; rem *= (1.f - h.frac);
                float vn = dot(vel, h.normal); if (vn < 0) vel = vel - h.normal * vn;   // respuesta inelástica: se quita la componente que entra
            }
            if (h.normal.y > 0.5f) { grounded = true; groundNormal = h.normal; surface = h.surface; }
        }
    }
};
