// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Mando (sin SDL): estado de botones/ejes -> BikeInput. Asignación (manual de SLES-52202: X pedalea, R2 salto, Triángulo freno, D-Pad gira/inclina, X×2 o R3 esfuerzo):
//   stick izquierdo X o cruceta ←/→ = girar (analógico con zona muerta); stick izquierdo Y o cruceta ↑/↓ = inclinar (arriba = inclinar hacia delante, morro abajo, como lo probó un jugador con mando; antes estaba al revés; DH_PADLEAN=-1 lo invierte);
//   Cruz (SDL SOUTH) = pedalear; Triángulo (NORTH; Y en Xbox) = freno, que anula el pedaleo mientras está pulsado (con el pedal pulsado el freno de 28 u/s² apenas se notaba frente a los ~25 u/s² del pedaleo); R2 = salto (flanco); R1 o R3 = esfuerzo extra. Se combina con el teclado: manda el de mayor magnitud.
#pragma once
#include <cmath>
#include <cstdint>
#include "controls.hpp"

struct PadState { float lx = 0, ly = 0, rt = 0; bool dl = false, dr = false, du = false, dd = false, south = false, north = false, r1 = false, r3 = false; };

// Zona muerta radial por eje con reescalado: |v| <= dz -> 0, |v| = 1 -> 1.
inline float padDeadzone(float v, float dz = 0.18f) { const float a = std::fabs(v); if (a <= dz) return 0.f; const float s = (a - dz) / (1.f - dz); return v < 0 ? -std::fmin(s, 1.f) : std::fmin(s, 1.f); }

inline bool padActive(const PadState& p) { return p.dl || p.dr || p.du || p.dd || p.south || p.north || p.r1 || p.r3 || p.rt > 0.5f || padDeadzone(p.lx) != 0.f || padDeadzone(p.ly) != 0.f; }

// Mezcla el mando en `in` (ya con el teclado). leanSign = -1 invierte arriba/abajo.
inline void applyPad(BikeInput& in, const PadState& p, float leanSign = 1.f) {
    float steer = padDeadzone(p.lx); if (p.dr) steer = 1.f; if (p.dl) steer = -1.f; if (p.dl && p.dr) steer = 0.f;
    if (std::fabs(steer) > std::fabs(in.steer)) in.steer = steer;
    float lean = padDeadzone(p.ly) * leanSign; if (p.du) lean = -leanSign; if (p.dd) lean = leanSign;   // SDL: stick arriba = ly negativo = lean negativo = inclinar hacia delante
    if (std::fabs(lean) > std::fabs(in.lean)) in.lean = lean;
    if (p.south) in.throttle = 1.f;
    if (p.north) { in.brake = 1.f; in.throttle = 0.f; }   // frenar manda sobre pedalear (también sobre el pedal del teclado)
    if (p.r1 || p.r3) in.sprint = true;
}

// Byte de entradas por tick (como packKeys) a partir de BikeInput: para registrar/repetir partidas jugadas con mando (se pierde lo analógico).
inline uint8_t quantizeInput(const BikeInput& in) {
    uint8_t b = 0;
    if (in.throttle > 0.5f) b |= kInThrottle;
    if (in.brake > 0.5f) b |= kInBrake;
    if (in.steer < -0.3f) b |= kInLeft;
    if (in.steer > 0.3f) b |= kInRight;
    if (in.lean > 0.3f) b |= kInLeanUp;
    if (in.lean < -0.3f) b |= kInLeanDown;
    if (in.hop) b |= kInHop;
    if (in.sprint) b |= kInSprint;
    return b;
}
