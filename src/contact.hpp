// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Respuesta de contacto de la moto, portada del motor: impulso de fricción FUN_00238648, impulso normal FUN_002384B8, aplicación de impulsos FUN_00237D28/FUN_00237CF0 y
// la rutina que los combina FUN_00134630 (decomp/phys1.c, phys2.c, integ.c). Funciones puras sin SDL/GL. Convención de vectores del motor: filas (v·M), y arriba o z arriba da igual.
// Lo no verificado va marcado `hipótesis`. Sin portar: integrador de posición/rotación FUN_00238818 (usa FUN_00227820/00227988/002278E8/00228568/002389B8, sin decompilar) y
// el bucle de sub-pasos de FUN_001340D8 (FUN_00238BC8/00238C60 guardan/restauran estado, no decompiladas).
#pragma once
#include "ground.hpp"

// Cuerpo rígido (struct del motor en rider+0x6420+0x10, offsets relativos a él). Sólo los campos que usan estas funciones.
struct RigidBody {
    float invMass = 1.f;        // +0x00 (FUN_002384B8: `fVar2 = *param_2` en el denominador; FUN_00237CF0: velocidad = momento * *param_1)
    V3 com;                     // +0x50 centro de masas (FUN_00237D28: r = punto - com)
    V3 linMom;                  // +0x60 momento lineal
    V3 angMom;                  // +0x70 momento angular
    float iInv[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};   // +0x80/+0x90/+0xA0 inversa del tensor de inercia en mundo, 3 filas de 16 B (aquí 3x3 fila a fila; simétrica)
    V3 vel;                     // +0xC0 velocidad
    V3 omega;                   // +0xD0 velocidad angular
    float restitution = 0.f;    // módulo de física +0x114 (FUN_00134630 -> FUN_002384B8 param_1)
    float friction = 0.f;       // módulo de física +0x118 (FUN_00134630: fVar7)
};

// FUN_002275F0: v * M con M = 3 filas. Se lee como out_i = fila_i . v; el motor lo hace con la receta vmul + vaddabc/vmaddbc (lanas exactas: hipótesis), idéntica para una M simétrica.
inline V3 mulInertia(const float m[9], V3 v) {
    return {m[0] * v.x + m[1] * v.y + m[2] * v.z, m[3] * v.x + m[4] * v.y + m[5] * v.z, m[6] * v.x + m[7] * v.y + m[8] * v.z};
}

// Velocidad del punto de contacto respecto a la superficie: (omega x r) + vel - surfVel con r = punto - com (FUN_00238648 / FUN_002384B8, primeras líneas).
inline V3 contactVelocity(const RigidBody& b, V3 point, V3 surfVel) { return cross(b.omega, point - b.com) + b.vel - surfVel; }

// Denominador de ambos impulsos: invMass + d . ((I^-1 (r x d)) x r) para la dirección unitaria d (FUN_002384B8 con d = normal, FUN_00238648 con d = tangente unitaria).
inline float impulseDenominator(const RigidBody& b, V3 point, V3 d) {
    V3 r = point - b.com; return b.invMass + dot(d, cross(mulInertia(b.iInv, cross(r, d)), r));
}

// FUN_00238648: impulso de FRICCIÓN. vn = N . vrel; si vn > -0.001 -> impulso 0. Si no: t = vrel - N*vn (velocidad tangencial), t^ = t/|t|,
// impulso = -t^ * |t| / impulseDenominator(t^)  (corregido 2026-10-04 con el desensamblado: antes se usaba -t^/K, sin el factor |t|; el lane w guarda |t|/K; se escala luego por el coeficiente
// en FUN_00134630, así la fricción resta `coef` x la velocidad tangencial por contacto). |t| = 0 no está protegido en el motor (división 0/0): aquí devuelve 0 (hipótesis: no ocurre con vn < -0.001 salvo caída exacta).
inline V3 frictionImpulse(const RigidBody& b, V3 point, V3 normal, V3 surfVel = {}) {
    V3 vr = contactVelocity(b, point, surfVel); float vn = dot(normal, vr);
    if (-0.001f < vn) return {};
    V3 t = vr - normal * vn; float l = std::sqrt(dot(t, t)); if (l == 0.f) return {};
    t = t * (1.f / l);
    return t * (-l / impulseDenominator(b, point, t));          // FUN_00238648 0x2387d4-0x2387f4: out.xyz = -t^ * |t| / K (|t| guardado en sp+0x3c por 0x238750): anula TODA la velocidad tangencial (adherencia)
}

// FUN_002384B8: impulso NORMAL con restitución e = body+0x114: j = -(e+1)*vn / denominador(N), impulso = N*j; si vn > -0.001 -> 0.
inline V3 normalImpulse(float e, const RigidBody& b, V3 point, V3 normal, V3 surfVel = {}) {
    float vn = dot(normal, contactVelocity(b, point, surfVel));
    if (-0.001f < vn) return {};
    return normal * (-(e + 1.f) * vn / impulseDenominator(b, point, normal));
}

// FUN_00237D28 (+FUN_00237CF0): aplica el impulso J en `point`: linMom += J, vel = linMom*invMass, angMom += (point-com) x J, omega = I^-1 * angMom (FUN_002275F0 sobre +0x70 -> +0xD0).
// Los argumentos de FUN_00237CF0 se infieren (el decompilador los perdió; hipótesis débil): (cuerpo, J).
inline void applyImpulse(RigidBody& b, V3 j, V3 point) {
    b.linMom = b.linMom + j; b.vel = b.linMom * b.invMass;
    b.angMom = b.angMom + cross(point - b.com, j); b.omega = mulInertia(b.iInv, b.angMom);
}

// FUN_00134630 (la respuesta; FUN_001344D0 sólo la llama): `pointIndex` = índice del punto de contacto del impacto (FUN_001340D8 lo calcula por la dirección del impacto); el punto 2 multiplica
// la fricción por 1.25 (`param_3 == 2`). `axis0` = fila 0 de la matriz del nodo del cuerpo (node+0x40). Pasos: (1) fricción * coef; (2) f = N x axis0; si |f|^2 > 0.5 se quita del impulso su componente
// a lo largo de f^ (fricción anisótropa); (3) aplicar; (4) impulso normal con la velocidad ya actualizada; (5) aplicar. Devuelve |impulso normal| (el motor lo compara con 400.0 y con un contador
// DAT_002C5348 para lanzar efectos de polvo/sonido FUN_001B9A50 y fija el contador +0x1D0 = 0; esos efectos no se portan).
inline float contactResponse(RigidBody& b, V3 point, V3 normal, V3 axis0, int pointIndex, V3 surfVel = {}) {
    float mu = b.friction * (pointIndex == 2 ? 1.25f : 1.f);
    V3 fi = frictionImpulse(b, point, normal, surfVel) * mu;
    V3 f = cross(normal, axis0); float l2 = dot(f, f);
    if (0.5f < l2) { f = f * (1.f / std::sqrt(l2)); fi = fi - f * dot(fi, f); }
    applyImpulse(b, fi, point);
    V3 ni = normalImpulse(b.restitution, b, point, normal, surfVel);
    applyImpulse(b, ni, point);
    return std::sqrt(dot(ni, ni));
}
