// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Prueba sintética de src/gamepad.hpp: zona muerta, mezcla con el teclado, asignación y cuantización.
#include "../src/gamepad.hpp"
#include <cstdio>
static int bad = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FALLA %s:%d %s\n", __FILE__, __LINE__, #c); bad++; } } while (0)
int main() {
    CHECK(padDeadzone(0.1f) == 0.f); CHECK(padDeadzone(-0.18f) == 0.f); CHECK(std::fabs(padDeadzone(1.f) - 1.f) < 1e-6f); CHECK(std::fabs(padDeadzone(-1.f) + 1.f) < 1e-6f);
    CHECK(std::fabs(padDeadzone(0.59f) - 0.5f) < 1e-6f);                 // (0.59-0.18)/0.82 = 0.5
    PadState p; CHECK(!padActive(p)); p.lx = 0.1f; CHECK(!padActive(p)); p.lx = 0.5f; CHECK(padActive(p));
    BikeInput in; applyPad(in, p); CHECK(in.steer > 0.3f && in.steer < 0.5f); CHECK(in.throttle == 0.f);
    PadState q; q.dl = true; q.south = true; q.north = true; q.r3 = true; q.du = true; BikeInput b; applyPad(b, q);
    CHECK(b.steer == -1.f); CHECK(b.throttle == 0.f); CHECK(b.brake == 1.f); CHECK(b.sprint); CHECK(b.lean == -1.f);   // Cruz y Triángulo a la vez: manda el freno; cruceta ↑ = hacia delante
    PadState r; r.dl = r.dr = true; BikeInput c; applyPad(c, r); CHECK(c.steer == 0.f);       // ambas = 0, como el teclado
    BikeInput k; k.steer = 1.f; PadState s; s.lx = -0.4f; applyPad(k, s); CHECK(k.steer == 1.f);   // manda la mayor magnitud
    PadState u; u.ly = -1.f; BikeInput d; applyPad(d, u); CHECK(d.lean < -0.99f); BikeInput e; applyPad(e, u, -1.f); CHECK(e.lean > 0.99f);   // stick arriba = inclinar hacia delante; DH_PADLEAN=-1 invierte
    BikeInput kb; kb.throttle = 1.f; PadState v; v.north = true; applyPad(kb, v); CHECK(kb.brake == 1.f); CHECK(kb.throttle == 0.f);   // el freno del mando anula el pedal del teclado
    BikeInput pd; PadState w; w.south = true; applyPad(pd, w); CHECK(pd.throttle == 1.f); CHECK(pd.brake == 0.f);                                 // sin freno, la Cruz pedalea
    BikeInput h; h.hop = true; h.steer = 0.31f; h.lean = -1.f; uint8_t bits = quantizeInput(h); CHECK(bits & kInHop); CHECK(bits & kInRight); CHECK(bits & kInLeanDown); CHECK(!(bits & kInLeft));
    std::printf(bad ? "gamepad_test: %d fallos\n" : "gamepad_test: OK\n", bad); return bad != 0;
}
