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
#include <limits>

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
struct HingeRange { float lower = -3.1415927f, upper = 3.1415927f, positiveDirection = 1.f; };
enum class AnatomicalDirection { Forward, Backward };
enum class FailureCause { None, Infeasible, LocalMinimum, SharedPelvis };
inline float distance3(const std::array<float, 3>& a, const std::array<float, 3>& b) {
    const float x = a[0] - b[0], y = a[1] - b[1], z = a[2] - b[2];
    return std::sqrt(x * x + y * y + z * z);
}

// Measures the signed forward (model +Y) displacement caused by increasing a
// hinge angle through the same FK path used by the solver. This avoids assuming
// that mirrored chains share the right-hand Euler sign.
inline HingeRange measureHingeRange(const Skeleton& sk, int effector, int channel,
                                    AnatomicalDirection direction, float flexion = 1.0f) {
    std::array<float, 40> zero{};
    if (effector < 0 || (size_t)effector >= sk.j.size() || channel < 0 || channel >= (int)zero.size()) return {};
    const auto base = sk.worldPositions(zero.data(), zero.size());
    auto trial = zero; trial[channel] = flexion;
    const auto moved = sk.worldPositions(trial.data(), trial.size());
    const float displacement = moved[effector][1] - base[effector][1];
    const float desired = direction == AnatomicalDirection::Forward ? 1.f : -1.f;
    if (displacement * desired >= 0.f) return {0.f, 3.1415927f, 1.f};
    return {-3.1415927f, 0.f, -1.f};
}
inline HingeRange forwardHingeRange(const Skeleton& sk, int effector, int channel, float flexion = 1.0f) {
    return measureHingeRange(sk, effector, channel, AnatomicalDirection::Forward, flexion);
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
    size_t evaluations = 0;
    size_t coarseEvaluations = 0;
    size_t refinementEvaluations = 0;
    bool refinementConverged = false;
    float refinementChange = 0;
    std::array<HingeRange, 4> hingeRanges{};
    std::array<float, 3> pelvisRotation{};
    std::array<FailureCause, 4> causes{};
};

struct IsolatedIKResult {
    float bestError = 0;
    size_t samples = 0;
    bool belowQuality = false;
};

inline IsolatedIKResult isolatedTargetSearch(const Skeleton& sk, int effector,
                                             const std::array<int, 4>& channels,
                                             const HingeRange& hinge, const std::array<float, 3>& target,
                                             const std::array<float, 3>& torsoRotation = {}) {
    IsolatedIKResult result{1e30f, 0, false};
    constexpr float quality = 0.05f;
    std::array<float, 40> pose{};
    pose[0] = torsoRotation[0]; pose[1] = torsoRotation[1]; pose[2] = torsoRotation[2];
    std::array<float, 40> bestPose{};
    for (int a = 0; a < 5; a++) for (int b = 0; b < 5; b++)
        for (int c = 0; c < 5; c++) for (int d = 0; d < 5; d++) {
            const float span = 3.1415927f * 2.f;
            pose[channels[0]] = -3.1415927f + span * a / 4.f;
            pose[channels[1]] = -3.1415927f + span * b / 4.f;
            pose[channels[2]] = -3.1415927f + span * c / 4.f;
            pose[channels[3]] = hinge.lower + (hinge.upper - hinge.lower) * d / 4.f;
            result.samples++;
            const auto p = sk.worldPositions(pose.data(), pose.size());
            const float error = distance3(p[effector], target);
            if (error < result.bestError) { result.bestError = error; bestPose = pose; }
        }
    const std::array<float, 4> low = {-3.1415927f, -3.1415927f, -3.1415927f, hinge.lower};
    const std::array<float, 4> high = {3.1415927f, 3.1415927f, 3.1415927f, hinge.upper};
    for (int seed = 0; seed < 8; seed++) {
        auto refined = bestPose;
        for (int i = 0; i < 4; i++) refined[channels[i]] =
            low[i] + (high[i] - low[i]) * ((seed * (i + 3)) % 8) / 7.f;
        const auto solved = solveTarget(sk, refined, effector, target, channels, low, high, 120);
        result.samples += solved.iterations;
        result.bestError = std::min(result.bestError, solved.error);
    }
    result.belowQuality = result.bestError < quality;
    return result;
}

