// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Moto jugable APROXIMADA (hito 3, carril A). Cuerpo rígido con integrador semi-implícito propio sobre la cadena del motor ya portada: barrido de los 4 puntos de contacto
// (Ground::sweepHits, FUN_0021A908/00219970), orden de impactos (nextHit, FUN_00217450), despenetración (FUN_001344F0) y respuesta de contacto (contactResponse, FUN_00134630).
// El integrador de aquí es propio (semi-implícito); el fiel (FUN_00238818) vive en integrator_fidel.hpp. De FUN_00134060 salen g y el amortiguamiento (namespace engine); lo que no sale del ELF/savestates va marcado `hipótesis`.
// Unidades: Y arriba, unidades de mundo del juego (sin conversión a metros: g = 96.6 u/s^2, véase integrator.md). Ejes locales de la moto como en el motor (node+0x40 fila 0 = lateral): x lateral, y adelante, z arriba (savestate).
#pragma once
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include "contact.hpp"
#include "gates.hpp"
#include "integrator_fidel.hpp"

// Constantes y orden del paso de fuerzas del motor, del desensamblado (docs/formats/integrator.md "Procedencia resuelta", physics-module.md). Unidades de mundo del juego, SIN conversión a metros.
namespace engine {
constexpr float kTickHz = 50.f;                                         // FUN_0023F078: u32 en 0x2C85FC = 50 (paso físico de 1/50 s)
inline float gravity() { return std::bit_cast<float>(0xC2C13334u); }    // FUN_00134060 0x1340A4: lui 0xC2C1; ori 0x3334 = -96.6000061 u/s^2 (= 3 x 32.2f en float32; el significado de 'unidad' es hipótesis)
inline float massWeight() { return std::bit_cast<float>(0x42C80000u); } // módulo+0x110 = 100.0 (FUN_00133AB0 0x133AB0): masa para el peso
inline float linDamp() { return std::bit_cast<float>(0x3F79999Au); }    // módulo+0x120 = 0.975 por tick (FUN_00133AB0 0x133B00..0x133B38; leído en FUN_00134060 0x134080)
inline float angDamp() { return std::bit_cast<float>(0x3F7CAC08u); }    // módulo+0x11C = 0.987 por tick (FUN_00133AB0 0x133AEC; leído en FUN_00134060 0x13408C)
// FUN_00134060(módulo): (1) FUN_00237C78 fuerza y par a cero; (2) FUN_00237960(cuerpo, +0x120): P y vel (xyz) *= f lineal; (3) FUN_00237998(cuerpo, +0x11C): L y omega (xyz) *= f angular;
// (4) FUN_00237C88(cuerpo, (0,0,m*G)) con G negativo: suma el peso a la fuerza. Después FUN_001340D8 llama a FUN_00238818 (integ::integrate / integ::tick).
inline void preStep(integ::Body& b, float kLin = linDamp(), float kAng = angDamp(), float m = massWeight(), float G = gravity()) {
    integ::clearForces(b);
    b.P.x *= kLin; b.P.y *= kLin; b.P.z *= kLin; b.vel.x *= kLin; b.vel.y *= kLin; b.vel.z *= kLin;
    b.L.x *= kAng; b.L.y *= kAng; b.L.z *= kAng; b.omega.x *= kAng; b.omega.y *= kAng; b.omega.z *= kAng;
    b.force.z += m * G;
}
}  // namespace engine

struct BikeInput { float throttle = 0, brake = 0, steer = 0, lean = 0; bool hop = false, sprint = false; };   // steer > 0 = derecha; lean > 0 = morro arriba; hop = un solo paso

