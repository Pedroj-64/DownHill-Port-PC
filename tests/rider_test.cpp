// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Esqueleto sintético de 2 huesos (mismo caso que tests/test_rider.py): pose de referencia = identidad; un giro de 90 grados mueve el hijo; entradas truncadas se rechazan.
#include "../src/rider.hpp"
#include <cstdio>
#include <fstream>
using namespace rider;
static void joint(std::vector<uint8_t>& b, size_t node, size_t table, uint16_t idx, float loc[3], float world[3], uint16_t c1, std::vector<size_t> kids) {
    Mat inv = identity(); inv[12] = -world[0]; inv[13] = -world[1]; inv[14] = -world[2];
    uint32_t hdr[4] = {0x23, 1, (uint32_t)idx, (uint32_t)(kBase + table)}; std::memcpy(&b[node], hdr, 16); std::memcpy(&b[node + 0x10], inv.data(), 64);
    uint32_t t[4] = {0x11, (uint32_t)kids.size(), 0, 0x3f800000}; std::memcpy(&b[table], t, 16);
    uint16_t ch[8] = {0xffff, 0xffff, 0xffff, c1, 0xffff, 0xffff, 0xffff, 0xffff}; std::memcpy(&b[table + 0x10], ch, 16);
    std::memcpy(&b[table + 0x30], loc, 12);
    for (int r = 0; r < 3; r++) { float row[3] = {0, 0, 0}; row[r] = 1; std::memcpy(&b[table + 0x80 + 16*r], row, 12); }
    uint32_t fl = 0x300; std::memcpy(&b[table + 0xc0], &fl, 4);
    for (size_t i = 0; i < kids.size(); i++) { uint32_t p = (uint32_t)(kBase + kids[i]); std::memcpy(&b[table + 0xcc + 4*i], &p, 4); }
}
int main() {
    std::vector<uint8_t> b(0x500, 0); float l0[3] = {0, 0, 2}, w0[3] = {0, 0, 2}, l1[3] = {1, 0, 0}, w1[3] = {1, 0, 2};
    joint(b, 0x100, 0x1a0, 0, l0, w0, 2, {0x300}); joint(b, 0x300, 0x360, 1, l1, w1, 0xffff, {});
    Skeleton s; if (!s.load(b, 0x100) || s.j.size() != 2) { std::puts("FAIL load"); return 1; }
    float pose[8] = {0}; auto S = s.skin(pose, 8);
    for (auto& m : S) for (int i = 0; i < 16; i++) if (std::fabs(m[i] - identity()[i]) > 1e-5f) { std::puts("FAIL bind identity"); return 1; }
    pose[2] = 1.5707963f; S = s.skin(pose, 8);
    float v[3] = {1, 0, 2}, o[3]; for (int c = 0; c < 3; c++) o[c] = v[0]*S[1][c] + v[1]*S[1][4+c] + v[2]*S[1][8+c] + S[1][12+c];
    float d = std::sqrt((o[0]-0)*(o[0]-0) + (o[1]-0)*(o[1]-0) + (o[2]-2)*(o[2]-2));
    if (std::fabs(d - 1.f) > 1e-4f || std::fabs(o[0] - 1.f) < 0.5f) { std::puts("FAIL rotation"); return 1; }
    std::vector<uint8_t> cut(b.begin(), b.begin() + 0x200); Skeleton s2; if (s2.load(cut, 0x100)) { std::puts("FAIL truncated accepted"); return 1; }
    float zeroPose[8] = {};
    auto wp = s.worldPositions(zeroPose, 8);
    if (wp.size() != 2 || std::fabs(wp[1][0] - 1.f) > 1e-5f || std::fabs(wp[1][2] - 2.f) > 1e-5f) { std::puts("FAIL world positions"); return 1; }
    std::array<float, 40> ik{};
    IKResult reachable = solveTarget(s, ik, 1, {0.f, -1.f, 2.f}, {2, 3, 4, 5});
    if (!reachable.reachable || reachable.error >= 1e-3f) { std::puts("FAIL reachable IK"); return 1; }
    IKResult unreachable = solveTarget(s, ik, 1, {100.f, 100.f, 100.f}, {2, 3, 4, 5});
    if (unreachable.reachable || !(unreachable.error > 1.f)) { std::puts("FAIL unreachable IK"); return 1; }
    if (bikeToModel({2.f, 3.f, 4.f}, {1.f, 1.f, 1.f}, {10.f, 20.f, 30.f}) != std::array<float, 3>{11.f, 22.f, 33.f}) { std::puts("FAIL bike/model frame"); return 1; }

    Skeleton leg;
    leg.j.resize(3);
    leg.j[0].chan[0] = 0; leg.j[0].loc[0] = 0; leg.j[0].loc[1] = 0;
    leg.j[1].parent = 0; leg.j[1].chan[0] = 1; leg.j[1].loc[1] = 1;
    leg.j[2].parent = 1; leg.j[2].chan[0] = 2; leg.j[2].loc[1] = 1;
    std::array<float, 40> legPose{};
    const auto legTarget = std::array<float, 3>{0.f, 1.7320508f, 1.f};
    IKResult legIk = solveTarget(leg, legPose, 2, legTarget, {0, 1, 2, 3},
                                 {-3.1415927f, -3.1415927f, -3.1415927f, 0.f},
                                 {3.1415927f, 3.1415927f, 3.1415927f, 3.1415927f});
    if (!legIk.reachable || legIk.error >= 1e-3f) { std::puts("FAIL synthetic leg IK"); return 1; }
    const HingeRange syntheticHinge = forwardHingeRange(leg, 2, 2);
    if (syntheticHinge.positiveDirection <= 0.f || syntheticHinge.lower > 0.f || syntheticHinge.upper < 1.f) {
        std::puts("FAIL synthetic forward hinge range"); return 1;
    }

    std::ifstream ngp("unpacked/LVL/ALP2.NGP", std::ios::binary);
    if (ngp) {
        std::vector<uint8_t> raw((std::istreambuf_iterator<char>(ngp)), {});
        Skeleton real;
        if (!real.load(raw, 0x9e2f40)) { std::puts("FAIL ALP2 skeleton load"); return 1; }
        ApproxResult unoptimized = approxPose(real, 0.f, {0.f, 0.f, 0.f}, 5.5f, false);
        ApproxResult approx = approxPose(real, 0.f, {0.f, 0.f, 0.f}, 5.5f, true);
        constexpr float qualityThreshold = 0.05f;
        const std::array<float, 3> footOffset = {0.f, -0.62f, 0.37f}; // HIPÓTESIS medida: tobillo (-0.11, +0.37) respecto a bola (+0.51).
        std::puts("ALP2 H4 experimental approximation (opt-in DH_RIDER_POSE=approx), not the game pose");
        const CycleResult cycleCenter = approxCycle(real, {0.f, 0.f, 0.f}, 5.5f);
        const CycleResult cycleFoot = approxCycle(real, {0.f, 0.f, 0.f}, 5.5f, footOffset);
        const char* cycleNames[4] = {"wrist+X", "wrist-X", "ankle+X", "ankle-X"};
        const auto assertCycleBound = [](const char* label, float value, float low, float high, const char* cause) {
            if (value < low || value > high) {
                std::printf("FAIL cycle bound %s=%.6f expected [%.6f, %.6f], cause: %s\n", label, value, low, high, cause);
                std::exit(1);
            }
        };
        for (const auto& cycle : {cycleCenter, cycleFoot}) {
            std::printf("ALP2 cycle offset=(%.3f,%.3f,%.3f) pelvis=(%.6f,%.6f,%.6f) rotation=(%.6f,%.6f,%.6f) evaluations=%zu\n",
                        cycle.footOffset[0], cycle.footOffset[1], cycle.footOffset[2],
                        cycle.pelvis[0], cycle.pelvis[1], cycle.pelvis[2],
                        cycle.pelvisRotation[0], cycle.pelvisRotation[1], cycle.pelvisRotation[2],
                        cycle.evaluations);
            for (int phaseIndex = 0; phaseIndex < 8; phaseIndex++) {
                std::printf("ALP2 cycle phase %d", phaseIndex);
                for (int i = 0; i < 4; i++) std::printf(" %s=%.6f", cycleNames[i], cycle.errors[phaseIndex][i]);
                std::puts("");
            }
            for (int i = 0; i < 4; i++) std::printf("ALP2 cycle worst %s=%.6f\n", cycleNames[i], cycle.maxError[i]);
        }
        assertCycleBound("center wrist+X", cycleCenter.maxError[0], 0.65f, 0.80f, "shared pelvis conflict");
        assertCycleBound("center wrist-X", cycleCenter.maxError[1], 0.34f, 0.43f, "shared pelvis conflict");
        assertCycleBound("center ankle+X", cycleCenter.maxError[2], 0.45f, 0.58f, "shared pelvis conflict");
        assertCycleBound("center ankle-X", cycleCenter.maxError[3], 0.45f, 0.58f, "shared pelvis conflict");
        assertCycleBound("foot wrist+X", cycleFoot.maxError[0], 0.12f, 0.20f, "foot offset plus shared pelvis conflict");
        assertCycleBound("foot wrist-X", cycleFoot.maxError[1], 0.58f, 0.72f, "foot offset plus shared pelvis conflict");
        assertCycleBound("foot ankle+X", cycleFoot.maxError[2], 0.28f, 0.40f, "foot offset plus shared pelvis conflict");
        assertCycleBound("foot ankle-X", cycleFoot.maxError[3], 0.65f, 0.80f, "foot offset plus shared pelvis conflict");
        std::printf("ALP2 phase0 unoptimized %.6f %.6f %.6f %.6f; optimized %.6f %.6f %.6f %.6f pelvis %.6f %.6f %.6f evaluations %zu\n",
                    unoptimized.errors[0], unoptimized.errors[1], unoptimized.errors[2], unoptimized.errors[3],
                    approx.errors[0], approx.errors[1], approx.errors[2], approx.errors[3],
                    approx.pelvis[0], approx.pelvis[1], approx.pelvis[2], approx.evaluations);
        const char* names[4] = {"wristR", "wristL", "ankleR", "ankleL"};
        for (int i = 0; i < 4; i++) std::printf("ALP2 hinge %s range [%.6f, %.6f] forward-sign %.0f\n",
                                                  names[i], approx.hingeRanges[i].lower, approx.hingeRanges[i].upper,
                                                  approx.hingeRanges[i].positiveDirection);
        for (int phaseIndex = 0; phaseIndex < 8; phaseIndex++) {
            const float phase = 0.7853981634f * phaseIndex;
            const ApproxResult frame = approxPose(real, phase, {0.f, 0.f, 0.f}, 5.5f, true);
            bool quality = true;
            for (int i = 0; i < 4; i++) {
                quality &= frame.errors[i] < qualityThreshold;
                constexpr float upper[8][4] = {
                    {0.05f, 0.05f, 0.05f, 0.20f}, {0.05f, 0.05f, 0.20f, 0.05f},
                    {0.05f, 0.25f, 0.05f, 0.05f}, {0.05f, 0.15f, 0.20f, 0.05f},
                    {0.05f, 0.05f, 0.35f, 0.05f}, {0.05f, 0.70f, 0.05f, 0.05f},
                    {0.05f, 0.05f, 0.15f, 0.05f}, {0.35f, 0.15f, 0.05f, 0.05f}};
                if (!(frame.errors[i] <= upper[phaseIndex][i])) {
                    std::printf("FAIL ALP2 regression phase %d effector %s %.6f > %.6f\n",
                                phaseIndex, names[i], frame.errors[i], upper[phaseIndex][i]);
                    return 1;
                }
                std::printf("ALP2 phase %.6f %s error %.6f status %s pelvis %.6f %.6f %.6f evaluations %zu\n",
                            phase, names[i], frame.errors[i], frame.errors[i] < qualityThreshold ? "PASS" : "FAIL: residual after side/anatomical mapping",
                            frame.pelvis[0], frame.pelvis[1], frame.pelvis[2], frame.evaluations);
            }
            std::printf("ALP2 pelvis rotation %.6f %.6f %.6f\n",
                        frame.pelvisRotation[0], frame.pelvisRotation[1], frame.pelvisRotation[2]);
            std::printf("ALP2 phase %.6f stages coarse=%zu refine=%zu converged=%s change=%.6f\n",
                        phase, frame.coarseEvaluations, frame.refinementEvaluations,
                        frame.refinementConverged ? "yes" : "no", frame.refinementChange);
            const auto root = real.worldPositions(std::array<float, 40>{}.data(), 40)[0];
            const float crank = phase * 5.5f;
            const std::array<std::array<float, 3>, 4> targets = {{
                {root[0] + 1.035f - frame.pelvis[0], root[1] + 0.791f - frame.pelvis[1], root[2] + 0.978f - frame.pelvis[2]},
                {root[0] - 1.024f - frame.pelvis[0], root[1] + 0.790f - frame.pelvis[1], root[2] + 0.978f - frame.pelvis[2]},
                {root[0] + 0.488f - frame.pelvis[0], root[1] - 0.28f + 0.488f * std::cos(crank) - frame.pelvis[1], root[2] - 1.025f + 0.488f * std::sin(crank) - frame.pelvis[2]},
                {root[0] - 0.488f - frame.pelvis[0], root[1] - 0.28f - 0.488f * std::cos(crank) - frame.pelvis[1], root[2] - 1.025f - 0.488f * std::sin(crank) - frame.pelvis[2]}}};
            const std::array<std::array<int, 4>, 4> channels = {{{12,13,14,15}, {19,20,21,22}, {33,34,35,36}, {26,27,28,29}}};
            const std::array<int, 4> effectors = {5, 8, 14, 11};
            for (int i = 0; i < 4; i++) if (frame.errors[i] >= qualityThreshold) {
                const auto isolated = isolatedTargetSearch(real, effectors[i], channels[i], frame.hingeRanges[i], targets[i]);
                const auto pelvisSearch = isolatedPelvisSearch(real, effectors[i], channels[i], frame.hingeRanges[i],
                                                               i < 2 ? (i == 0 ? std::array<float, 3>{1.035f, 0.791f, 0.978f} : std::array<float, 3>{-1.024f, 0.790f, 0.978f})
                                                                     : (i == 2 ? std::array<float, 3>{0.488f, -0.28f, -1.025f} : std::array<float, 3>{-0.488f, -0.28f, -1.025f}),
                                                               root, {0.f, 0.f, 0.f});
                std::printf("ALP2 diagnosis phase %.6f %s isolated-best %.6f samples %zu cause %s\n",
                            phase, names[i], isolated.bestError, isolated.samples,
                            pelvisSearch.belowQuality ? "shared pelvis/torso conflict" : "infeasible over pelvis/torso range");
            }
            if (!quality) std::printf("ALP2 quality: FAIL at this phase, threshold %.3f u; residual is not an anchoring-distance failure and approximation is not game pose\n", qualityThreshold);
            if (!frame.refinementConverged || frame.refinementChange >= 1e-4f) {
                std::puts("FAIL refinement did not converge"); return 1;
            }
        }
        const std::array<float, 3> pelvis = approx.pelvis;
        const std::array<std::array<float, 3>, 4> anchors = {{
            {1.035f, 0.791f, 0.978f}, {-1.024f, 0.790f, 0.978f},
            {0.488f, -0.280f, -1.025f}, {-0.488f, -0.280f, -1.025f}}};
        const float armLength = 1.824f, legLength = 2.77f;
        for (int i = 0; i < 4; i++) {
            const float d = distance3(pelvis, anchors[i]);
            const float limit = i < 2 ? armLength : legLength;
            std::printf("ALP2 geometry %s distance %.6f limit %.6f status %s\n",
                        names[i], d, limit, d <= limit ? "PASS" : "KNOWN-FAIL");
        }
        for (const auto& chain : std::array<std::pair<int, int>, 2>{{{11, 26}, {14, 33}}}) {
            std::array<float, 40> zero{};
            const auto base = real.worldPositions(zero.data(), zero.size());
            zero[chain.second] = 0.8f;
            const auto flexed = real.worldPositions(zero.data(), zero.size());
            const float dy = flexed[chain.first][1] - base[chain.first][1];
            if (!(dy > 0.f)) { std::puts("FAIL measured hip forward flexion"); return 1; }
            std::printf("ALP2 hip channel %d effector %d +0.8 dy %.6f\n", chain.second, chain.first, dy);
        }
        for (int i = 0; i < 4; i++) {
            const HingeRange& range = approx.hingeRanges[i];
            if (i == 0 && !(range.lower <= 0.f && range.upper >= 1.f)) { std::puts("FAIL right elbow forward range"); return 1; }
            if (i == 1 && !(range.lower <= -1.f && range.upper >= 0.f)) { std::puts("FAIL left elbow forward range"); return 1; }
            if (i >= 2 && !(range.lower <= -1.f && range.upper >= 0.f)) { std::puts("FAIL knee backward range"); return 1; }
        }
    }
    std::puts("rider_test OK"); return 0;
}
