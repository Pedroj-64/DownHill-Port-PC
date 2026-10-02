// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Lector nativo del .NGP (primer paso del loader, docs/native-loader-plan.md): malla de colisión (nodos 0x2A). Mismo algoritmo que tools/collision.py.
// Los punteros del NGP son direcciones de carga: offset = ptr - kBase (base 0xA00000, la de las tablas .PTR).
#pragma once
#include <cstdint>
#include <cstring>
#include <vector>

namespace ngp {
inline constexpr uint32_t kBase = 0xA00000;
struct ColTri { float v[9]; uint16_t surface; };   // espacio NGP (Z arriba)

struct Reader {
    const std::vector<uint8_t>& d;
    uint32_t u32(size_t o) const { uint32_t v = 0; if (o + 4 <= d.size()) std::memcpy(&v, d.data() + o, 4); return v; }
    uint16_t u16(size_t o) const { uint16_t v = 0; if (o + 2 <= d.size()) std::memcpy(&v, d.data() + o, 2); return v; }
    bool ptr(size_t o, size_t& out) const { uint32_t v = u32(o); if (v < kBase || v - kBase >= d.size()) return false; out = v - kBase; return true; }
};

// Todos los nodos 0x2A (cabecera exacta 42, 1..64 partes, punteros dentro del archivo) y sus triángulos (FUN_00216780 rama 0x2A, FUN_00219970).
inline bool collisionTris(const std::vector<uint8_t>& data, std::vector<ColTri>& out) {
    Reader r{data};
    for (size_t o = 0; o + 0x20 <= data.size(); o += 4) {
        if (r.u32(o) != 42) continue;
        uint16_t cnt = r.u16(o + 4); size_t table;
        if (cnt < 1 || cnt > 64 || !r.ptr(o + 8, table)) continue;
        size_t parts[64]; bool ok = true;
        for (int i = 0; i < cnt && ok; i++) ok = r.ptr(o + 0xc + 4 * i, parts[i]);
        if (!ok) continue;
        for (int i = 0; i < cnt; i++) {
            size_t pt = parts[i], vp; uint16_t nt = r.u16(pt + 4), nn = r.u16(pt + 6);
            if (!r.ptr(pt, vp)) return false;
            size_t tb = pt + 0x18 + (size_t)nn * 14;
            if (tb + (size_t)nt * 4 > data.size()) return false;
            for (uint16_t k = 0; k < nt; k++) {
                const uint8_t* t = data.data() + tb + 4 * k; ColTri c;
                for (int j = 0; j < 3; j++) { size_t vo = vp + 12 * (size_t)t[j]; if (vo + 12 > data.size()) return false; std::memcpy(c.v + 3 * j, data.data() + vo, 12); }
                size_t so = table + 2 * (size_t)t[3]; if (so + 2 > data.size()) return false; std::memcpy(&c.surface, data.data() + so, 2);
                out.push_back(c);
            }
        }
    }
    return true;
}
}  // namespace ngp