struct BikeParams {
    // --- del savestate (rider+0x6420, docs/p2s-savestates.md y bike-physics.md) ---
    V3 pts[4] = {{0, 2.5f, -2.6f}, {0, -1.4f, -2.8f}, {0, 1.5f, 0.5f}, {0, -0.75f, 0}};   // +0x150+16i (local lateral, adelante, arriba): rueda delantera, trasera, dos del cuerpo
    float radius = 0.75f;                                  // +0x15C+16i
    float mass = 100.f;                                    // body+0x00 invMass = 0.01
    float invI[3] = {0.0023f, 0.00474f, 0.00382f};         // body+0x80/0x90/0xA0 diagonal; asignación a (lateral, adelante, arriba): hipótesis
    float restitution = 0.2f, friction = 0.28f;            // módulo +0x114 / +0x118
    float gravity = -engine::gravity();                    // 96.6 u/s^2: FUN_00134060 0x1340A4 (g del motor, módulo+0x110 = masa del peso)
    float linDamp = engine::linDamp();                     // 0.975 por tick de 1/50 s sobre P y vel: módulo+0x120, FUN_00134060 / FUN_00237960
    float angDamp = engine::angDamp();                     // 0.987 por tick sobre L y omega: módulo+0x11C, FUN_00134060 / FUN_00237998
    // --- HIPÓTESIS (no salen del ELF; afinadas para que se conduzca, véase bike-physics.md) ---
    float sprintMul = 1.5f;                                // esfuerzo extra: FUN_00136A88 multiplica la fuerza de pedaleo por 1.5 con el botón rider+0x7A69 (la hipótesis es la correspondencia con una tecla)
    float pedalAccel = 60.f, pedalMax = 50.f;              // u/s^2 de pedaleo y velocidad a la que deja de empujar (hipótesis; reajustado: el amortiguamiento lineal del motor, 1.27/s, frena en llano: v_eq = a/(c + a/vmax) ~ 24 u/s)
    float brakeDecel = 28.f;                               // u/s^2 con el freno a fondo (nunca invierte el sentido)
    float rolling = 0.f;                                   // rodadura 1/s (hipótesis; 0: el término de suelo de abajo ya la cubre)
    // Conducción (hito 3b, FUN_00136330; ride_force_check.py): el cuerpo de conducción NO usa el amortiguamiento lineal 0.975 de FUN_00134060 sino arrastre cuadrático -k|v|v (validado a 4.6e-7, k = ctrl+0x490 = 0.0063..0.0079).
    // groundShare = 0.46 (con k = 0.0063 del motor) se AJUSTÓ a 191 ventanas de 50 ticks de rodadura sin entrada (ride_s1/s5, roll_s1/s5/s5b): 72 % dentro del 15 % de la velocidad capturada (mediana 10.3 %; sin término de suelo 12 %). HIPÓTESIS.
    float dragK = 0.0063f, dragKAir = 0.0077f;             // 1/u: aceleración de arrastre = k·|v|²; valores del motor (ctrl+0x490): 0.00627/0.00632 con ctrl+0x4A4 = 0 (suelo), 0.00744/0.0079 con >= 2 (aire)
    float groundShare = 0.46f;                             // fracción de la gravedad tangente a la pendiente que sobrevive en suelo (el resto lo anula la fuerza de suelo desconocida, origen abierto: physics-module.md "3b"). HIPÓTESIS empírica
    float comLift = 1.4f;                                  // sube todos los puntos locales (baja el centro de masas efectivo): el motor da los puntos respecto al nodo y el centro de masas está desplazado (+0x50), desplazamiento desconocido: hipótesis de estabilidad
    float rollConst = 0.8f;                                // u/s^2 de rodadura constante en el suelo: detiene la moto en llano (hipótesis)
    float pitchDamp = 0.975f;                              // HIPÓTESIS (no es del motor): amortiguación extra del cabeceo por tick de 1/50 s; era el único uso del 0.975 antes de localizar FUN_00134060, y sin ella el cabeceo oscila en las caídas largas de ALP2
    float grip = 10.f;                                     // 1/s: rapidez con que el neumático anula la velocidad lateral (el motor sólo tiene la fricción anisótropa de FUN_00134630)
    float steerRate = 1.5f, steerSpeedK = 0.03f;           // rad/s de giro a velocidad 0 y su caída con la velocidad (u/s)
    float airYawAccel = 3.f, airYawMax = 0.9f;             // HIPÓTESIS: dirección en el aire (rad/s^2 y tope rad/s); el motor no se ha estudiado en vuelo. Sin esto la moto conserva el rumbo con el que sale del borde
    float leanAccel = 7.f;                                 // rad/s^2 sobre el eje lateral (inclinar el cuerpo adelante/atrás)
    float hop = 15.6f;                                     // u/s de impulso del salto (hipótesis; con g = 96.6 da la misma altura ~1.26 u que 9 u/s con el g antiguo)
    float subStep = 1.f / 120.f;                           // paso máximo del integrador
};

