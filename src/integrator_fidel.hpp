// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Integrador fiel del cuerpo rígido de la moto (carril B del hito 3). Puro C++, sin SDL/GL. Doc: docs/formats/integrator.md, docs/formats/vu0-macro-ops.md.
// Todo sale del DESENSAMBLADO (Ghidra no decompila los COP2/VU0 macro): cada función cita su FUN_xxxx. 'hipótesis' = no verificado con datos vivos.
// Convención del juego: filas = vectores de la base (mundo = loc.x*fila0 + loc.y*fila1 + loc.z*fila2), matrices 4x4 de 16 B por fila; w sólo viaja por las copias.
// El orden de las sumas respeta los acumuladores del VU0 (ACC = a*x; ACC += b*y; r = ACC + c*z) para acercarse al redondeo del original;
// el VU0 no es IEEE (sin denormales, redondeo por truncamiento): bit-exactitud con el motor = hipótesis.
#pragma once
#include <cmath>
#include <utility>

namespace integ {

struct V4 { float x = 0, y = 0, z = 0, w = 0; };
struct M4 { V4 r[4]; };   // filas
inline float& at(V4& v, int i) { return i == 0 ? v.x : i == 1 ? v.y : v.z; }
inline float at(const V4& v, int i) { return i == 0 ? v.x : i == 1 ? v.y : v.z; }
inline M4 identity() { M4 m; m.r[0].x = m.r[1].y = m.r[2].z = m.r[3].w = 1; return m; }

// Campos del cuerpo rígido (offsets relativos al cuerpo = módulo de física + 0x10; FUN_001340D8 pasa iVar17+0x10 a FUN_00238818).
struct Body {
    float invMass = 0;      // +0x00: FUN_002389B8/FUN_00237CF0 multiplican el momento por este valor (nombre: hipótesis; 0.01 en las bicis medidas)
    V4 com;                 // +0x10: centro de masas en coordenadas locales (0,0,-0.85) (FUN_00238818 cola)
    V4 invInertia;          // +0x20: diagonal del tensor de inercia inverso (x,y,z) (FUN_002389B8 bucle)
    V4 pos;                 // +0x50: posición del centro de masas (FUN_00238818, FUN_00237AB0)
    V4 P;                   // +0x60: momento lineal (w=1) (FUN_00238818, FUN_00237CF0)
    V4 L;                   // +0x70: momento angular (w=1)
    M4 iw;                  // +0x80: "inercia inversa en mundo" (4x4); medido: SIEMPRE diagonal = invInertia (ver docs/formats/integrator.md)
    V4 vel;                 // +0xC0: velocidad = P * invMass (FUN_002389B8)
    V4 omega;               // +0xD0: velocidad angular = iw * L (FUN_002389B8 -> FUN_002275F0)
    V4 force;               // +0xE0: fuerza acumulada, se pone a (0,0,0,1) tras integrar (FUN_00237C78)
    V4 torque;              // +0xF0: torque acumulado, ídem
    // Nodo de escena al que se sincroniza el cuerpo: *(+0x34) posición (vec4), *(+0x38) matriz 4x4 (filas = ejes)
    V4 nodePos; M4 nodeRot = identity();
    // *(+0x3C)/*(+0x40)/*(+0x30): copia de la pose anterior y último dt (sólo si los dos punteros existen)
    bool keepPrev = false; V4 prevPos; M4 prevRot; float lastDt = 0;
};

// FUN_00237C78: fuerza y torque a (0,0,0,1) (sqc2 vf0).
inline void clearForces(Body& b) { b.force = V4{0, 0, 0, 1}; b.torque = V4{0, 0, 0, 1}; }

// FUN_002275F0(A, v, out): out.xyz = A*v (producto punto de cada fila con v); out.w = v.w. Orden de suma del VU0: x:(a+b)+c, y:(b+c)+a, z:(c+a)+b.
inline V4 matVec(const M4& A, const V4& v) {
    V4 o; o.w = v.w;
    o.x = (A.r[0].x * v.x + A.r[0].y * v.y) + A.r[0].z * v.z;
    o.y = (A.r[1].y * v.y + A.r[1].z * v.z) + A.r[1].x * v.x;
    o.z = (A.r[2].z * v.z + A.r[2].x * v.x) + A.r[2].y * v.y;
    return o;
}

// FUN_00227820(out, in, A): fila_i.xyz = A * fila_i.xyz para i=0..2 (A traspuesta por columnas con PEXTLW/PCPYLD/PCPYUD/PEXTUW), fila 3 y w sin cambios.
inline M4 mulRowsByA(const M4& in, const M4& A) {
    M4 o = in;
    for (int i = 0; i < 3; i++) {
        const V4& v = in.r[i];
        for (int j = 0; j < 3; j++) at(o.r[i], j) = (at(A.r[j], 0) * v.x + at(A.r[j], 1) * v.y) + at(A.r[j], 2) * v.z;   // ACC = col0*x; ACC += col1*y; + col2*z
    }
    return o;
}

// FUN_002277C8(out, a, b): fila_i.xyz = fila_i(a).x*b0 + .y*b1 + .z*b2 (vector fila por matriz b), w y fila 3 de a sin cambios.
inline M4 mulRowVecMat(const M4& a, const M4& b) {
    M4 o = a;
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) at(o.r[i], j) = (a.r[i].x * at(b.r[0], j) + a.r[i].y * at(b.r[1], j)) + a.r[i].z * at(b.r[2], j);
    return o;
}

