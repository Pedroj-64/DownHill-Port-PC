// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Paso 5a del loader nativo (docs/native-loader-plan.md): geometría de modelos SUELTOS (bici, etc.: sin grafo útil, todo sin transformar). Mismo algoritmo y mismos bytes (.mdl DHM2) que tools/extract_model.py sin
// Owners: escaneo de bloques de vértices (UNPACK V3-32 precedido del comando 0x68/0x78), cadena VIF posterior (S-8 índices, ADC, V4-8/V4-5 color, V2-16 UV, MSCAL/MSCNT dispara las tiras) y materiales del PTR.
// VIF: tools/vif.py (iter_vif). Cuentas en doble y redondeo a f32 al escribir, como Python. Accesos acotados (entrada no fiable).
#pragma once
#include "tex.hpp"
#include <cmath>
#include <tuple>

namespace mesh {
struct VifCmd { size_t off; int kind; uint32_t imm, count; size_t size; };   // kind: 0 otro, 1 UNPACK V4-32, 2 S-8, 3 V4-8, 4 V4-5, 5 V2-16, 6 MS*, 7 UNPACK V3-32, 8 desconocido ('?'), 9 resto de UNPACK
// iter_vif de tools/vif.py: comandos desde off hasta end (cada uno consume 4 + size bytes).
template <class F> inline void iterVif(const std::vector<uint8_t>& d, size_t off, size_t end, F&& fn) {
    ngp::Reader r{d};
    while (off + 4 <= end) {
        const uint32_t w = r.u32(off), imm = w & 0xffff, num = (w >> 16) & 0xff, cmd = (w >> 24) & 0x7f; VifCmd c{off, 0, imm, 0, 0};
        if (cmd >= 0x60) {
            const uint32_t f = cmd & 0xf; int comps = 0, bits = 0;
            switch (f) { case 0: comps = 1; bits = 32; break; case 1: comps = 1; bits = 16; break; case 2: comps = 1; bits = 8; break; case 4: comps = 2; bits = 32; break; case 5: comps = 2; bits = 16; break; case 6: comps = 2; bits = 8; break;
                         case 8: comps = 3; bits = 32; break; case 9: comps = 3; bits = 16; break; case 0xA: comps = 3; bits = 8; break; case 0xC: comps = 4; bits = 32; break; case 0xD: comps = 4; bits = 16; break; case 0xE: comps = 4; bits = 8; break; case 0xF: comps = 1; bits = 16; break; default: break; }
            const uint32_t count = num ? num : 256; c.count = count; c.size = comps ? ((size_t)count * comps * bits + 31) / 32 * 4 : 0;
            c.kind = f == 0xC && bits == 32 ? 1 : f == 2 ? 2 : f == 0xE ? 3 : f == 0xF ? 4 : f == 5 ? 5 : f == 8 ? 7 : comps == 0 ? 9 : 9;   // 'UNPACK ?' (formatos 3, 7, 0xB) no empieza por '?': kind 9, sin tamaño
        } else {
            c.count = num; c.size = 0;
            switch (cmd) { case 0x20: c.size = 4; break; case 0x30: case 0x31: c.size = 16; break; default: break; }
            if (cmd == 0x4a) { c.count = num ? num : 256; c.size = (size_t)c.count * 8; }
            if (cmd == 0x50 || cmd == 0x51) c.size = (size_t)(imm ? imm : 65536) * 16;
            static const uint8_t known[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x10, 0x11, 0x13, 0x14, 0x15, 0x17, 0x20, 0x30, 0x31, 0x4a, 0x50, 0x51};
            bool k = false; for (uint8_t v : known) if (v == cmd) k = true;
            c.kind = !k ? 8 : (cmd == 0x14 || cmd == 0x15 || cmd == 0x17) ? 6 : 0;   // nombres que empiezan por 'MS': MSCAL, MSCALF, MSCNT (y MSKPATH3 = 0x06: también empieza por 'MS')
            if (cmd == 0x06) c.kind = 6;
        }
        if (!fn(c)) return;
        off += 4 + c.size;
    }
}

// Escanea un modelo y construye .mdl DHM2 (sin instancias). `variant` = 0.
inline bool extractStatic(const std::vector<uint8_t>& ngpD, const tex::Level& L, std::vector<uint8_t>& mdl) {
    ngp::Reader r{ngpD}; const size_t len = ngpD.size();
    std::vector<std::tuple<uint32_t, uint32_t, std::vector<uint8_t>>> textures; std::map<std::pair<uint32_t, uint32_t>, long> tindex;
    auto getTex = [&](const tex::Material& m) -> long {
        const auto key = std::make_pair(m.tid, m.cbp); auto it = tindex.find(key); if (it != tindex.end()) return it->second;
        uint32_t w, h; std::vector<uint8_t> px; long id = -1;
        if (tex::makeTexture(L, m, w, h, px)) { id = (long)textures.size(); textures.emplace_back(w, h, std::move(px)); }
        tindex[key] = id; return id;
    };
    std::vector<float> V; long lastTid = -1; size_t i = 0, prevEnd = 0;
    while (len >= 16 && i < len - 16) {
        const uint32_t w = r.u32(i), cmd = w >> 24, num = (w >> 16) & 0xff; const size_t psz = 12 * (size_t)num;
        if ((cmd == 0x68 || cmd == 0x78) && num >= 3 && i + 4 + psz <= len) {
            std::vector<double> pos(3 * (size_t)num); bool fin = true;
            for (size_t k = 0; k < pos.size(); k++) { uint32_t u = r.u32(i + 4 + 4 * k); float f; std::memcpy(&f, &u, 4); if (!std::isfinite(f) || std::fabs(f) >= 2e5f) fin = false; pos[k] = f; }
            if (fin) {
                std::vector<const tex::Material*> group; for (const tex::Material& m : L.mats) if (prevEnd <= m.tex0Off && m.tex0Off < i) group.push_back(&m);
                long tid = group.empty() ? lastTid : getTex(*group[0]);   // variant 0
                bool haveIdx = false, haveHdr = false; std::vector<uint8_t> idx; long hdr = 0; std::set<long> flag; std::vector<std::pair<int16_t, int16_t>> uvs; bool haveUv = false; std::vector<std::array<double, 4>> cols; bool haveCol = false;
                size_t end = i + 4 + psz;
                iterVif(ngpD, end, std::min(len, end + 0x4000), [&](const VifCmd& c) -> bool {
                    if (c.kind == 8 || c.kind == 7) return false;
                    const size_t off = c.off;
                    if (c.kind == 1) {
                        hdr = c.imm & 0x3ff; haveHdr = true; haveIdx = false; idx.clear(); flag.clear(); haveUv = false; haveCol = false;
                        const uint32_t sel = r.u32(off + 16);
                        if (sel && !group.empty()) { const size_t slot = std::min<size_t>((size_t)__builtin_ctz(sel) / 2, group.size() - 1); tid = getTex(*group[slot]); }
                    } else if (c.kind == 2 && c.count >= 3 && !haveIdx) {
                        idx.clear(); for (size_t k = 0; k < c.count && off + 4 + k < len; k++) idx.push_back(ngpD[off + 4 + k]); haveIdx = true;
                    } else if (c.kind == 2 && c.count <= 3 && haveIdx && haveHdr) {
                        const long a = (long)(c.imm & 0x3ff) - (hdr + 3); const long v0 = a >= 0 ? a / 3 : -((-a + 2) / 3);   // división entera hacia -inf como Python
                        for (long k = v0; k < v0 + (long)c.count; k++) flag.insert(k);
                    } else if (c.kind == 3 && haveIdx && c.count == idx.size()) {
                        cols.clear(); for (size_t j = 0; j < c.count; j++) { std::array<double, 4> q; for (int k = 0; k < 4; k++) { const size_t p = off + 4 + 4 * j + k; q[k] = (p < len ? ngpD[p] : 0) / 128.0; } cols.push_back(q); } haveCol = true;
                    } else if (c.kind == 4 && haveIdx && c.count == idx.size()) {
                        cols.clear(); for (size_t j = 0; j < c.count; j++) { const uint32_t cc = r.u16(off + 4 + 2 * j); cols.push_back({(cc & 31) * 8 / 128.0, (cc >> 5 & 31) * 8 / 128.0, (cc >> 10 & 31) * 8 / 128.0, 1.0}); } haveCol = true;
                    } else if (c.kind == 5 && haveIdx && c.count == idx.size()) {
                        uvs.clear(); for (size_t j = 0; j < c.count; j++) { uint16_t a = r.u16(off + 4 + 4 * j), b = r.u16(off + 6 + 4 * j); uvs.push_back({(int16_t)a, (int16_t)b}); } haveUv = true;
                    } else if (c.kind == 6 && haveIdx) {
                        uint8_t mx = 0; for (uint8_t q : idx) mx = std::max(mx, q);
                        if (mx < num) {
                            for (size_t j = 2; j < idx.size(); j++) {
                                if (flag.count((long)j)) continue;
                                size_t o[3] = {j - 2, j - 1, j}; if (j & 1) { o[1] = j; o[2] = j - 1; }
                                if (idx[o[0]] == idx[o[1]] || idx[o[0]] == idx[o[2]] || idx[o[1]] == idx[o[2]]) continue;
                                for (int a = 0; a < 3; a++) {
                                    const double u = haveUv ? uvs[o[a]].first / 4096.0 : 0.0, vv = haveUv ? uvs[o[a]].second / 4096.0 : 0.0; const size_t pi = idx[o[a]];
                                    V.push_back((float)pos[3 * pi]); V.push_back((float)pos[3 * pi + 1]); V.push_back((float)pos[3 * pi + 2]); V.push_back((float)u); V.push_back((float)(1.0 - vv));
                                    for (int k = 0; k < 4; k++) V.push_back((float)(haveCol ? cols[o[a]][k] : 1.0)); V.push_back((float)tid);
                                }
                            }
                        }
                        haveIdx = false; haveHdr = false; idx.clear(); flag.clear(); haveUv = false; haveCol = false;
                    }
                    end = off + 4 + c.size; return true;
                });
                prevEnd = end; lastTid = tid; i += 4 + psz; continue;
            }
        }
        i += 4;
    }
    // save(): recompacta texturas por primera aparición en los vértices (id >= 0); sin dueños ni chunks (DHM2)
    std::map<long, long> remap; std::vector<std::tuple<uint32_t, uint32_t, std::vector<uint8_t>>> tx;
    for (size_t k = 9; k < V.size(); k += 10) { const long t = (long)V[k]; if (t >= 0) { if (!remap.count(t)) { remap[t] = (long)tx.size(); tx.push_back(textures[(size_t)t]); } V[k] = (float)remap[t]; } }
    const uint32_t nvv = (uint32_t)(V.size() / 10), ntex = (uint32_t)tx.size();
    mdl.assign({'D', 'H', 'M', '2'}); auto put = [&](const void* p, size_t n) { const uint8_t* b = (const uint8_t*)p; mdl.insert(mdl.end(), b, b + n); };
    put(&ntex, 4); put(&nvv, 4); put(&nvv, 4);
    for (auto& [w, h, px] : tx) { put(&w, 4); put(&h, 4); put(px.data(), px.size()); }
    put(V.data(), V.size() * 4); for (uint32_t k = 0; k < nvv; k++) put(&k, 4);
    return true;
}
}  // namespace mesh