struct Axes { V3 r, f, u; };                               // lateral, adelante, arriba (derecha)
inline V3 unit(V3 a) { float l = std::sqrt(dot(a, a)); return l > 1e-9f ? a * (1.f / l) : V3{}; }
// Orientación con balanceo bloqueado: sólo `f` (rumbo + cabeceo) es estado; r = f x Yarriba, u = r x f. (El motor mantiene la moto en pie con otro controlador; los puntos de contacto
// tienen x = 0, así que el cuerpo rígido solo no se sostendría de lado: hipótesis.)
inline Axes axesFromFwd(V3 f) {
    f = unit(f); V3 r = cross(f, {0, 1, 0}); if (dot(r, r) < 1e-6f) r = {1, 0, 0};
    r = unit(r); return {r, f, cross(r, f)};
}
inline V3 rotateAbout(V3 v, V3 w, float t) {               // Rodrigues: v girado el ángulo |w|*t alrededor de w
    float l = std::sqrt(dot(w, w)), th = l * t; if (th < 1e-9f) return v;
    V3 k = w * (1.f / l); float c = std::cos(th), s = std::sin(th);
    return v * c + cross(k, v) * s + k * (dot(k, v) * (1.f - c));
}
inline float comp(V3 v, int i) { return i == 0 ? v.x : i == 1 ? v.y : v.z; }

struct Bike {
    BikeParams P; RigidBody rb; V3 pos; V3 fwd{0, 0, -1};
    bool grounded = false, wheelF = false, wheelR = false; float coyote = 0, tF = 0, tR = 0;   // grounded/wheelF/wheelR = contacto en los últimos 0.1 s (en reposo el barrido a veces no llega a tocar en un sub-paso)
    V3 groundN{0, 1, 0}; uint16_t surface = 0;
    uint32_t sweeps = 0, hits = 0, overlaps = 0; double airTime = 0;
    FILE* trace = nullptr; bool traceOn = false;           // traza de depuración (DH_CONTACT_TRACE en bike_demo): una línea por respuesta de contacto

    Bike() { rb.invMass = 1.f / P.mass; rb.restitution = P.restitution; rb.friction = P.friction; }
    Axes axes() const { return axesFromFwd(fwd); }
    V3 vel() const { return rb.vel; }
    float speed() const { return std::sqrt(dot(rb.vel, rb.vel)); }
    float heading() const { return std::atan2(fwd.x, -fwd.z); }   // convención de dhview: adelante = (sin h, 0, -cos h)
    V3 point(int i, const Axes& A, V3 c) const { return c + A.r * P.pts[i].x + A.f * P.pts[i].y + A.u * (P.pts[i].z + P.comLift); }
    void place(V3 p, float heading) {
        pos = p; fwd = {std::sin(heading), 0, -std::cos(heading)}; rb.vel = rb.omega = rb.linMom = rb.angMom = V3{}; rb.com = p;
        grounded = wheelF = wheelR = false; coyote = tF = tR = 0; groundN = {0, 1, 0};
    }