inline IsolatedIKResult isolatedPelvisSearch(const Skeleton& sk, int effector,
                                             const std::array<int, 4>& channels,
                                             const HingeRange& hinge,
                                             const std::array<float, 3>& anchor,
                                             const std::array<float, 3>& rootModel,
                                             const std::array<float, 3>& origin) {
    IsolatedIKResult result{1e30f, 0, false};
    const std::array<float, 4> low = {-3.1415927f, -3.1415927f, -3.1415927f, hinge.lower};
    const std::array<float, 4> high = {3.1415927f, 3.1415927f, 3.1415927f, hinge.upper};
    for (int px = -2; px <= 2; px++) for (int py = -2; py <= 2; py++) for (int pz = -2; pz <= 2; pz++)
        for (int tx = -1; tx <= 1; tx++) for (int ty = -1; ty <= 1; ty++) for (int tz = -1; tz <= 1; tz++) {
            std::array<float, 40> pose{};
            pose[0] = 0.6f * tx; pose[1] = 0.6f * ty; pose[2] = 0.6f * tz;
            const std::array<float, 3> pelvis = {origin[0] + (float)px, origin[1] + (float)py, origin[2] + (float)pz};
            const std::array<float, 3> target = {rootModel[0] + anchor[0] - pelvis[0],
                                                 rootModel[1] + anchor[1] - pelvis[1],
                                                 rootModel[2] + anchor[2] - pelvis[2]};
            const auto solved = solveTarget(sk, pose, effector, target, channels, low, high, 80);
            result.samples += solved.iterations;
            result.bestError = std::min(result.bestError, solved.error);
        }
    result.belowQuality = result.bestError < 0.05f;
    return result;
}

