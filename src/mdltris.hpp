// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Lee los triángulos de un .mdl (DHM2/DHM3, tools/extract_model.py) como Ground::Tri en Y arriba (el .mdl está en Z arriba). Sólo para herramientas de validación.
#pragma once
#include "ground.hpp"
inline bool mdlTris(const std::vector<uint8_t>& m, std::vector<Ground::Tri>& out, bool twoSided = false) {
    if (m.size() < 16 || (std::memcmp(m.data(), "DHM3", 4) && std::memcmp(m.data(), "DHM2", 4))) return false;
    uint32_t nt, nv, ni; std::memcpy(&nt, m.data() + 4, 4); std::memcpy(&nv, m.data() + 8, 4); std::memcpy(&ni, m.data() + 12, 4);
    size_t o = 16; for (uint32_t k = 0; k < nt; k++) { int w, h; if (o + 8 > m.size()) return false; std::memcpy(&w, m.data() + o, 4); std::memcpy(&h, m.data() + o + 4, 4); o += 8 + (size_t)w * h * 4; }
    if (o + (size_t)nv * 40 + (size_t)ni * 4 > m.size()) return false;
    const float* mv = (const float*)(m.data() + o); const uint32_t* mi = (const uint32_t*)(m.data() + o + (size_t)nv * 40);
    for (uint32_t t = 0; t + 2 < ni; t += 3) {
        Ground::Tri q; q.surface = 0;
        for (int k = 0; k < 3; k++) { if (mi[t + k] >= nv) return false; const float* p = mv + (size_t)mi[t + k] * 10; q.v[3*k] = p[0]; q.v[3*k+1] = p[2]; q.v[3*k+2] = -p[1]; }
        out.push_back(q);
        if (twoSided) { Ground::Tri r = q; for (int k = 0; k < 3; k++) { r.v[3 + k] = q.v[6 + k]; r.v[6 + k] = q.v[3 + k]; } out.push_back(r); }   // cara opuesta
    }
    return true;
}
