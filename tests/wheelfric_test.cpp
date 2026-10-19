// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Prueba sintética de src/wheelfric.hpp (sin datos del juego).
#include "../src/wheelfric.hpp"
#include <cstdio>
static int fails = 0;
static void check(bool ok, const char* what) { if (!ok) { std::printf("FALLA: %s\n", what); fails++; } }
static bool near(float a, float b, float e = 1e-5f) { return std::fabs(a - b) <= e; }
int main() {
    const V3 n{0, 1, 0}, lat{1, 0, 0};
    // ramp = 1, sin saturar: una rueda anula toda la velocidad lateral (e = 1)
    V3 v = wheelfric::lateral({0.3f, 0, 5.f}, n, lat, 1.f, 100.f, 1); check(near(v.x, 0.f) && near(v.z, 5.f), "una rueda anula 0.3");
    // ramp = 0.25: solo la fracción 0.25 con una rueda; con dos ruedas queda (1-0.25)^2
    v = wheelfric::lateral({1.f, 0, 0}, n, lat, 0.25f, 100.f, 1); check(near(v.x, 0.75f), "ramp 0.25 una rueda");
    v = wheelfric::lateral({1.f, 0, 0}, n, lat, 0.25f, 100.f, 2); check(near(v.x, 0.5625f), "ramp 0.25 dos ruedas");
    // saturación: tope por llamada 0.0225·coef0 (coef0 = 80 -> 1.8 u/s), dos ruedas -> 3.6
    v = wheelfric::lateral({10.f, 0, 0}, n, lat, 1.f, 80.f, 1); check(near(v.x, 10.f - 1.8f, 1e-4f), "tope una rueda");
    v = wheelfric::lateral({10.f, 0, 0}, n, lat, 1.f, 80.f, 2); check(near(v.x, 10.f - 3.6f, 1e-4f), "tope dos ruedas");
    v = wheelfric::lateral({-10.f, 0, 0}, n, lat, 1.f, 80.f, 2); check(near(v.x, -10.f + 3.6f, 1e-4f), "signo negativo");
    // sin ruedas / eje degenerado: sin cambios
    v = wheelfric::lateral({1.f, 2.f, 3.f}, n, lat, 1.f, 80.f, 0); check(near(v.x, 1.f), "sin ruedas");
    v = wheelfric::lateral({1.f, 2.f, 3.f}, n, n, 1.f, 80.f, 2); check(near(v.x, 1.f) && near(v.y, 2.f), "eje paralelo a la normal");
    // la componente normal no se toca
    v = wheelfric::lateral({0.5f, -7.f, 0}, n, lat, 1.f, 100.f, 2); check(near(v.y, -7.f), "normal intacta");
    // rampa: 0.25 -> 0.325 -> ... -> 1; sin contacto vuelve a 0.25
    float r = 0.25f; for (int i = 0; i < 6; i++) r = wheelfric::nextRamp(r, true); check(near(r, 1.f), "la rampa llega a 1");
    check(near(wheelfric::nextRamp(0.9f, false), 0.25f), "sin contacto reinicia");
    std::printf(fails ? "wheelfric_test: %d fallos\n" : "wheelfric_test: OK\n", fails); return fails != 0;
}
