// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Ensamblado de la bici (cuadro + manillar/horquilla + 2 ruedas) a partir de piezas .mdl DHM2/DHM3: port de tools/assemble_bike.py, MISMOS BYTES. Cuentas en doble como Python; sin fma (-ffp-contract=off en CMake).
// Parámetros por defecto del Python: ruedas en Y = -1.60 (trasera) y +2.43 (delantera), Z = -1.84, manillar inclinado 0.436 rad, eje de salida del visor (x, z, -y).
#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace bikeasm {
struct Part { std::vector<std::tuple<uint32_t, uint32_t, std::vector<uint8_t>>> tex; std::vector<std::array<double, 10>> v; std::vector<uint32_t> i; };
inline bool loadPart(const std::vector<uint8_t>& b, Part& p) {
    if (b.size() < 16 || (std::memcmp(b.data(), "DHM2", 4) && std::memcmp(b.data(), "DHM3", 4))) return false;
    uint32_t nt, nv, ni; std::memcpy(&nt, b.data() + 4, 4); std::memcpy(&nv, b.data() + 8, 4); std::memcpy(&ni, b.data() + 12, 4); size_t o = 16;
    for (uint32_t k = 0; k < nt; k++) {
        if (o + 8 > b.size()) return false; uint32_t w, h; std::memcpy(&w, &b[o], 4); std::memcpy(&h, &b[o + 4], 4); const size_t n = (size_t)w * h * 4; if (o + 8 + n > b.size()) return false;
        p.tex.emplace_back(w, h, std::vector<uint8_t>(b.begin() + o + 8, b.begin() + o + 8 + n)); o += 8 + n;
    }
    if (o + (size_t)nv * 40 + (size_t)ni * 4 > b.size()) return false;
    for (uint32_t k = 0; k < nv; k++) { float f[10]; std::memcpy(f, &b[o + 40 * (size_t)k], 40); std::array<double, 10> r; for (int c = 0; c < 10; c++) r[c] = f[c]; p.v.push_back(r); }
    o += (size_t)nv * 40; p.i.resize(ni); std::memcpy(p.i.data(), &b[o], (size_t)ni * 4); return true;
}
struct Opts { double rear = -1.60, front = 2.43, hub = -1.84, head = 0.436; bool keepAxes = false; };
inline bool assemble(const std::vector<uint8_t>& frame, const std::vector<uint8_t>& bars, const std::vector<uint8_t>& wheel, const Opts& op, std::vector<uint8_t>& out) {
    Part pf, pb, pw; if (!loadPart(frame, pf) || !loadPart(bars, pb) || !loadPart(wheel, pw)) return false;
    std::vector<std::tuple<uint32_t, uint32_t, std::vector<uint8_t>>> T; std::vector<std::array<double, 10>> V; std::vector<uint32_t> I; const double FY = 0.13, FZ = -2.28;
    auto add = [&](const Part& p, double dx, double dy, double dz, bool dropLod, bool fork) {
        const size_t base = V.size(), tb = T.size(); T.insert(T.end(), p.tex.begin(), p.tex.end()); std::vector<uint32_t> idx = p.i;
        if (dropLod) {   // los cuadros traen un impostor plano de baja calidad que ocupa toda la caja: se descarta
            double ymin = 1e300, ymax = -1e300; for (const auto& r : p.v) { ymin = std::min(ymin, r[1]); ymax = std::max(ymax, r[1]); }
            std::vector<uint32_t> keep;
            for (size_t k = 0; k + 2 < idx.size(); k += 3) {
                double y0 = 1e300, y1 = -1e300, xm = -1e300; for (int j = 0; j < 3; j++) { const auto& r = p.v[idx[k + j]]; y0 = std::min(y0, r[1]); y1 = std::max(y1, r[1]); xm = std::max(xm, std::fabs(r[0])); }
                if (y1 - y0 > 0.9 * (ymax - ymin) && xm < 0.05) continue;
                keep.insert(keep.end(), idx.begin() + k, idx.begin() + k + 3);
            }
            idx = keep;
        }
        for (auto r : p.v) {
            if (fork) { const double ay = r[1] - FY, az = r[2] - FZ, c = std::cos(op.head), sn = std::sin(op.head); r[1] = op.front + ay * c - az * sn; r[2] = op.hub + ay * sn + az * c; }
            else { r[0] += dx; r[1] += dy; r[2] += dz; }
            if (r[9] >= 0) r[9] += (double)tb;
            V.push_back(r);
        }
        for (uint32_t k : idx) I.push_back((uint32_t)base + k);
    };
    add(pf, 0, 0, 0, true, false); add(pb, 0, 0, 0, false, true); add(pw, 0, op.rear, op.hub, false, false); add(pw, 0, op.front, op.hub, false, false);
    if (!op.keepAxes) for (auto& r : V) { const double x = r[0], y = r[1], z = r[2]; r[0] = x; r[1] = z; r[2] = -y; }
    out.assign({'D', 'H', 'M', '2'}); auto put = [&](const void* q, size_t n) { const uint8_t* b = (const uint8_t*)q; out.insert(out.end(), b, b + n); };
    const uint32_t nt = (uint32_t)T.size(), nv = (uint32_t)V.size(), ni = (uint32_t)I.size(); put(&nt, 4); put(&nv, 4); put(&ni, 4);
    for (auto& [w, h, px] : T) { put(&w, 4); put(&h, 4); put(px.data(), px.size()); }
    for (const auto& r : V) for (int c = 0; c < 10; c++) { const float f = (float)r[c]; put(&f, 4); }
    put(I.data(), I.size() * 4); return true;
}
}  // namespace bikeasm
