// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Paso 3 del loader nativo (docs/native-loader-plan.md): planos de control (kinds 8050-8052), rejillas de salida (8060-8068) y cadena de la línea de carrera del .PTS.
// Mismo algoritmo y MISMOS BYTES que tools/markers.py (write_gates, puntos de rejilla) y tools/pts_path.py --chain. Citas y hallazgos: docs/formats/markers.md (FUN_001A33D0, FUN_001A3480).
#pragma once
#include "ngp.hpp"
#include <algorithm>
#include <tuple>

namespace markers {
struct Node { uint32_t kind, idx, type; size_t off; bool operator<(const Node& o) const { return std::tie(kind, idx, type, off) < std::tie(o.kind, o.idx, o.type, o.off); } };

// Nodos con lo <= hdr>>18 < hi alcanzables desde las raíces (sin tabla tipo 7), ordenados como en markers.py walk_kinds.
inline std::vector<Node> walkKinds(const std::vector<uint8_t>& data, uint32_t lo, uint32_t hi) {
    ngp::Reader r{data}; ngp::SceneStats st; std::set<size_t> seen; std::vector<Node> out;
    if (data.size() < 8) return out;
    const uint32_t n = r.u32(0); if (n > (data.size() - 4) / 4) return out;
    std::vector<size_t> stack;
    for (uint32_t i = 0; i < n; i++) {
        size_t root; if (!r.ptr(4 + 4 * i, root) || (r.u32(root) & 0x3f) == 7) continue;
        struct Rec { static void go(const ngp::Reader& r, size_t o, int depth, std::set<size_t>& seen, uint32_t lo, uint32_t hi, std::vector<Node>& out) {
            if (depth > 40 || !seen.insert(o).second) return;
            const uint32_t h = r.u32(o); if (h >> 18 >= lo && h >> 18 < hi) out.push_back({h >> 18, (h >> 7) & 0x7ff, h & 0x3f, o});
            std::vector<size_t> ch; ngp::sceneChildren(r, o, ch); for (size_t c : ch) go(r, c, depth + 1, seen, lo, hi, out); } };
        Rec::go(r, root, 0, seen, lo, hi, out);
    }
    std::sort(out.begin(), out.end()); return out;
}

// .gates: u32 n; n * (u32 kind, u32 idx, f32 nx ny nz d) en espacio NGP (Z arriba): nodo tipo 11 cuyo puntero +4 apunta a un registro con el plano en +0x10.
inline std::vector<uint8_t> gatesFile(const std::vector<uint8_t>& data) {
    ngp::Reader r{data}; std::vector<uint8_t> out(4);
    uint32_t n = 0;
    for (const Node& k : walkKinds(data, 8050, 8053)) {
        size_t dst; if (k.type != 11 || !r.ptr(k.off + 4, dst) || dst + 0x20 > data.size()) continue;
        const size_t at = out.size(); out.resize(at + 24); std::memcpy(&out[at], &k.kind, 4); std::memcpy(&out[at + 4], &k.idx, 4); std::memcpy(&out[at + 8], data.data() + dst + 0x10, 16); n++;
    }
    std::memcpy(out.data(), &n, 4); return out;
}

inline int startSlotCount(uint32_t kind) { switch (kind) { case 8060: return 12; case 8061: return 4; case 8062: return 6; case 8063: return 6; case 8064: return 8; case 8065: return 6; case 8066: return 10; case 8068: return 2; default: return 0; } }

// FUN_001A3480: posiciones de una rejilla (tipo 3: traslación en +0x10; tipo 4: matriz 3x3 en +0x10/+0x20/+0x30 y traslación en +0x40). Cuenta en doble como Python y redondea a f32 al escribir.
inline std::vector<std::array<double, 3>> startSlots(const ngp::Reader& r, size_t o, uint32_t kind) {
    const int n = startSlotCount(kind); const uint32_t t = r.u32(o) & 0x3f; const bool wide = kind == 8064 || kind == 8065 || kind == 8066;
    double v[2][3] = {{3.0, 9.7, 4.0}, {-3.0, 9.7, 4.0}}; double w[2][3] = {{5.0, 3.0, 4.0}, {5.0, -3.0, 4.0}};
    double A[3], B[3]; const double(*src)[3] = wide ? v : w; for (int c = 0; c < 3; c++) { A[c] = src[0][c]; B[c] = src[1][c]; }
    float M[16] = {}; float T[3];
    if (t == 4) { for (int i = 0; i < 16; i++) { uint32_t u = r.u32(o + 0x10 + 4 * i); std::memcpy(&M[i], &u, 4); } for (int c = 0; c < 3; c++) T[c] = M[12 + c]; }
    else for (int c = 0; c < 3; c++) { uint32_t u = r.u32(o + 0x10 + 4 * c); std::memcpy(&T[c], &u, 4); }
    std::vector<std::array<double, 3>> out;
    for (int i = 0; i < n; i++) {
        const bool first = i < n / 2; double* p = first ? A : B; std::array<double, 3> q;
        for (int c = 0; c < 3; c++) {
            if (t == 4) { double s = 0; for (int k = 0; k < 3; k++) s += p[k] * (double)M[4 * k + c]; q[c] = s + (double)T[c]; }
            else q[c] = p[c] + (double)T[c];
        }
        out.push_back(q); p[wide ? 0 : 1] += first ? 6.0 : -6.0;
    }
    return out;
}
// Puntos de todas las rejillas (tipos 3/4) como f32 x,y,z consecutivos (NGP), en el orden de markers.py.
inline std::vector<uint8_t> startPointsFile(const std::vector<uint8_t>& data) {
    ngp::Reader r{data}; std::vector<uint8_t> out;
    for (const Node& k : walkKinds(data, 8060, 8069)) {
        if (k.type != 3 && k.type != 4) continue;
        for (const auto& q : startSlots(r, k.off, k.kind)) for (int c = 0; c < 3; c++) { float f = (float)q[c]; const size_t at = out.size(); out.resize(at + 4); std::memcpy(&out[at], &f, 4); }
    }
    return out;
}

// .PTS: 8 B de cabecera + N * 32 B (f32 x,y,z; i16 a0..a7; 4 B). Cadena desde el registro `start` siguiendo a6 (i16 en +0x1C) hasta repetir, como pts_path.py --chain.
inline std::vector<uint8_t> ptsChainFile(const std::vector<uint8_t>& d, int start) {
    std::vector<uint8_t> out; if (d.size() < 8) return out; const long N = (long)((d.size() - 8) / 32); std::vector<char> seen((size_t)N, 0);
    long i = start;
    while (i >= 0 && i < N && !seen[(size_t)i]) {
        seen[(size_t)i] = 1; const uint8_t* rec = d.data() + 8 + 32 * (size_t)i; const size_t at = out.size(); out.resize(at + 12); std::memcpy(&out[at], rec, 12);
        int16_t a6; std::memcpy(&a6, rec + 12 + 2 * 6, 2); i = a6;
    }
    return out;
}
}  // namespace markers
