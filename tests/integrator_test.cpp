// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Integrador fiel (FUN_00238818 y auxiliares, src/integrator_fidel.hpp) con casos sintéticos; sin datos del juego. Las comprobaciones con datos vivos están en tools/integrator_check.py.
#include "../src/integrator_fidel.hpp"
#include <cstdio>
#include <cstdlib>
#include <vector>
#define CHECK(c) do { if (!(c)) { std::printf("FALLO %s:%d %s\n", __FILE__, __LINE__, #c); std::exit(1); } } while (0)
#define NEAR(a, b, e) CHECK(std::fabs((a) - (b)) <= (e))
using namespace integ;
static Body mk() { Body b; b.invMass = 0.01f; b.invInertia = {0.002f, 0.005f, 0.004f, 0}; b.P.w = b.L.w = 1; b.force.w = b.torque.w = 1; b.nodeRot = identity(); b.iw = identity(); return b; }
static float orthoErr(const M4& R) { float e = 0; for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) { float d = R.r[i].x * R.r[j].x + R.r[i].y * R.r[j].y + R.r[i].z * R.r[j].z - (i == j); e = std::fmax(e, std::fabs(d)); } return e; }
int main() {
    // giro libre: omega = (0,0,1) rad/s con inercia 1 -> tras 1 s (50 pasos de 0.02) la fila 0 gira ~1 rad (fila' = omega x fila, convención de filas)
    { Body b = mk(); b.invInertia = {1, 1, 1, 0}; b.L = {0, 0, 1, 1}; updateDerived(b);
      for (int i = 0; i < 50; i++) integrate(b, 0.02f);
      NEAR(std::atan2(b.nodeRot.r[0].y, b.nodeRot.r[0].x), 1.0f, 2e-3f); NEAR(b.nodeRot.r[0].z, 0, 1e-6f); CHECK(orthoErr(b.nodeRot) < 1e-6f); }
    // lineal: F=(0,0,-1000) un paso -> P=F*dt, vel=P*invMass; la posición usa la velocidad ANTERIOR (Euler explícito): sólo avanza en el 2º paso
    { Body b = mk(); b.force = {0, 0, -1000, 1}; integrate(b, 0.02f);
      NEAR(b.P.z, -20, 1e-4f); NEAR(b.vel.z, -0.2f, 1e-6f); NEAR(b.pos.z, 0, 1e-9f);
      integrate(b, 0.02f); NEAR(b.pos.z, -0.2f * 0.02f, 1e-7f); }
    // la posición del nodo es pos - com*R: com=(0,0,-0.85), R=I -> nodo = pos + (0,0,0.85)
    { Body b = mk(); b.com = {0, 0, -0.85f, 0}; b.pos = {10, 20, 30, 0}; integrate(b, 0.02f); NEAR(b.nodePos.x, 10, 1e-6f); NEAR(b.nodePos.z, 30.85f, 1e-5f); }
    // la inercia "en mundo" es la diagonal aunque R esté girada (hallazgo medido en 16 pilotos): iw = D (D*R^T*R)
    { Body b = mk(); b.nodeRot.r[0] = {0.6f, 0.8f, 0, 0}; b.nodeRot.r[1] = {-0.8f, 0.6f, 0, 0}; b.nodeRot.r[2] = {0, 0, 1, 0}; b.L = {100, 200, 300, 1}; updateDerived(b);
      for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) NEAR(at(b.iw.r[i], j), i == j ? at(b.invInertia, i) : 0.f, 1e-7f);
      NEAR(b.omega.x, 0.2f, 1e-6f); NEAR(b.omega.y, 1.0f, 1e-6f); NEAR(b.omega.z, 1.2f, 1e-6f); }
    // ortonormal tras muchos pasos con omega arbitraria
    { Body b = mk(); b.L = {300, -700, 500, 1}; for (int i = 0; i < 2000; i++) { integrate(b, 0.02f); b.L = {300, -700, 500, 1}; } CHECK(orthoErr(b.nodeRot) < 1e-5f); }
    // dt <= 0 no hace nada; impulso: P += J y vel = P*invMass; translate mueve nodo y centro de masas
    { Body b = mk(); b.pos = {1, 2, 3, 0}; integrate(b, 0); integrate(b, -1); NEAR(b.pos.x, 1, 0); addImpulse(b, {50, 0, 0, 0}); NEAR(b.vel.x, 0.5f, 1e-7f);
      translate(b, {1, 1, 1, 0}); NEAR(b.pos.y, 3, 0); NEAR(b.nodePos.y, 1, 0); }
    // clearForces tras integrar en tick; sin impactos el paso es un único integrate(1/50)
    { Body a = mk(), b = mk(); a.force = b.force = {0, 0, -1000, 1}; a.vel = b.vel = {1, 0, 0, 1}; a.P = b.P = {100, 0, 0, 1};
      integrate(a, 0.02f); int it = tick(b, 0.02f, [](const Body&, const Body&) { return std::vector<Hit>{}; }, [](Body&, std::vector<Hit>&) {}, [](Body&, const Hit&) { return false; });
      CHECK(it == 0); NEAR(a.pos.x, b.pos.x, 0); NEAR(a.P.z, b.P.z, 0); NEAR(b.force.z, 0, 0); NEAR(b.force.w, 1, 0); }
    // un impacto a frac 0.5 con respuesta: se integra medio paso, rem=0.5 y el resto en la 2ª iteración -> avance total = vel*dt (la fuerza ya está a cero en las integraciones parciales)
    { Body b = mk(); b.vel = {10, 0, 0, 1}; b.P = {1000, 0, 0, 1}; b.force = {0, 0, -1000, 1}; int calls = 0, resp = 0;
      int it = tick(b, 0.02f, [&](const Body&, const Body&) { return calls++ == 0 ? std::vector<Hit>{Hit{0.5f, 2}} : std::vector<Hit>{}; }, [](Body&, std::vector<Hit>&) {}, [&](Body&, const Hit& h) { resp++; return h.point == 2; });
      CHECK(it == 1); CHECK(resp == 1); CHECK(calls == 2); NEAR(b.pos.x, 10 * 0.02f, 1e-6f); NEAR(b.P.z, 0, 0); }
    // 2ª iteración con solape persistente: se despenetra una vez con frac=-1 y se termina
    { Body b = mk(); int dep = 0; int it = tick(b, 0.02f, [](const Body&, const Body&) { return std::vector<Hit>{Hit{0.3f, 0}}; }, [&](Body&, std::vector<Hit>& h) { dep++; if (h[0].frac < 0) dep += 10; }, [](Body&, const Hit&) { return false; });
      CHECK(it == 2); CHECK(dep == 11); }
    std::printf("integrator_test OK\n"); return 0;
}
