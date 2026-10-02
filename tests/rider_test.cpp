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

    std::ifstream ngp("unpacked/LVL/ALP2.NGP", std::ios::binary);
    if (ngp) {
        std::vector<uint8_t> raw((std::istreambuf_iterator<char>(ngp)), {});
        Skeleton real;
        if (!real.load(raw, 0x9e2f40)) { std::puts("FAIL ALP2 skeleton load"); return 1; }
        ApproxResult unoptimized = approxPose(real, 0.f, {0.f, 0.f, 0.f}, 5.5f, false);
        ApproxResult approx = approxPose(real, 0.f, {0.f, 0.f, 0.f}, 5.5f, true);
        constexpr float documentedThreshold = 3.0f; // u: diagnóstico de aproximación, no fidelidad al juego.
        for (float e : approx.errors) if (!(e < documentedThreshold)) { std::puts("FAIL ALP2 IK threshold"); return 1; }
        std::printf("ALP2 unoptimized errors %.6f %.6f %.6f %.6f; optimized errors %.6f %.6f %.6f %.6f pelvis %.6f %.6f %.6f\n",
                    unoptimized.errors[0], unoptimized.errors[1], unoptimized.errors[2], unoptimized.errors[3],
                    approx.errors[0], approx.errors[1], approx.errors[2], approx.errors[3],
                    approx.pelvis[0], approx.pelvis[1], approx.pelvis[2]);
    }
    std::puts("rider_test OK"); return 0;
}