// FUN_00238B70(body, out, omega): matriz antisimétrica [w]x (S*v = w x v) en los 9 floats xyz de 3 filas; el resto no se escribe (aquí 0).
inline M4 skew(const V4& w) {
    M4 s; s.r[0].y = -w.z; s.r[0].z = w.y; s.r[1].x = w.z; s.r[1].z = -w.x; s.r[2].x = -w.y; s.r[2].y = w.x; return s;
}

// FUN_00227988(out, in, k): fila_i.xyz *= k (i=0..2); w sin cambios; la fila 3 no se escribe.
inline void scaleRows(M4& m, float k) { for (int i = 0; i < 3; i++) { m.r[i].x *= k; m.r[i].y *= k; m.r[i].z *= k; } }

// FUN_002278E8(out, a, b): out[i][j] = a[i][j] + b[j][i] (3x3; b traspuesta); sólo escribe los 9 floats xyz.
inline void addTransposed(M4& out, const M4& a, const M4& b) {
    for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) at(out.r[i], j) = at(a.r[i], j) + at(b.r[j], i);
}

inline V4 cross(const V4& a, const V4& b) { return V4{a.y * b.z - b.y * a.z, a.z * b.x - b.z * a.x, a.x * b.y - b.x * a.y, 0}; }   // VOPMULA/VOPMSUB
inline void normalize3(V4& v) {   // mula/madda/madd (x²+y²+z²) y RSQRT.S (1/sqrt; el RSQRT del EE es aproximado: hipótesis)
    float n2 = v.x * v.x; n2 += v.y * v.y; n2 += v.z * v.z; float k = 1.0f / std::sqrt(n2); v.x *= k; v.y *= k; v.z *= k;
}
// FUN_00228568(R): reortonormaliza: r0 = N(r1 x r2); r1 = N(r2 x r0); r2 = N(r0 x r1). El w de cada fila se copia de la fila usada como primer operando
// (sqc2 guarda vf11 completo): r0.w = r1.w(antes), r1.w = r2.w(antes), r2.w = r0.w(nuevo).
inline void orthonormalize(M4& R) {
    V4 r0 = cross(R.r[1], R.r[2]); r0.w = R.r[1].w; normalize3(r0); R.r[0] = r0;
    V4 r1 = cross(R.r[2], R.r[0]); r1.w = R.r[2].w; normalize3(r1); R.r[1] = r1;
    V4 r2 = cross(R.r[0], R.r[1]); r2.w = R.r[0].w; normalize3(r2); R.r[2] = r2;
}

// FUN_002389B8: recalcula vel, iw y omega desde los momentos. vel = P*invMass (vmulx.xyz con f0 = body[0]);
// FUN_00227C88 deja la identidad en una pila; sp[i][j] = invInertia[i] * R[j][i]; iw = FUN_002277C8(sp, R) = sp*R; omega = FUN_002275F0(iw, L).
// Hallazgo: sp*R = D*R^T*R = D para R ortonormal -> el motor NO rota la inercia (medido en 16 pilotos: iw = diag y omega = D*L al error de float).
inline void updateDerived(Body& b) {
    b.vel.x = b.P.x * b.invMass; b.vel.y = b.P.y * b.invMass; b.vel.z = b.P.z * b.invMass; b.vel.w = b.P.w;
    M4 sp = identity();
    for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) at(sp.r[i], j) = at(b.invInertia, i) * at(b.nodeRot.r[j], i);
    b.iw = mulRowVecMat(sp, b.nodeRot);
    b.omega = matVec(b.iw, b.L);
}

// FUN_00237CF0(body, J): impulso lineal: P += J (xyz) y vel = P*invMass. (Argumentos antes perdidos por el decompilador: confirmados en el desensamblado.)
inline void addImpulse(Body& b, const V4& J) {
    b.P.x += J.x; b.P.y += J.y; b.P.z += J.z;
    b.vel.x = b.P.x * b.invMass; b.vel.y = b.P.y * b.invMass; b.vel.z = b.P.z * b.invMass;
}

// FUN_00237AB0(body, d): traslada nodo y centro de masas d (despenetración, FUN_001344F0).
inline void translate(Body& b, const V4& d) {
    b.nodePos.x += d.x; b.nodePos.y += d.y; b.nodePos.z += d.z; b.pos.x += d.x; b.pos.y += d.y; b.pos.z += d.z;
}

