// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Lector nativo del .NGP (primer paso del loader, docs/native-loader-plan.md): malla de colisión (nodos 0x2A). Mismo algoritmo que tools/collision.py.
// Los punteros del NGP son direcciones de carga: offset = ptr - kBase (base 0xA00000, la de las tablas .PTR).
#pragma once
#include <cstdint>
#include <array>
#include <cmath>
#include <cstring>
#include <set>
#include <utility>
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

// Triángulos de un nodo 0x2A en o (FUN_00216780 rama 0x2A, FUN_00219970); `m` = matriz 4x4 fila-por-vector (traslación en m[12..14]) o nullptr (sin transformar).
inline bool node42Tris(const Reader& r, size_t o, const double* m, std::vector<ColTri>& out) {
    const std::vector<uint8_t>& data = r.d; uint16_t cnt = r.u16(o + 4); size_t table;
    if (cnt < 1 || cnt > 64 || !r.ptr(o + 8, table)) return false;
    size_t parts[64];
    for (int i = 0; i < cnt; i++) if (!r.ptr(o + 0xc + 4 * i, parts[i])) return false;
    for (int i = 0; i < cnt; i++) {
        size_t pt = parts[i], vp; uint16_t nt = r.u16(pt + 4), nn = r.u16(pt + 6);
        if (!r.ptr(pt, vp)) return false;
        size_t tb = pt + 0x18 + (size_t)nn * 14;
        if (tb + (size_t)nt * 4 > data.size()) return false;
        for (uint16_t k = 0; k < nt; k++) {
            const uint8_t* t = data.data() + tb + 4 * k; ColTri c;
            for (int j = 0; j < 3; j++) {
                size_t vo = vp + 12 * (size_t)t[j]; if (vo + 12 > data.size()) return false;
                float v[3]; std::memcpy(v, data.data() + vo, 12);
                for (int a = 0; a < 3; a++) c.v[3 * j + a] = m ? (float)((double)v[0] * m[a] + (double)v[1] * m[4 + a] + (double)v[2] * m[8 + a] + m[12 + a]) : v[a];
            }
            size_t so = table + 2 * (size_t)t[3]; if (so + 2 > data.size()) return false; std::memcpy(&c.surface, data.data() + so, 2);
            out.push_back(c);
        }
    }
    return true;
}

// Todos los nodos 0x2A (cabecera exacta 42, 1..64 partes, punteros dentro del archivo) SIN transformar: un triángulo-juego por nodo (export antiguo; espacio local, ver collisionTrisInstanced).
inline bool collisionTris(const std::vector<uint8_t>& data, std::vector<ColTri>& out) {
    Reader r{data};
    for (size_t o = 0; o + 0x20 <= data.size(); o += 4) {
        if (r.u32(o) != 42) continue;
        uint16_t cnt = r.u16(o + 4); size_t q;
        if (cnt < 1 || cnt > 64 || !r.ptr(o + 8, q)) continue;
        bool ok = true; for (int i = 0; i < cnt && ok; i++) ok = r.ptr(o + 0xc + 4 * i, q);
        if (!ok) continue;
        if (!node42Tris(r, o, nullptr, out)) return false;
    }
    return true;
}

// Matriz local de un nodo de la cadena (tipo 3 = traslación en +0x10, tipo 4 = matriz en +0x10 con traslación en la fila 3, como tools/scene.py: local; tipo 30 = escala f32x3 en +0x20 y
// traslación f32x3 en +0x10, v' = v*s + t: hipótesis, tools/collision.py: local_inst). false si no es de transformación.
inline bool localMatrix(const Reader& r, size_t o, double m[16]) {
    uint32_t t = r.u32(o) & 0x3f; if (t != 3 && t != 4 && t != 30) return false;
    if (o + 0x10 + 64 > r.d.size()) return false;
    float f[16]; std::memcpy(f, r.d.data() + o + 0x10, t == 4 ? 64 : 12);
    for (int i = 0; i < 16; i++) m[i] = (i % 5 == 0) ? 1.0 : 0.0;
    if (t == 3) { m[12] = f[0]; m[13] = f[1]; m[14] = f[2]; }
    else if (t == 30) { float sc[3]; std::memcpy(sc, r.d.data() + o + 0x20, 12); m[0] = sc[0]; m[5] = sc[1]; m[10] = sc[2]; m[12] = f[0]; m[13] = f[1]; m[14] = f[2]; }
    else { for (int i = 0; i < 16; i++) m[i] = f[i]; m[3] = m[7] = m[11] = 0; m[15] = 1; }
    return true;
}

