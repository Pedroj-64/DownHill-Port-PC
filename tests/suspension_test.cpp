// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Prueba sintética de src/suspension.hpp (sin datos del juego).
#include "../src/suspension.hpp"
#include <cmath>
#include <cstdio>
static int fails = 0;
static void check(bool ok, const char* what) { if (!ok) { std::printf("FALLA: %s\n", what); fails++; } }
static bool near(float a, float b, float e = 1e-5f) { return std::fabs(a - b) <= e; }
int main() {
    susp::Wheel w; w.e = 0.2f; w.off = 0.f; w.s = 0.f;
    susp::step(w, 0.02f); check(near(w.e, 0.2f) && near(w.s, 80.f * 0.3f * 0.02f), "primer paso: e igual, s += a·h con a = 24");
    susp::step(w, 0.02f); check(near(w.e, 0.2f + 0.48f * 0.02f), "e avanza con la s anterior");
    // amortiguamiento: s < 0 usa 10, s >= 0 usa 7 (en equilibrio e = 0.5, off = 0)
    susp::Wheel a; a.e = 0.5f; a.s = -1.f; susp::step(a, 0.1f); check(near(a.s, -1.f + 10.f * 0.1f), "amortiguación negativa 10");
    susp::Wheel b; b.e = 0.5f; b.s = 1.f; susp::step(b, 0.1f); check(near(b.s, 1.f - 7.f * 0.1f), "amortiguación positiva 7");
    // el desplazamiento off entra igual que e
    susp::Wheel c; c.e = 0.3f; c.off = 0.2f; susp::step(c, 0.1f); check(near(c.s, 0.f), "e + off = 0.5 es equilibrio");
    std::printf(fails ? "suspension_test: %d fallos\n" : "suspension_test: OK\n", fails); return fails != 0;
}