// FUN_00238BC8 / FUN_00238C60: copia el estado integrable a un búfer de 0xE0 B y lo restaura (nodo pos/matriz, +0x50, +0x60, +0x70, +0x80, +0xC0, +0xD0).
// NO incluye fuerza ni torque (+0xE0/+0xF0): tras restaurar, siguen a cero (FUN_00237C78 ya los limpió).
struct Snapshot { V4 nodePos; M4 nodeRot; V4 pos, P, L; M4 iw; V4 vel, omega; };
inline Snapshot save(const Body& b) { return Snapshot{b.nodePos, b.nodeRot, b.pos, b.P, b.L, b.iw, b.vel, b.omega}; }
inline void restore(Body& b, const Snapshot& s) {
    b.nodePos = s.nodePos; b.nodeRot = s.nodeRot; b.pos = s.pos; b.P = s.P; b.L = s.L; b.iw = s.iw; b.vel = s.vel; b.omega = s.omega;
}

// FUN_00238818(body, dt): un paso de integración (Euler explícito con la velocidad/omega ANTERIORES). Orden exacto del desensamblado:
//  0. dt <= 0 -> nada.  1. si existen *(+0x3C) y *(+0x40): guarda nodo pos y matriz y *(+0x30)=dt.
//  2. S = [omega]x ; S = S*R^T (FUN_00227820 con R = *(+0x38) antigua).  3. pos += dt*vel (vaddax/vmaddx.xyz).  4. S *= dt ; R[i][j] += S[j][i] ; orthonormalize(R).
//  5. P += dt*force ; L += dt*torque.  6. FUN_002389B8: vel, iw, omega nuevos.  7. nodePos = pos - R*com.
inline void integrate(Body& b, float dt) {
    if (dt <= 0.0f) return;
    if (b.keepPrev) { b.prevPos = b.nodePos; b.prevRot = b.nodeRot; b.lastDt = dt; }
    M4 S = skew(b.omega);
    S = mulRowsByA(S, b.nodeRot);
    b.pos.x = b.pos.x + b.vel.x * dt; b.pos.y = b.pos.y + b.vel.y * dt; b.pos.z = b.pos.z + b.vel.z * dt;
    scaleRows(S, dt);
    addTransposed(b.nodeRot, b.nodeRot, S);
    orthonormalize(b.nodeRot);
    b.P.x = b.P.x + b.force.x * dt; b.P.y = b.P.y + b.force.y * dt; b.P.z = b.P.z + b.force.z * dt;
    b.L.x = b.L.x + b.torque.x * dt; b.L.y = b.L.y + b.torque.y * dt; b.L.z = b.L.z + b.torque.z * dt;
    updateDerived(b);
    V4 c;   // com * R (vmulax/vmadday/vmaddz)
    for (int j = 0; j < 3; j++) at(c, j) = (b.com.x * at(b.nodeRot.r[0], j) + b.com.y * at(b.nodeRot.r[1], j)) + b.com.z * at(b.nodeRot.r[2], j);
    b.nodePos.x = b.pos.x - c.x; b.nodePos.y = b.pos.y - c.y; b.nodePos.z = b.pos.z - c.z; b.nodePos.w = b.pos.w;
}

// Un contacto candidato del barrido (estructura mínima; la geometría la pone el llamador, p. ej. Ground::sweepHits).
struct Hit { float frac = 0; int point = 0; };

// Bucle de sub-pasos de FUN_001340D8 (una vez por paso físico; el divisor del dt es el u32 en 0x2C85FC = gp-0x4B74 = 50 en los savestates PAL, FUN_0023F078).
// Hasta 2 iteraciones; en cada una: guarda el estado, integra el dt restante, limpia fuerzas (FUN_00237C78), barre los puntos de contacto de la pose inicial a la final (FUN_0021A908)
// y toma el primer impacto (FUN_00217450). Sin impacto: fin. 2ª iteración: se fuerza frac=-1 sin restaurar (despenetra). 1ª: restaura y, si frac<0, despenetra (FUN_001344F0);
// si no, por cada impacto en orden: integra dt*frac (con fuerza 0), llama a la respuesta (FUN_001344D0); si responde, rem -= rem*frac (acotado a >=0, hipótesis: el
// decompilador lo muestra como (int)f*(f>=0)) y pasa a la siguiente iteración; si ninguno responde, restaura e integra el dt completo.
//   sweep(start, end) -> impactos ordenados por frac (vacío = sin contacto);  depen(body, hits);  respond(body, hit) -> true si hubo respuesta.
template <class SweepFn, class DepenFn, class RespondFn>
inline int tick(Body& b, float dtTick, SweepFn sweep, DepenFn depen, RespondFn respond) {
    float rem = 1.0f; int it = 0;
    for (;;) {
        float dt = dtTick * rem; Snapshot snap = save(b); Body startBody = b;
        integrate(b, dt); clearForces(b);
        auto hits = sweep(startBody, b);
        if (hits.empty()) break;
        it++;
        if (it == 2) hits[0].frac = -1.0f; else restore(b, snap);
        if (hits[0].frac < 0.0f) depen(b, hits);
        else {
            bool responded = false;
            for (const Hit& h : hits) {
                integrate(b, dt * h.frac);
                if (respond(b, h)) { rem = rem - rem * h.frac; if (rem < 0.0f) rem = 0.0f; responded = true; break; }
                restore(b, snap);
            }
            if (!responded) integrate(b, dt);
        }
        if (rem <= 0.0f || it > 1) break;
    }
    return it;
}

}  // namespace integ