inline ApproxResult approxPose(const Skeleton& sk, float phase, const std::array<float, 3>& pelvisBike,
                               float cadence = 5.5f, bool optimizePelvis = true,
                               const std::array<float, 3>& pelvisRotation = {},
                               const std::array<float, 3>& footOffset = {}) {
    ApproxResult result; result.pelvis = pelvisBike;
    result.pelvisRotation = pelvisRotation;
    const auto bind = sk.worldPositions(result.pose.data(), result.pose.size());
    const std::array<float, 3> rootModel = bind.empty() ? std::array<float, 3>{} : bind[0];
    const std::array<float, 3> hands[2] = {{1.035f, 0.791f, 0.978f}, {-1.024f, 0.790f, 0.978f}};
    const float crank = phase * cadence;
    const std::array<float, 3> feet[2] = {{0.488f, -0.28f + 0.488f * std::cos(crank), -1.025f + 0.488f * std::sin(crank)},
                                          {-0.488f, -0.28f - 0.488f * std::cos(crank), -1.025f - 0.488f * std::sin(crank)}};
    struct Chain { int effector; std::array<int, 4> channels; AnatomicalDirection direction; };
    const auto bindPositions = sk.worldPositions(result.pose.data(), result.pose.size());
    const auto bySide = [&](int positive, Chain a, Chain b) {
        return bindPositions[a.effector][0] * positive >= 0.f ? a : b;
    };
    const Chain armPositive = bySide(1, {5, {12, 13, 14, 15}, AnatomicalDirection::Forward},
                                     {8, {19, 20, 21, 22}, AnatomicalDirection::Forward});
    const Chain armNegative = armPositive.effector == 5
        ? Chain{8, {19, 20, 21, 22}, AnatomicalDirection::Forward}
        : Chain{5, {12, 13, 14, 15}, AnatomicalDirection::Forward};
    const Chain legPositive = bySide(1, {11, {26, 27, 28, 29}, AnatomicalDirection::Backward},
                                     {14, {33, 34, 35, 36}, AnatomicalDirection::Backward});
    const Chain legNegative = legPositive.effector == 11
        ? Chain{14, {33, 34, 35, 36}, AnatomicalDirection::Backward}
        : Chain{11, {26, 27, 28, 29}, AnatomicalDirection::Backward};
    const std::array<Chain, 4> chains = {armPositive, armNegative, legPositive, legNegative};
    std::array<HingeRange, 4> ranges{};
    for (size_t i = 0; i < chains.size(); i++)
        ranges[i] = measureHingeRange(sk, chains[i].effector, chains[i].channels[3], chains[i].direction);
    result.hingeRanges = ranges;
    const std::array<float, 4> reachLimits = {1.824f, 1.824f, 2.77f, 2.77f};
    const std::array<float, 3> geometryAnchors[4] = {
        hands[0], hands[1], feet[0], feet[1]};
    bool refining = false;
    auto solveAll = [&](std::array<float, 40>& pose, const std::array<float, 3>& pelvis) {
        std::array<IKResult, 4> ik{};
        auto solve = [&](int slot, const std::array<float, 3>& target, const Chain& chain) {
            const HingeRange& range = ranges[slot];
            const std::array<float, 4> low = {-3.1415927f, -3.1415927f, -3.1415927f, range.lower};
            const std::array<float, 4> high = {3.1415927f, 3.1415927f, 3.1415927f, range.upper};
            ik[slot] = solveTarget(sk, pose, chain.effector, target, chain.channels, low, high);
        };
        solve(0, bikeToModel(hands[0], pelvis, rootModel), armPositive);
        solve(1, bikeToModel(hands[1], pelvis, rootModel), armNegative);
        const auto footTargetR = std::array<float, 3>{feet[0][0] + footOffset[0], feet[0][1] + footOffset[1], feet[0][2] + footOffset[2]};
        const auto footTargetL = std::array<float, 3>{feet[1][0] + footOffset[0], feet[1][1] + footOffset[1], feet[1][2] + footOffset[2]};
        solve(2, bikeToModel(footTargetR, pelvis, rootModel), legPositive);
        solve(3, bikeToModel(footTargetL, pelvis, rootModel), legNegative);
        const auto pos = sk.worldPositions(pose.data(), pose.size());
        const std::array<float, 4> errors = {distance3(pos[armPositive.effector], bikeToModel(hands[0], pelvis, rootModel)),
                                             distance3(pos[armNegative.effector], bikeToModel(hands[1], pelvis, rootModel)),
                                             distance3(pos[legPositive.effector], bikeToModel(footTargetR, pelvis, rootModel)),
                                             distance3(pos[legNegative.effector], bikeToModel(footTargetL, pelvis, rootModel))};
        float total = 0; for (float e : errors) total += e;
        return std::tuple<std::array<float, 4>, float, std::array<IKResult, 4>>{errors, total, ik};
    };
    // HIPÓTESIS H4: torso y cadencia; ninguna de estas cifras procede del ELF.
    std::array<float, 3> bestTorso = {-0.18f, -0.10f, 0.f};
    std::array<float, 3> bestRotation = pelvisRotation;
    auto evaluate = [&](const std::array<float, 3>& pelvis, const std::array<float, 3>& torsoAngles,
                        const std::array<float, 3>& rotation) {
        result.evaluations++;
        if (refining) result.refinementEvaluations++; else result.coarseEvaluations++;
        std::array<float, 40> candidate{};
        candidate[0] = rotation[0]; candidate[1] = rotation[1]; candidate[2] = rotation[2];
        candidate[6] = torsoAngles[0]; candidate[7] = torsoAngles[1]; candidate[8] = torsoAngles[2];
        for (int i = 0; i < 4; i++) {
            if (distance3(pelvis, geometryAnchors[i]) > reachLimits[i]) {
                return std::tuple<std::array<float, 40>, std::array<float, 4>, float, std::array<IKResult, 4>>{
                    candidate, {distance3(pelvis, geometryAnchors[0]), distance3(pelvis, geometryAnchors[1]),
                                distance3(pelvis, geometryAnchors[2]), distance3(pelvis, geometryAnchors[3])},
                    1e6f, {}};
            }
        }
        auto score = solveAll(candidate, pelvis);
        return std::tuple<std::array<float, 40>, std::array<float, 4>, float, std::array<IKResult, 4>>{
            candidate, std::get<0>(score), std::get<1>(score), std::get<2>(score)};
    };
    auto score = evaluate(result.pelvis, bestTorso, bestRotation);
    if (optimizePelvis) {
        auto search = [&](float radius, float step, const std::array<float, 3>& origin) {
            std::array<float, 3> best = origin;
            auto bestScore = score;
            for (float x = -radius; x <= radius + step * 0.5f; x += step)
                for (float y = -radius; y <= radius + step * 0.5f; y += step)
                    for (float z = -radius; z <= radius + step * 0.5f; z += step) {
                        const std::array<float, 3> candidate = {origin[0] + x, origin[1] + y, origin[2] + z};
                        auto tested = evaluate(candidate, bestTorso, bestRotation);
                        if (std::get<2>(tested) < std::get<2>(bestScore)) { best = candidate; bestScore = std::move(tested); }
                    }
            return std::pair<std::array<float, 3>, decltype(score)>{best, std::move(bestScore)};
        };
        auto coarse = search(2.f, 1.f, pelvisBike);
        auto onEdge = [&](const std::array<float, 3>& p, float radius) {
            return std::fabs(std::fabs(p[0] - pelvisBike[0]) - radius) < 1e-4f ||
                   std::fabs(std::fabs(p[1] - pelvisBike[1]) - radius) < 1e-4f ||
                   std::fabs(std::fabs(p[2] - pelvisBike[2]) - radius) < 1e-4f;
        };
        if (onEdge(coarse.first, 2.f)) coarse = search(4.f, 1.f, pelvisBike);
        std::array<float, 3> bestPelvis = coarse.first;
        score = coarse.second;
        refining = true;
        for (float step : {0.25f, 0.05f}) {
            bool changed = true;
            while (changed) {
                changed = false;
                float largestChange = 0;
                for (int axis = 0; axis < 3; axis++) for (float delta : {-step, step}) {
                    auto candidatePelvis = bestPelvis; candidatePelvis[axis] += delta;
                    auto tested = evaluate(candidatePelvis, bestTorso, bestRotation);
                    if (std::get<2>(tested) < std::get<2>(score)) { bestPelvis = candidatePelvis; score = std::move(tested); changed = true; largestChange = std::max(largestChange, std::fabs(delta)); }
                }
                for (int axis = 0; axis < 3; axis++) for (float delta : {-step, step}) {
                    auto candidateTorso = bestTorso; candidateTorso[axis] += delta;
                    auto tested = evaluate(bestPelvis, candidateTorso, bestRotation);
                    if (std::get<2>(tested) < std::get<2>(score)) { bestTorso = candidateTorso; score = std::move(tested); changed = true; largestChange = std::max(largestChange, std::fabs(delta)); }
                }
                result.refinementChange = largestChange;
                if (!changed || largestChange < 1e-4f) { result.refinementConverged = true; break; }
            }
        }
        result.pelvis = bestPelvis;
        for (float rx = -0.6f; rx <= 0.6001f; rx += 0.3f)
            for (float ry = -0.6f; ry <= 0.6001f; ry += 0.3f)
                for (float rz = -0.6f; rz <= 0.6001f; rz += 0.3f) {
                    const std::array<float, 3> rotation = {rx, ry, rz};
                    auto tested = evaluate(bestPelvis, bestTorso, rotation);
                    if (std::get<2>(tested) < std::get<2>(score)) { bestRotation = rotation; score = std::move(tested); }
                }
        for (float step : {0.15f, 0.05f}) {
            bool changed = true;
            while (changed) {
                changed = false;
                float largestChange = 0;
                for (int axis = 0; axis < 3; axis++) for (float delta : {-step, step}) {
                    auto candidateRotation = bestRotation; candidateRotation[axis] += delta;
                    candidateRotation[axis] = std::clamp(candidateRotation[axis], -0.6f, 0.6f);
                    auto tested = evaluate(bestPelvis, bestTorso, candidateRotation);
                    if (std::get<2>(tested) < std::get<2>(score)) { bestRotation = candidateRotation; score = std::move(tested); changed = true; largestChange = std::max(largestChange, std::fabs(delta)); }
                }
                result.refinementChange = std::min(result.refinementChange, largestChange);
                if (!changed || largestChange < 1e-4f) break;
            }
        }
        result.pelvisRotation = bestRotation;
        result.optimized = true;
    } else {
        score = evaluate(result.pelvis, bestTorso, bestRotation);
    }
    result.pose = std::get<0>(score);
    result.errors = std::get<1>(score); result.totalError = std::get<2>(score); result.ik = std::get<3>(score);
    return result;
}

