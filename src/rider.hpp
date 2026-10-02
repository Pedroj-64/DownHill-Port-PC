// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Piloto: esqueleto (nodos tipo 35/17 del NGP del nivel) + piel de 3 huesos por vértice. Sin OpenGL. Mismo algoritmo que tools/rider_skel.py + tools/rider_pose.py
// (docs/formats/rider.md); cada lectura comprueba límites (el NGP viene del disco del usuario).
#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace rider {
inline constexpr uint32_t kBase = 0xA00000;     // los punteros guardados valen offset + kBase (como en ngp.hpp)
inline constexpr int kMaxJoints = 32;
using Mat = std::array<float, 16>;               // 4x4 por filas, vector fila (v * M), traslación en m[12..14]
inline Mat identity() { Mat m{}; m[0] = m[5] = m[10] = m[15] = 1; return m; }
inline Mat mul(const Mat& a, const Mat& b) { Mat r{}; for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) { float s = 0; for (int k = 0; k < 4; k++) s += a[4*i+k] * b[4*k+j]; r[4*i+j] = s; } return r; }

// FUN_00227da8: filas en memoria = Rx(a)Ry(b)Rz(c) en convención columna (orden de los ángulos: ver docs/formats/rider.md)
inline Mat euler(float a, float b, float c) {
    float sa = std::sin(a), ca = std::cos(a), sb = std::sin(b), cb = std::cos(b), sc = std::sin(c), cc = std::cos(c);
    Mat m = identity();
    m[0] = cb*cc;            m[1] = -cb*sc;           m[2] = sb;
    m[4] = sa*sb*cc + ca*sc; m[5] = -sa*sb*sc + ca*cc; m[6] = -sa*cb;
    m[8] = -ca*sb*cc + sa*sc; m[9] = ca*sb*sc + sa*cc; m[10] = ca*cb;
    return m;
}

struct Joint { int parent = -1; int chan[3] = {-1, -1, -1}; float loc[3] = {0, 0, 0}; Mat invBind = identity(); Mat bindRot = identity(); bool mirror = false; bool transFromPose = false; int tchan[3] = {-1, -1, -1}; };   // flags&1: loc = pose[tchan] (FUN_0021ad00; el piloto no lo usa)
struct Skeleton {
    std::vector<Joint> j;                        // en orden de paleta = preorden del árbol (id de hueso del vértice / 4)
    // Lee el árbol desde el nodo raíz (offset en el NGP del nivel). Devuelve false ante cualquier puntero/recuento fuera de rango.
    bool load(const std::vector<uint8_t>& d, size_t root) {
        j.clear(); return rec(d, root, -1, 0);
    }
    // Matrices de piel (inv_bind * mundo) por hueso para una pose (pose[canal] = ángulo en rad, tamaño >= 40; los ausentes valen 0)
    std::vector<Mat> skin(const float* pose, size_t nPose) const {
        std::vector<Mat> world(j.size()), out(j.size());
        for (size_t i = 0; i < j.size(); i++) {
            const Joint& q = j[i]; float e[3];
            for (int k = 0; k < 3; k++) e[k] = (q.chan[k] >= 0 && (size_t)q.chan[k] < nPose) ? pose[q.chan[k]] : 0.f;
            Mat l = euler(-e[0], -e[1], -e[2]);
            if (q.mirror) l = mul(l, q.bindRot);                  // marco espejado de la cadena izquierda (flags & 0x20000)
            l[12] = q.loc[0]; l[13] = q.loc[1]; l[14] = q.loc[2];
            if (q.transFromPose) for (int k = 0; k < 3; k++) if (q.tchan[k] >= 0 && (size_t)q.tchan[k] < nPose) l[12 + k] = pose[q.tchan[k]];
            world[i] = q.parent < 0 ? l : mul(l, world[q.parent]);
            out[i] = mul(q.invBind, world[i]);
        }
        return out;
    }
private:
    static bool u32(const std::vector<uint8_t>& d, size_t o, uint32_t& v) { if (o + 4 > d.size()) return false; std::memcpy(&v, d.data() + o, 4); return true; }
    static bool rf(const std::vector<uint8_t>& d, size_t o, float* f, int n) { if (o + 4 * (size_t)n > d.size()) return false; std::memcpy(f, d.data() + o, 4 * (size_t)n); return true; }
    bool rec(const std::vector<uint8_t>& d, size_t node, int parent, int depth) {
        uint32_t type, tp, nch, flags; if (depth > 16 || j.size() >= (size_t)kMaxJoints || !u32(d, node, type) || type != 0x23 || !u32(d, node + 0xc, tp) || tp < kBase) return false;
        size_t t = tp - kBase; if (t + 0xe0 > d.size() || !u32(d, t + 4, nch) || nch > 8 || !u32(d, t + 0xc0, flags)) return false;
        Joint q; q.parent = parent; q.mirror = flags & 0x20000; q.transFromPose = flags & 1;
        uint16_t ch[8]; if (t + 0x10 + 16 > d.size()) return false; std::memcpy(ch, d.data() + t + 0x10, 16);
        for (int k = 0; k < 3; k++) q.chan[k] = ch[1 + k] == 0xffff ? -1 : ch[1 + k];
        for (int k = 0; k < 3; k++) q.tchan[k] = ch[4 + k] == 0xffff ? -1 : ch[4 + k];
        if (!rf(d, node + 0x10, q.invBind.data(), 16) || !rf(d, t + 0x30, q.loc, 3)) return false;
        for (int r = 0; r < 3; r++) if (!rf(d, t + 0x80 + 16*r, &q.bindRot[4*r], 3)) return false;
        int me = (int)j.size(); j.push_back(q);
        for (uint32_t c = 0; c < nch; c++) { uint32_t cp; if (!u32(d, t + 0xcc + 4*c, cp) || cp < kBase || !rec(d, cp - kBase, me, depth + 1)) return false; }
        return true;
    }
};

