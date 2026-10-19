// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Rozamiento lateral de rueda del motor (FUN_00132FD8 -> FUN_00131078), puro C++ sin dependencias. Doc: docs/formats/physics-module.md (T7).
// Modelo contrastado con las grabaciones (tools/contact_impulse_check.py, 90 % de 11753 ticks en 4 niveles; tests/wheelfric_check.cpp): por cada rueda en contacto, EN ORDEN,
//   s = a·v_tangente (a = fila 0 de la orientación del tick SIGUIENTE, proyectada al plano tangente y normalizada)
//   d = signo(s)·min(|s|·ramp·e, 0.0225·coef0[clase])        (impulso con e = 1; el tope por llamada es 225·coef0·0.01 · invMasa)
//   v -= d·a
// ramp = ctrl+0x48C (acotada a 1; tras el tick: ramp = min(1.3·ramp, 1) con contacto de alguna sonda, 0.25 sin contacto).
// NO cubierto (hipótesis): ticks con impacto del barrido (FUN_00131C90 aplica antes otra llamada) y el eje longitudinal B (cerrado al rodar hacia delante).
#pragma once
#include <cmath>
#include "ground.hpp"   // V3, dot

namespace wheelfric {

constexpr float kCapPerCall = 0.0225f;     // 225 · 0.01 (gp: RAM de carrera) · invMasa 0.01: Δv máximo por llamada = kCapPerCall · coef0
constexpr float kRampStart = 0.25f, kRampMul = 1.3f;

// Velocidad tras las llamadas de rozamiento lateral. lat = fila 0 de la orientación (no hace falta que sea unitario ni tangente), n = normal del suelo.
inline V3 lateral(V3 v, V3 n, V3 lat, float ramp, float coef0, int wheels, float e = 1.f) {
    const float nn = std::sqrt(dot(n, n)); if (nn < 1e-6f) return v; n = n * (1.f / nn); lat = lat - n * dot(lat, n); const float ln = std::sqrt(dot(lat, lat)); if (ln < 1e-6f) return v; lat = lat * (1.f / ln);
    const float cap = kCapPerCall * coef0, r = ramp < 1.f ? ramp : 1.f;
    for (int w = 0; w < wheels; w++) { const V3 vt = v - n * dot(v, n); const float s = dot(lat, vt), a = std::fabs(s) * r * e, d = (s < 0.f ? -1.f : 1.f) * (a < cap ? a : cap); v = v - lat * d; }
    return v;
}
inline float nextRamp(float ramp, bool contact) { return contact ? (ramp * kRampMul < 1.f ? ramp * kRampMul : 1.f) : kRampStart; }

}   // namespace wheelfric
