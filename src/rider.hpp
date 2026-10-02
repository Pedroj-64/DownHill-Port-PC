// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Piloto: esqueleto (nodos tipo 35/17 del NGP del nivel) + piel de 3 huesos por vértice. Sin OpenGL. Mismo algoritmo que tools/rider_skel.py + tools/rider_pose.py
// (docs/formats/rider.md); cada lectura comprueba límites (el NGP viene del disco del usuario).
#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <tuple>
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
    std::vector<Mat> worldMatrices(const float* pose, size_t nPose) const {
        std::vector<Mat> world(j.size());
        for (size_t i = 0; i < j.size(); i++) {
            const Joint& q = j[i]; float e[3];
            for (int k = 0; k < 3; k++) e[k] = (q.chan[k] >= 0 && (size_t)q.chan[k] < nPose) ? pose[q.chan[k]] : 0.f;
            Mat l = euler(-e[0], -e[1], -e[2]);
            if (q.mirror) l = mul(l, q.bindRot);
            l[12] = q.loc[0]; l[13] = q.loc[1]; l[14] = q.loc[2];
            if (q.transFromPose) for (int k = 0; k < 3; k++) if (q.tchan[k] >= 0 && (size_t)q.tchan[k] < nPose) l[12 + k] = pose[q.tchan[k]];
            world[i] = q.parent < 0 ? l : mul(l, world[q.parent]);
        }
        return world;
    }
    std::vector<std::array<float, 3>> worldPositions(const float* pose, size_t nPose) const {
        const auto world = worldMatrices(pose, nPose);
        std::vector<std::array<float, 3>> out(world.size());
        for (size_t i = 0; i < world.size(); i++) out[i] = {world[i][12], world[i][13], world[i][14]};
        return out;
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

struct IKResult { float error = 0; int iterations = 0; bool reachable = false; };
inline float distance3(const std::array<float, 3>& a, const std::array<float, 3>& b) {
    const float x = a[0] - b[0], y = a[1] - b[1], z = a[2] - b[2];
    return std::sqrt(x * x + y * y + z * z);
}

// Solver numérico sobre la cinemática directa real: usa los canales Euler del
// joint, la negación y bindRot de worldMatrices(). No recorta la distancia del
// objetivo: un objetivo inalcanzable conserva su error residual.
inline IKResult solveTarget(const Skeleton& sk, std::array<float, 40>& pose, int effector,
                            const std::array<float, 3>& target, const std::array<int, 4>& channels,
                            const std::array<float, 4>& lower = {-3.1415927f, -3.1415927f, -3.1415927f, -3.1415927f},
                            const std::array<float, 4>& upper = {3.1415927f, 3.1415927f, 3.1415927f, 3.1415927f},
                            int maxIterations = 80) {
    if (effector < 0 || (size_t)effector >= sk.j.size()) return {0, 0, false};
    constexpr float eps = 1e-4f, damping = 1e-3f;
    IKResult result;
    for (int it = 0; it < maxIterations; it++) {
        const auto p = sk.worldPositions(pose.data(), pose.size());
        const float base[3] = {target[0] - p[effector][0], target[1] - p[effector][1], target[2] - p[effector][2]};
        float jtJ[4][4] = {}, jtR[4] = {};
        float J[4][3] = {};
        for (int c = 0; c < 4; c++) {
            auto trial = pose; trial[channels[c]] += eps; const auto q = sk.worldPositions(trial.data(), trial.size());
            for (int k = 0; k < 3; k++) J[c][k] = (q[effector][k] - p[effector][k]) / eps;
        }
        for (int a = 0; a < 4; a++) { jtR[a] = 0; for (int k = 0; k < 3; k++) jtR[a] += J[a][k] * base[k]; for (int b = 0; b < 4; b++) { jtJ[a][b] = damping; for (int k = 0; k < 3; k++) jtJ[a][b] += J[a][k] * J[b][k]; } }
        for (int col = 0; col < 4; col++) {
            int pivot = col; for (int row = col + 1; row < 4; row++) if (std::fabs(jtJ[row][col]) > std::fabs(jtJ[pivot][col])) pivot = row;
            if (std::fabs(jtJ[pivot][col]) < 1e-8f) continue;
            for (int k = col; k < 4; k++) std::swap(jtJ[col][k], jtJ[pivot][k]); std::swap(jtR[col], jtR[pivot]);
            const float div = jtJ[col][col]; for (int k = col; k < 4; k++) jtJ[col][k] /= div; jtR[col] /= div;
            for (int row = 0; row < 4; row++) if (row != col) { const float f = jtJ[row][col]; for (int k = col; k < 4; k++) jtJ[row][k] -= f * jtJ[col][k]; jtR[row] -= f * jtR[col]; }
        }
        float step2 = 0; for (int c = 0; c < 4; c++) { pose[channels[c]] = std::clamp(pose[channels[c]] + jtR[c], lower[c], upper[c]); step2 += jtR[c] * jtR[c]; }
        result.iterations = it + 1; if (step2 < 1e-10f) break;
    }
    const auto p = sk.worldPositions(pose.data(), pose.size()); result.error = distance3(p[effector], target); result.reachable = result.error < 1e-3f; return result;
}

inline std::array<float, 3> bikeToModel(const std::array<float, 3>& anchor, const std::array<float, 3>& pelvis,
                                         const std::array<float, 3>& rootModel) {
    // Ambas convenciones son X derecha, Y adelante, Z arriba: R bici->modelo = I.
    return {rootModel[0] + anchor[0] - pelvis[0], rootModel[1] + anchor[1] - pelvis[1], rootModel[2] + anchor[2] - pelvis[2]};
}

struct ApproxResult {
    std::array<float, 40> pose{};
    std::array<float, 4> errors{}; // muñeca R/L, tobillo R/L en unidades del juego
    std::array<IKResult, 4> ik{};
    std::array<float, 3> pelvis{};
    float totalError = 0;
    bool optimized = false;
};

inline ApproxResult approxPose(const Skeleton& sk, float phase, const std::array<float, 3>& pelvisBike,
                               float cadence = 5.5f, bool optimizePelvis = true) {
    ApproxResult result; result.pelvis = pelvisBike;
    const auto bind = sk.worldPositions(result.pose.data(), result.pose.size());
    const std::array<float, 3> rootModel = bind.empty() ? std::array<float, 3>{} : bind[0];
    const std::array<float, 3> hands[2] = {{1.035f, 0.791f, 0.978f}, {-1.024f, 0.790f, 0.978f}};
    const float crank = phase * cadence;
    const std::array<float, 3> feet[2] = {{0.488f, -0.28f + 0.488f * std::cos(crank), -1.025f + 0.488f * std::sin(crank)},
                                          {-0.488f, -0.28f - 0.488f * std::cos(crank), -1.025f - 0.488f * std::sin(crank)}};
    const std::array<float, 4> hingeLow = {-3.1415927f, -3.1415927f, -3.1415927f, 0.f};
    const std::array<float, 4> hingeHigh = {3.1415927f, 3.1415927f, 3.1415927f, 3.1415927f};
    auto solveAll = [&](std::array<float, 40>& pose, const std::array<float, 3>& pelvis) {
        std::array<IKResult, 4> ik{};
        auto solve = [&](int slot, const std::array<float, 3>& target, int eff, const std::array<int, 4>& ch) {
            ik[slot] = solveTarget(sk, pose, eff, target, ch, hingeLow, hingeHigh);
        };
        solve(0, bikeToModel(hands[0], pelvis, rootModel), 5, {12, 13, 14, 15});
        solve(1, bikeToModel(hands[1], pelvis, rootModel), 8, {19, 20, 21, 22});
        solve(2, bikeToModel(feet[0], pelvis, rootModel), 11, {26, 27, 28, 29});
        solve(3, bikeToModel(feet[1], pelvis, rootModel), 14, {33, 34, 35, 36});
        const auto pos = sk.worldPositions(pose.data(), pose.size());
        const std::array<float, 4> errors = {distance3(pos[5], bikeToModel(hands[0], pelvis, rootModel)),
                                             distance3(pos[8], bikeToModel(hands[1], pelvis, rootModel)),
                                             distance3(pos[11], bikeToModel(feet[0], pelvis, rootModel)),
                                             distance3(pos[14], bikeToModel(feet[1], pelvis, rootModel))};
        float total = 0; for (float e : errors) total += e;
        return std::tuple<std::array<float, 4>, float, std::array<IKResult, 4>>{errors, total, ik};
    };
    // HIPÓTESIS H4: torso y cadencia; la búsqueda finita minimiza el error,
    // pero no afirma que estos valores procedan del ELF.
    result.pose[6] = -0.18f; result.pose[7] = -0.10f;
    auto score = solveAll(result.pose, result.pelvis);
    for (int pass = 0; pass < (optimizePelvis ? 4 : 1); pass++) {
        bool changed = false;
        for (int channel : {6, 7, 8}) {
            for (float delta : {-0.05f, 0.05f}) {
                auto candidatePose = result.pose; candidatePose[channel] += delta;
                auto candidateScore = solveAll(candidatePose, result.pelvis);
                if (std::get<1>(candidateScore) < std::get<1>(score)) { result.pose = candidatePose; score = candidateScore; changed = true; }
            }
        }
        if (optimizePelvis) for (int axis = 0; axis < 3; axis++) for (float delta : {-0.05f, 0.05f}) {
            auto candidatePelvis = result.pelvis; candidatePelvis[axis] += delta;
            auto candidatePose = result.pose; auto candidateScore = solveAll(candidatePose, candidatePelvis);
            if (std::get<1>(candidateScore) < std::get<1>(score)) { result.pose = candidatePose; result.pelvis = candidatePelvis; score = candidateScore; changed = true; result.optimized = true; }
        }
        if (!changed) break;
    }
    result.errors = std::get<0>(score); result.totalError = std::get<1>(score); result.ik = std::get<2>(score);
    return result;
}

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
    uint32_t n; std::memcpy(&n, raw.data() + 4, 4); if (raw.size() < 8 + (size_t)n * 16) return false;
    out.resize(n);
    for (uint32_t i = 0; i < n; i++) { const uint8_t* p = raw.data() + 8 + (size_t)i * 16; for (int k = 0; k < 3; k++) out[i].bone[k] = p[k]; std::memcpy(out[i].w, p + 4, 12); }
    return true;
}
}  // namespace rider