struct CycleResult {
    std::array<float, 3> pelvis{};
    std::array<float, 3> pelvisRotation{};
    std::array<float, 3> torso{};
    std::array<std::array<float, 4>, 8> errors{};
    std::array<float, 4> maxError{};
    size_t evaluations = 0;
    std::array<float, 3> footOffset{};
};

// Shared-cycle approximation: one pelvis/torso/rotation is selected for all
// crank phases; each frame may still solve its four limb channels.
inline CycleResult approxCycle(const Skeleton& sk, const std::array<float, 3>& origin,
                               float cadence = 5.5f,
                               const std::array<float, 3>& footOffset = {}) {
    CycleResult result; result.pelvis = origin; result.torso = {-0.18f, -0.10f, 0.f};
    result.footOffset = footOffset;
    auto evaluate = [&](const std::array<float, 3>& pelvis, const std::array<float, 3>& rotation) {
        std::array<float, 4> worst{};
        for (int phaseIndex = 0; phaseIndex < 8; phaseIndex++) {
            const auto frame = approxPose(sk, 0.7853981634f * phaseIndex, pelvis, cadence, false, rotation, footOffset);
            result.evaluations++;
            for (int i = 0; i < 4; i++) worst[i] = std::max(worst[i], frame.errors[i]);
        }
        return worst;
    };
    auto best = evaluate(result.pelvis, result.pelvisRotation);
    for (float x = -2.f; x <= 2.001f; x += 1.f)
        for (float y = -2.f; y <= 2.001f; y += 1.f)
            for (float z = -2.f; z <= 2.001f; z += 1.f)
                for (float rx = -0.6f; rx <= 0.6001f; rx += 0.6f)
                    for (float ry = -0.6f; ry <= 0.6001f; ry += 0.6f)
                        for (float rz = -0.6f; rz <= 0.6001f; rz += 0.6f) {
                            const std::array<float, 3> p = {origin[0] + x, origin[1] + y, origin[2] + z};
                            const std::array<float, 3> r = {rx, ry, rz};
                            const auto tested = evaluate(p, r);
                            if (*std::max_element(tested.begin(), tested.end()) <
                                *std::max_element(best.begin(), best.end())) {
                                result.pelvis = p; result.pelvisRotation = r; best = tested;
                            }
                        }
    result.maxError = best;
    for (int phaseIndex = 0; phaseIndex < 8; phaseIndex++)
        result.errors[phaseIndex] = approxPose(sk, 0.7853981634f * phaseIndex, result.pelvis, cadence, false, result.pelvisRotation, footOffset).errors;
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
