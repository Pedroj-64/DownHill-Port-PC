// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Lista de impactos (FUN_00219970/FUN_00217450), despenetración (FUN_001344F0) y respuesta de contacto (FUN_00134630/00238648/002384B8/00237D28) con casos sintéticos.
// Sin datos del juego: los hit records de los savestates sólo guardan punto/normal/fracción (docs/p2s-savestates.md), no impulsos ni velocidades previas, así que la respuesta no es verificable contra ellos.
#include "../src/contact.hpp"
#include <cstdio>
#include <cstdlib>
#define CHECK(c) do { if (!(c)) { std::printf("FALLO %s:%d %s\n", __FILE__, __LINE__, #c); std::exit(1); } } while (0)
#define NEAR(a, b, e) CHECK(std::fabs((a) - (b)) <= (e))
using Tri = Ground::Tri;
static Tri tri(float x0, float y0, float z0, float x1, float y1, float z1, float x2, float y2, float z2, uint16_t s) { Tri t{{x0, y0, z0, x1, y1, z1, x2, y2, z2}, s}; return t; }
static void quad(std::vector<Tri>& o, float y, float x0, float z0, float x1, float z1, uint16_t s) {   // cara +Y
    o.push_back(tri(x0, y, z0, x0, y, z1, x1, y, z0, s)); o.push_back(tri(x1, y, z1, x1, y, z0, x0, y, z1, s));
}
static SweepHit mk(float frac, V3 n, float pen) { SweepHit h; h.hit = true; h.frac = frac; h.normal = n; h.pen = pen; return h; }
int main() {
    // --- sweepHits: todos los solapes se conservan; sin solape sólo el de menor fracción ---
    { std::vector<Tri> T; quad(T, 0, -50, -50, 50, 50, 1); quad(T, 0.5f, -50, -50, 50, 50, 2);   // dos láminas, la esfera (r=1) en y=0.8 solapa ambas; x,z fuera de la diagonal compartida
      Ground g; g.set(T);
      auto o = g.sweepHits({10, 0.8f, 5}, {10, -5, 5}, 1.f); CHECK(o.size() == 2); for (auto& h : o) CHECK(h.frac == kOverlap);
      auto n = g.sweepHits({10, 10, 5}, {10, -5, 5}, 1.f); CHECK(n.size() == 1); CHECK(n[0].surface == 2);   // la lámina alta primero; la baja no se guarda
      NEAR(10 - 15 * n[0].frac, 1.5f, 0.02f);
      CHECK(g.sweepHits({10, 10, 5}, {10, 8, 5}, 1.f).empty());                                          // no llega
      auto s = g.sweep({10, 0.8f, 5}, {10, -5, 5}, 1.f); CHECK(s.hit && s.frac == o[0].frac && s.tri == o[0].tri); }   // sweep() == primer elemento
    // capacidad 8: con 10 láminas solapadas la lista se corta en cap (FUN_00219970 `if (param_9 < n) return param_9`)
    { std::vector<Tri> T; for (int i = 0; i < 10; i++) quad(T, 0.1f * i, -50, -50, 50, 50, (uint16_t)i); Ground g; g.set(T);
      CHECK(g.sweepHits({10, 1.f, 5}, {10, -5, 5}, 5.f).size() == 8); CHECK(g.sweepHits({10, 1.f, 5}, {10, -5, 5}, 5.f, 3).size() == 3); }
    // --- nextHit: orden (frac, dirección), empates por (punto, índice) ---
    { std::vector<std::vector<SweepHit>> P = {{mk(0.5f, {0, 1, 0}, 0), mk(0.2f, {0, 1, 0}, 0)}, {mk(0.2f, {0, 1, 0}, 0), mk(kOverlap, {0, 1, 0}, 0.1f)}};
      HitRef a = nextHit(P); CHECK(a.point == 1 && a.index == 1);                 // el solape primero
      HitRef b = nextHit(P, a); CHECK(b.point == 0 && b.index == 1);              // 0.2 en el punto 0 (dirección menor)
      HitRef c = nextHit(P, b); CHECK(c.point == 1 && c.index == 0);              // 0.2 empatado, dirección posterior
      HitRef d = nextHit(P, c); CHECK(d.point == 0 && d.index == 0);              // 0.5
      CHECK(!nextHit(P, d).valid()); CHECK(!nextHit({}).valid()); }
    // --- depenetración: máximo |.| por eje entre los impactos con frac<0; >1.0 de módulo -> no se aplica ---
    { std::vector<std::vector<SweepHit>> P = {{mk(kOverlap, {0, 1, 0}, 0.3f), mk(0.4f, {1, 0, 0}, 9.f)}, {mk(kOverlap, {0.6f, 0.8f, 0}, 0.2f), mk(kOverlap, {0, -1, 0}, 0.1f)}};
      Depenetration d = depenetration(P); CHECK(d.apply); NEAR(d.move.x, 0.12f, 1e-6f); NEAR(d.move.y, 0.3f, 1e-6f); NEAR(d.move.z, 0.f, 1e-6f);   // el de frac>0 se ignora
      P[0][0].pen = 2.f; CHECK(!depenetration(P).apply);
      CHECK(depenetration({}).apply); }   // sin impactos: desplazamiento 0 (<= 1.0)
    // --- impulso normal: masa 1, sin rotación (I^-1 = 0). Cae a 10 u/s contra un suelo +Y ---
    { RigidBody b; b.invMass = 1; for (float& f : b.iInv) f = 0; b.vel = {0, -10, 0}; b.linMom = b.vel; V3 p{0, 0, 0}, n{0, 1, 0};
      V3 j0 = normalImpulse(0.f, b, p, n); NEAR(j0.y, 10.f, 1e-4f);                       // restitución 0: para en seco
      V3 j1 = normalImpulse(1.f, b, p, n); NEAR(j1.y, 20.f, 1e-4f);                       // restitución 1: rebota a +10
      applyImpulse(b, j1, p); NEAR(b.vel.y, 10.f, 1e-4f); NEAR(b.linMom.y, 10.f, 1e-4f);
      CHECK(normalImpulse(0.f, b, p, n).y == 0.f); }                                     // se aleja (vn > -0.001): sin impulso
    // --- impulso normal con rotación: contacto 1 u por debajo y desplazado; denominador 1 + n.((I^-1 (r x n)) x r) ---
    { RigidBody b; b.invMass = 0.5f; b.iInv[0] = b.iInv[4] = b.iInv[8] = 2.f; b.vel = {0, -4, 0}; b.linMom = {0, -8, 0}; V3 p{1, -1, 0}, n{0, 1, 0};
      // contacto descentrado: el denominador usa el signo del motor tal cual (puede ser < invMass)
      V3 r = p - b.com; float den = impulseDenominator(b, p, n); V3 chk = cross(cross(r, n) * 2.f, r); NEAR(den, 0.5f + dot(n, chk), 1e-6f);
      // caso físico válido: contacto bajo el cuerpo y centrado (r x n = 0): denominador = invMass
      V3 q{0, -1, 0}; NEAR(impulseDenominator(b, q, n), 0.5f, 1e-6f);
      V3 j = normalImpulse(0.f, b, q, n); NEAR(j.y, 4.f / 0.5f, 1e-4f); applyImpulse(b, j, q); NEAR(b.vel.y, 0.f, 1e-4f); NEAR(b.omega.x, 0.f, 1e-6f); }
    // --- rotación: impulso fuera de línea con el centro de masas: L += r x J, omega = I^-1 L ---
    { RigidBody b; b.invMass = 1; b.iInv[0] = b.iInv[4] = b.iInv[8] = 0.5f; applyImpulse(b, {0, 2, 0}, {1, 0, 0});   // r=(1,0,0) x (0,2,0) = (0,0,2)
      NEAR(b.angMom.z, 2.f, 1e-6f); NEAR(b.omega.z, 1.f, 1e-6f); NEAR(b.vel.y, 2.f, 1e-6f); }
    // --- fricción: tangencial 3 u/s, masa 1, sin rotación, coef 1 -> resta 1 u/s de la velocidad tangencial; punto 2: x1.25 ---
    { for (int idx : {0, 2}) {
        RigidBody b; b.invMass = 1; for (float& f : b.iInv) f = 0; b.vel = {3, -5, 0}; b.linMom = b.vel; b.friction = 1.f; b.restitution = 0.f;
        V3 fi = frictionImpulse(b, {0, 0, 0}, {0, 1, 0}); NEAR(fi.x, -1.f, 1e-5f); NEAR(fi.y, 0.f, 1e-5f);          // módulo 1/denominador, sentido opuesto a la tangente
        float mag = contactResponse(b, {0, 0, 0}, {0, 1, 0}, {1, 0, 0}, idx);   // axis0 = X: f = N x X = (0,0,1); el impulso de fricción (en X) no tiene componente en f -> se conserva
        float k = idx == 2 ? 1.25f : 1.f; NEAR(b.vel.x, 3.f - k, 1e-4f); NEAR(b.vel.y, 0.f, 1e-4f); NEAR(mag, 5.f, 1e-4f); } }
    // --- fricción anisótropa: axis0 = Z -> f = N x Z = (-1,0,0): el componente X de la fricción desaparece; Z se conserva ---
    { RigidBody b; b.invMass = 1; for (float& f : b.iInv) f = 0; b.vel = {3, -5, 4}; b.linMom = b.vel; b.friction = 1.f;
      contactResponse(b, {0, 0, 0}, {0, 1, 0}, {0, 0, 1}, 0);
      NEAR(b.vel.x, 3.f, 1e-4f);                       // sin fricción en X
      NEAR(b.vel.z, 4.f - 0.8f, 1e-4f);                // tangente (3,0,4)/5: fricción -(0.6,0,0.8); sólo queda la parte en Z
      NEAR(b.vel.y, 0.f, 1e-4f); }
    // --- |N x axis0|^2 <= 0.5 (eje casi paralelo a la normal): no se filtra la fricción ---
    { RigidBody b; b.invMass = 1; for (float& f : b.iInv) f = 0; b.vel = {3, -5, 0}; b.linMom = b.vel; b.friction = 1.f;
      contactResponse(b, {0, 0, 0}, {0, 1, 0}, {0, 1, 0}, 0); NEAR(b.vel.x, 2.f, 1e-4f); }
    std::printf("contact_test OK\n"); return 0;
}