    // Paso de simulación (dt de pantalla): sub-pasos de P.subStep como máximo.
    void step(const Ground& g, const BikeInput& in, float dt) {
        int n = std::max(1, (int)std::ceil(dt / P.subStep)); float h = dt / n;
        for (int i = 0; i < n; i++) sub(g, in, h, i == 0);
    }

private:
    void syncInertia(const Axes& A) {        // I^-1 en mundo (suma de ejes) y momento angular a partir de omega
        V3 ax[3] = {A.r, A.f, A.u}; for (float& v : rb.iInv) v = 0; rb.angMom = {};
        for (int k = 0; k < 3; k++) {
            for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) rb.iInv[3 * i + j] += P.invI[k] * comp(ax[k], i) * comp(ax[k], j);
            rb.angMom = rb.angMom + ax[k] * (dot(ax[k], rb.omega) / P.invI[k]);
        }
    }
    void advance(float t) { pos = pos + rb.vel * t; fwd = unit(rotateAbout(fwd, rb.omega, t)); rb.com = pos; }

    void sub(const Ground& g, const BikeInput& in, float h, bool first) {
        Axes A = axes(); rb.com = pos; syncInertia(A);
        // --- mandos y fuerzas (hipótesis: ver BikeParams) ---
        // orden de FUN_00134060: primero el amortiguamiento de P/vel y L/omega (factor por tick de 1/50 s llevado al sub-paso h), luego las fuerzas (peso incluido)
        float ka = std::pow(P.angDamp, engine::kTickHz * h);
        rb.omega = rb.omega * ka;
        bool gr = coyote > 0.f; V3 v = rb.vel; v = v * (1.f / (1.f + (gr ? P.dragK : P.dragKAir) * std::sqrt(dot(v, v)) * h));   // arrastre cuadrático implícito (estable a cualquier h)
        if (gr) {
            V3 f = unit(fwd - groundN * dot(fwd, groundN)); float sf = dot(v, f);
            if (in.throttle > 0) v = v + f * (P.pedalAccel * (in.sprint ? P.sprintMul : 1.f) * in.throttle * std::fmax(0.f, 1.f - sf / P.pedalMax) * h);
            if (in.brake > 0 && sf > 0) v = v - f * std::fmin(sf, P.brakeDecel * in.brake * h);
            v = v - A.r * (dot(v, A.r) * std::fmin(1.f, P.grip * h)); v = v * (1.f - P.rolling * h);
            { float vs = std::sqrt(dot(v, v)); if (vs > 1e-6f) v = v * (std::fmax(0.f, vs - P.rollConst * h) / vs); }
        }
        v.y -= P.gravity * h;
        if (gr) { V3 gt = V3{0, -P.gravity, 0}; gt = gt - groundN * dot(gt, groundN); v = v - gt * ((1.f - P.groundShare) * h); }   // fuerza de suelo empírica: anula (1-groundShare) de la gravedad tangente (hipótesis)
        if (in.hop && first && gr) { v = v + A.u * P.hop; coyote = 0; }
        rb.vel = v; rb.linMom = v * P.mass;
        float wp = dot(rb.omega, A.r), wy = rb.omega.y;
        if (gr) wy = -in.steer * P.steerRate / (1.f + P.steerSpeedK * std::sqrt(dot(v, v))); else wy = std::fmax(-P.airYawMax, std::fmin(P.airYawMax, wy * (1.f - 0.3f * h) - in.steer * P.airYawAccel * h));   // derecha = giro negativo sobre Y
        wp *= std::pow(P.pitchDamp, engine::kTickHz * h); wp += in.lean * P.leanAccel * h;
        rb.omega = A.r * wp + V3{0, 1, 0} * wy; syncInertia(A);

        // --- barrido y respuesta: estructura de FUN_001340D8 (como mucho 2 impactos por paso) ---
        bool hitGround = false; float rem = 1.f; bool fHit = false, rHit = false;
        for (int it = 0; it < 2 && rem > 1e-4f; it++) {
            Axes A0 = axes(); Axes A1 = axesFromFwd(rotateAbout(fwd, rb.omega, h * rem)); V3 disp = rb.vel * (h * rem);
            std::vector<std::vector<SweepHit>> L(4);
            for (int i = 0; i < 4; i++) { L[i] = g.sweepHits(point(i, A0, pos), point(i, A1, pos + disp), P.radius); sweeps++; }
            for (int i = 0; i < 2; i++) for (const SweepHit& q : L[i]) if (q.normal.y > 0.3f) { hitGround = true; groundN = q.normal; surface = q.surface; (i == 0 ? fHit : rHit) = true; }   // contacto de rueda en este paso, aunque no sea el impacto que se procesa
            HitRef hr = nextHit(L); if (!hr.valid()) break;
            const SweepHit H = L[hr.point][hr.index]; hits++;
            if (H.frac < 0.f) {                                    // solape inicial: FUN_001344F0 mueve el cuerpo; la respuesta de velocidad a continuación es hipótesis (el motor la deja al impacto siguiente)
                overlaps++; Depenetration d = depenetration(L);
                if (trace && traceOn) std::fprintf(trace, "it=%d DEPEN pt=%d pen=%.3f move=(%.2f %.2f %.2f) |move|=%.2f apply=%d\n", it, hr.point, H.pen, d.move.x, d.move.y, d.move.z, std::sqrt(dot(d.move, d.move)), (int)d.apply);
                if (d.apply || (!std::getenv("DH_DEPEN_FAITHFUL") && dot(d.move, d.move) < 16.f)) { pos = pos + d.move; rb.com = pos; }
            } else { advance(h * rem * H.frac); rem *= 1.f - H.frac; }
            Axes Ah = axes(); syncInertia(Ah);
            V3 vb = rb.vel; float vnB = dot(H.normal, vb);
            float jn = contactResponse(rb, H.point, H.normal, Ah.r, hr.point);
            if (trace && traceOn) { V3 cr = cross(H.normal, Ah.r); std::fprintf(trace, "it=%d pt=%d surf=%u tri=%u frac=%.4f pen=%.3f n=(%.2f %.2f %.2f) aniso=%d vn=%.1f |Jn|=%.1f v=(%.2f %.2f %.2f)->(%.2f %.2f %.2f) |v|=%.2f->%.2f\n", it, hr.point, (unsigned)H.surface, (unsigned)H.tri, H.frac, H.pen, H.normal.x, H.normal.y, H.normal.z, dot(cr, cr) > 0.5f, vnB, jn, vb.x, vb.y, vb.z, rb.vel.x, rb.vel.y, rb.vel.z, std::sqrt(dot(vb, vb)), std::sqrt(dot(rb.vel, rb.vel))); }
        }
        if (rem > 1e-4f) advance(h * rem);                         // el resto del paso sin comprobar (el solape que quede lo corrige el paso siguiente)
        Axes Ae = axes(); float wpe = dot(rb.omega, Ae.r); wpe = std::fmax(-8.f, std::fmin(8.f, wpe));
        rb.omega = Ae.r * wpe + V3{0, 1, 0} * rb.omega.y;           // balanceo bloqueado (hipótesis)
        if (hitGround) { coyote = 0.1f; airTime = 0; } else { coyote = std::fmax(0.f, coyote - h); airTime += h; }
        tF = fHit ? 0.1f : std::fmax(0.f, tF - h); tR = rHit ? 0.1f : std::fmax(0.f, tR - h); grounded = coyote > 0.f; wheelF = tF > 0.f; wheelR = tR > 0.f;
    }
};