struct SkinVertex { uint8_t bone[3]; float w[3]; };   // sidecar .skin de tools/extract_model.py (DHSK)
// Posa vértices (x,y,z en el espacio del modelo, 10 f32 por vértice como el .mdl) en sitio: v' = sum w_i * v * piel[hueso_i]
inline void applySkin(const std::vector<Mat>& skin, const std::vector<SkinVertex>& sv, const std::vector<float>& bindPos, std::vector<float>& mv) {
    for (size_t i = 0; i < sv.size() && 10 * i + 2 < mv.size() && 3 * i + 2 < bindPos.size(); i++) {
        const float* p = &bindPos[3*i]; float o[3] = {0, 0, 0};
        for (int k = 0; k < 3; k++) {
            if (sv[i].bone[k] >= skin.size()) continue; float w = sv[i].w[k] < 0 ? 0 : sv[i].w[k]; const Mat& m = skin[sv[i].bone[k]];
            for (int c = 0; c < 3; c++) o[c] += w * (p[0]*m[c] + p[1]*m[4+c] + p[2]*m[8+c] + m[12+c]);
        }
        mv[10*i] = o[0]; mv[10*i+1] = o[1]; mv[10*i+2] = o[2];
    }
}
inline bool parseSkin(const std::vector<uint8_t>& raw, std::vector<SkinVertex>& out) {
    if (raw.size() < 8 || std::memcmp(raw.data(), "DHSK", 4)) return false;
    uint32_t n; std::memcpy(&n, raw.data() + 4, 4); if (raw.size() < 8 + (size_t)n * 20) return false;
    out.resize(n);
    for (uint32_t i = 0; i < n; i++) { const uint8_t* p = raw.data() + 8 + (size_t)i * 20; for (int k = 0; k < 3; k++) out[i].bone[k] = p[k]; std::memcpy(out[i].w, p + 4, 12); }
    return true;
}
}  // namespace rider
