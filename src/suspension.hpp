// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Suspensión de rueda del cuerpo de conducción (FUN_00133810, esferas 0 y 1 en ctrl+0x2E0/0x360), puro C++ sin dependencias. Doc: docs/formats/physics-module.md (T7).
// Variables en las unidades de la grabación: e = -1.8 - z_local del centro de la esfera (esfera+0x00; FUN_00135138/FUN_00135180: z = reposo + recorrido·ext_n + esfera+0x70),
// off = esfera+0x70 (suavizado hacia la col. 7 de la clase, FUN_00131840), s = esfera+0x60 (velocidad de suspensión).
// Un paso de duración h: a = 80·(0.5 - (e + off)) - (s < 0 ? 10 : 7)·s;  e += s·h;  s += a·h  (constantes de esfera+0x64/0x68/0x6C: 80, 10, 7).
// Contrastado con las grabaciones (tools/suspension_check.py): e exacto (p90 2e-7), s con residuo 2e-4 (redondeo float) en 12 682 ticks con ruedas en contacto, 4 niveles.
// hipótesis: acotado de e a [0, 0.5] (visto en las grabaciones) y de s antes del cálculo (max/min con 0 y 1 en 0x13385c..0x1338a4): sin portar aquí.
#pragma once
namespace susp {

constexpr float kSpring = 80.f, kDampNeg = 10.f, kDampPos = 7.f, kExtMax = 0.5f;

struct Wheel { float e = kExtMax, off = 0.f, s = 0.f; };

inline void step(Wheel& w, float h) {
    const float a = kSpring * (kExtMax - (w.e + w.off)) - (w.s < 0.f ? kDampNeg : kDampPos) * w.s;
    w.e += w.s * h;
    w.s += a * h;
}

}   // namespace susp
