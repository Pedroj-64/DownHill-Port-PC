// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Esqueleto sintético de 2 huesos (mismo caso que tests/test_rider.py): pose de referencia = identidad; un giro de 90 grados mueve el hijo; entradas truncadas se rechazan.
#include "../src/rider.hpp"
#include <cstdio>
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
    std::puts("rider_test OK"); return 0;
}
