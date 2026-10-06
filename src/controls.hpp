// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Controles de teclado de la moto (sin SDL): teclas -> BikeInput, y registro de entradas por tick de 1/60 s para repetir partidas de forma determinista (prueba de regresión, fantasma).
// Asignación por defecto: acelerar W/↑, frenar S/↓, girar A/D o ←/→, inclinar Q (morro arriba) / E (morro abajo), saltar ESPACIO, esfuerzo extra (pedaleo x1.5, HIPÓTESIS de FUN_00136A88: botón +0x7A69) MAYÚS.
#pragma once
#include <cstdint>
#include <cstdio>
#include <vector>
#include "bike.hpp"

struct Keys { bool up = false, down = false, left = false, right = false, w = false, s = false, a = false, d = false, q = false, e = false, space = false, shift = false; };

// Bits de una muestra de entrada (1 byte por tick).
enum : uint8_t { kInThrottle = 1, kInBrake = 2, kInLeft = 4, kInRight = 8, kInLeanUp = 16, kInLeanDown = 32, kInHop = 64, kInSprint = 128 };

inline uint8_t packKeys(const Keys& k, bool hopEdge) {
    uint8_t b = 0;
    if (k.up || k.w) b |= kInThrottle;
    if (k.down || k.s) b |= kInBrake;
    if (k.left || k.a) b |= kInLeft;
    if (k.right || k.d) b |= kInRight;
    if (k.q) b |= kInLeanUp;
    if (k.e) b |= kInLeanDown;
    if (hopEdge) b |= kInHop;                              // el salto es un flanco (una pulsación = un salto)
    if (k.shift) b |= kInSprint;
    return b;
}
inline BikeInput unpackInput(uint8_t b) {
    BikeInput in;
    in.throttle = (b & kInThrottle) ? 1.f : 0.f; in.brake = (b & kInBrake) ? 1.f : 0.f;
    in.steer = ((b & kInRight) ? 1.f : 0.f) - ((b & kInLeft) ? 1.f : 0.f);      // derecha = positivo; ambas = 0
    in.lean = ((b & kInLeanUp) ? 1.f : 0.f) - ((b & kInLeanDown) ? 1.f : 0.f);
    in.hop = (b & kInHop) != 0; in.sprint = (b & kInSprint) != 0;
    return in;
}

// Archivo de entradas: "DHIN" u32 versión(1) u32 n, n bytes. Un byte por tick de 1/60 s.
inline bool saveInputs(const char* path, const std::vector<uint8_t>& v) {
    FILE* f = std::fopen(path, "wb"); if (!f) return false; uint32_t h[3] = {0x4E494844u, 1u, (uint32_t)v.size()};
    bool ok = std::fwrite(h, 4, 3, f) == 3 && (v.empty() || std::fwrite(v.data(), 1, v.size(), f) == v.size()); std::fclose(f); return ok;
}
inline bool loadInputs(const char* path, std::vector<uint8_t>& v) {
    FILE* f = std::fopen(path, "rb"); if (!f) return false; uint32_t h[3]; bool ok = std::fread(h, 4, 3, f) == 3 && h[0] == 0x4E494844u && h[1] == 1u && h[2] <= (1u << 24);
    if (ok) { v.resize(h[2]); ok = v.empty() || std::fread(v.data(), 1, v.size(), f) == v.size(); } std::fclose(f); return ok;
}