// Reproduce la verja de salida cerrada de la malla estática (superficie 0x681D, collision.md "Demo"): se quita para poder salir de la rejilla (hipótesis: el juego la abre al empezar).
inline void openStartGate(Ground& g, uint16_t surface = 0x681D) {
    std::vector<Ground::Tri> t; for (const auto& q : g.tris()) if (q.surface != surface) t.push_back(q);
    g.set(std::move(t));
}

// Carrera: moto + puertas + último punto bueno para reiniciar (como el reinicio al último punto bueno del modo previo de dhview; el reinicio del juego real no está estudiado: hipótesis).
struct Run {
    Bike bike; const Gates* gates = nullptr; Gates::State gs; double t = 0; unsigned respawns = 0;
    V3 goodPos; float goodHeading = 0; double goodT = 0, lastSave = 0;
    void start(const Ground& g, V3 p, float heading) {
        auto gi = g.groundQuery(p.x, p.y + 10.f, p.z, bike.P.radius, 66.f);
        V3 q{p.x, (gi.hit ? gi.height : p.y) + 3.8f, p.z}; bike.place(q, heading); goodPos = q; goodHeading = heading; gs = Gates::State(); t = 0; respawns = 0; lastSave = 0; goodT = 0;
    }
    void respawn() { bike.place(goodPos + V3{0, 0.5f, 0}, goodHeading); respawns++; }
    int update(const Ground& g, const BikeInput& in, float dt) {
        bike.step(g, in, dt); t += dt;
        if (bike.grounded && bike.wheelF && bike.wheelR && bike.groundN.y > 0.7f && bike.speed() > 2.f && t - lastSave > 0.5) { goodPos = bike.pos; goodHeading = bike.heading(); lastSave = t; }
        int ev = gates ? gates->update(gs, bike.pos, (float)t) : 0;
        // META (hipótesis, corrige la de gates.hpp): el plano 8052 de ALP2 queda ~3 puntos de la línea ANTES de la puerta 28 y con la normal hacia atrás (dist > 0 antes, < 0 después), así que no se puede cruzar tras la última puerta;
        // se toma la última puerta del recorrido como línea de meta (docs/formats/bike-physics.md, "Meta").
        if (gates && !gs.finished && gs.counter == gates->size() && gates->size() > 0) { gs.finished = true; gs.finishTime = (float)t; }
        if (bike.pos.y < g.boundsMin().y - 100.f) respawn();
        return ev;
    }
};