// Colisión estática con TODAS las instancias de nodos 0x2A colocadas en el mundo (mismo recorrido que tools/collision.py: find_instances + instance_tris, docs/formats/collision.md).
// Registro de objeto de la rejilla: AABB (6 f32), u32 0 en +0x18, u8 n en +0x1E, n punteros desde +0x20 (cadena raíz->hoja); la hoja 0x2A está en espacio local y su matriz es el
// producto de los nodos 3/4 de la cadena (m = local · m_padre). Sin duplicados (hoja, matriz). Los nodos 0x2A sin registro no los coloca nada y se descartan.
inline bool collisionTrisInstanced(const std::vector<uint8_t>& data, std::vector<ColTri>& out) {
    Reader r{data}; const size_t L = data.size(); std::set<std::pair<size_t, std::array<long long, 16>>> seen; std::set<size_t> leaves;
    for (size_t o = 0; o + 0x24 <= L; o += 4) {
        if (r.u32(o + 0x18) != 0) continue;
        unsigned cnt = data[o + 0x1e]; if (cnt < 1 || o + 0x20 + 4 * (size_t)cnt > L) continue;
        size_t ps[255]; bool ok = true;
        for (unsigned i = 0; i < cnt && ok; i++) ok = r.ptr(o + 0x20 + 4 * i, ps[i]);
        if (!ok || (r.u32(ps[cnt - 1]) & 0x3f) != 42) continue;
        float bb[6]; std::memcpy(bb, data.data() + o, 24);
        for (float v : bb) if (!std::isfinite(v) || std::fabs(v) >= 1e6f) ok = false;
        if (!ok || bb[0] > bb[3] || bb[1] > bb[4] || bb[2] > bb[5]) continue;
        double m[16]; for (int i = 0; i < 16; i++) m[i] = (i % 5 == 0) ? 1.0 : 0.0;
        for (unsigned i = 0; i + 1 < cnt; i++) {
            double l[16], p[16]; if (!localMatrix(r, ps[i], l)) continue;
            for (int a = 0; a < 4; a++) for (int c = 0; c < 4; c++) { double s = 0; for (int k = 0; k < 4; k++) s += l[4 * a + k] * m[4 * k + c]; p[4 * a + c] = s; }
            std::memcpy(m, p, sizeof m);
        }
        std::array<long long, 16> key; for (int i = 0; i < 16; i++) key[i] = std::llround(m[i] * 1e4);
        leaves.insert(ps[cnt - 1]);
        if (!seen.insert({ps[cnt - 1], key}).second) continue;
        if (!node42Tris(r, ps[cnt - 1], m, out)) return false;
    }
    // nodos 0x2A sin registro: se conservan sin transformar si su centroide está a >= 50 u del origen (tools/collision.py: ORPHAN_MIN)
    for (size_t o = 0; o + 0x20 <= L; o += 4) {
        if (r.u32(o) != 42 || leaves.count(o)) continue;
        uint16_t cnt = r.u16(o + 4); size_t q;
        if (cnt < 1 || cnt > 64 || !r.ptr(o + 8, q)) continue;
        bool ok = true; for (int i = 0; i < cnt && ok; i++) ok = r.ptr(o + 0xc + 4 * i, q);
        if (!ok) continue;
        std::vector<ColTri> tmp; if (!node42Tris(r, o, nullptr, tmp)) return false;
        if (tmp.empty()) continue;
        double c[3] = {0, 0, 0}; for (const ColTri& tr : tmp) for (int a = 0; a < 3; a++) c[a] += ((double)tr.v[a] + tr.v[3 + a] + tr.v[6 + a]) / 3.0;
        for (double& v : c) v /= (double)tmp.size();
        if (std::sqrt(c[0] * c[0] + c[1] * c[1] + c[2] * c[2]) >= 50.0) out.insert(out.end(), tmp.begin(), tmp.end());
    }
    return true;
}
}  // namespace ngp
