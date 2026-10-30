// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Paso 5 del loader nativo (docs/native-loader-plan.md): 5a geometría de modelos SUELTOS (bici, etc.: sin grafo útil, todo sin transformar; .mdl DHM2) y 5b NIVELES (instancias del grafo con matrices acumuladas = Owners de tools/scene.py,
// .mdl DHM3 con chunks + cielo y domo). Mismo algoritmo y mismos bytes que tools/extract_model.py. 5a sin
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

// ---------------- Owners (tools/scene.py: walk_payloads, Owners, split_type25) ----------------
using Mat = std::array<double, 16>;
inline constexpr Mat kIdent = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
struct Inst { size_t ptr = 0; Mat m = kIdent; std::vector<std::array<double, 5>> sels; uint32_t layer = 0, kind = 0; double rad = 0; long root = -1; };
inline Mat mulMat(const Mat& a, const Mat& b) { Mat o; for (int r = 0; r < 4; r++) for (int c = 0; c < 4; c++) { double s = 0; for (int k = 0; k < 4; k++) s += a[4 * r + k] * b[4 * k + c]; o[4 * r + c] = s; } return o; }
inline float f32at(const ngp::Reader& r, size_t o) { uint32_t u = r.u32(o); float f; std::memcpy(&f, &u, 4); return f; }
inline bool localMat(const ngp::Reader& r, size_t o, Mat& m) {
    const uint32_t t = r.u32(o) & 0x3f;
    if (t == 3) { m = kIdent; m[12] = f32at(r, o + 0x10); m[13] = f32at(r, o + 0x14); m[14] = f32at(r, o + 0x18); return true; }
    if (t == 4) { for (int i = 0; i < 16; i++) m[i] = f32at(r, o + 0x10 + 4 * i); m[3] = m[7] = m[11] = 0; m[15] = 1; return true; }
    return false;
}
struct Anc { uint32_t layer = 0, kind = 0; double rad = 0; bool bd = false; };
// Escena del menú (SHELL/UI*): los nodos tipo 6 son SWITCH (+8 = nº de hijos << 24 | hijo activo; hijos desde +0xC): el juego solo dibuja el hijo activo, que en RAM del menú principal es el 0 salvo en el nodo kind 9006 (= 1);
// las instancias de capa 7 (cascarones de radio 1000) no se dibujan. Medido contra volcado GS: acuerdo con lo que dibuja el juego 77 % en vista (sin la regla 25 %). HIPÓTESIS parcial: faltan reglas (p. ej. 9004 dibuja también algo del hijo 1).
inline bool gUiSwitch = false;
inline void walkRec(const ngp::Reader& r, size_t o, const Mat& m, const std::vector<std::array<double, 5>>& sels, std::vector<size_t>& path, long rk, Anc anc, std::vector<Inst>& out) {
    if (std::find(path.begin(), path.end(), o) != path.end() || path.size() > 60) return;
    const uint32_t h = r.u32(o), t = h & 0x3f; if (rk < 0 && !path.empty()) rk = (long)o;
    if (t == 1) {
        const uint32_t lay = r.u32(o + 8) >> 16; const bool bd = anc.bd || lay == 2; anc = {bd ? 2u : lay, h >> 18, (double)f32at(r, o + 0x1c), bd};
        size_t ch; const bool has = r.ptr(o + 0x20, ch);
        if (anc.kind == 0 && r.u16(o + 8) <= 1 && has && (r.u32(ch) & 0x3f) != 0 && !(gUiSwitch && anc.layer == 7)) { Inst d; d.ptr = ch; d.m = m; d.sels = sels; d.layer = anc.layer; d.kind = anc.kind; d.rad = anc.rad; d.root = rk; out.push_back(std::move(d)); }
    }
    if (t == 0 && anc.kind == 0 && !(gUiSwitch && anc.layer == 7)) { Inst d; d.ptr = o; d.m = m; d.sels = sels; d.layer = anc.layer; d.kind = anc.kind; d.rad = anc.rad; d.root = rk; out.push_back(std::move(d)); }
    Mat l, m2 = m; if (localMat(r, o, l)) m2 = mulMat(l, m);
    std::vector<std::array<double, 5>> s2 = sels;
    if (t == 2) { const double cx = f32at(r, o + 0x10), cy = f32at(r, o + 0x14), cz = f32at(r, o + 0x18); std::array<double, 5> e; for (int k = 0; k < 3; k++) e[k] = cx * m[0 + k] + cy * m[4 + k] + cz * m[8 + k] + m[12 + k]; e[3] = f32at(r, o + 0x1c); e[4] = f32at(r, o + 0x24); s2.push_back(e); }
    std::vector<size_t> ch; ngp::sceneChildren(r, o, ch);
    if (gUiSwitch && t == 6) { const uint32_t w = r.u32(o + 8), n = w >> 24; uint32_t sel = w & 0xFFFFFF; if ((h >> 18) == 9006) sel = 1; ch.clear(); size_t c; if (sel < n && r.ptr(o + 0xc + 4 * sel, c)) ch.push_back(c); }
    path.push_back(o); for (size_t c : ch) walkRec(r, c, m2, s2, path, rk, anc, out); path.pop_back();
}
struct Owners {
    std::vector<Inst> inst; std::map<size_t, std::vector<long>> byPtr; std::vector<size_t> starts;
    long ownerStart(size_t i) const { auto it = std::upper_bound(starts.begin(), starts.end(), i); return it == starts.begin() ? -1 : (long)*(it - 1); }
    const std::vector<long>* ids(size_t i) const { const long o = ownerStart(i); if (o < 0) return nullptr; auto it = byPtr.find((size_t)o); return it == byPtr.end() ? nullptr : &it->second; }
    bool build(const std::vector<uint8_t>& d) {
        ngp::Reader r{d}; if (d.size() < 8) return false; const uint32_t n = r.u32(0); if (n > (d.size() - 4) / 4) return false;
        for (uint32_t i = 0; i < n; i++) { size_t root; if (!r.ptr(4 + 4 * i, root)) continue; if ((r.u32(root) & 0x3f) == 7) continue; std::vector<size_t> path; walkRec(r, root, kIdent, {}, path, -1, Anc{}, inst); }
        for (size_t k = 0; k < inst.size(); k++) byPtr[inst[k].ptr].push_back((long)k);
        // El cielo (capa 2) es una hoja clásica: su rango va de su inicio hasta el inicio de la siguiente hoja clásica
        std::map<size_t, uint32_t> classic;
        struct Rec { static void go(const ngp::Reader& r, size_t o, std::vector<size_t>& path, std::map<size_t, uint32_t>& classic) {
            if (std::find(path.begin(), path.end(), o) != path.end() || path.size() > 60) return;
            const uint32_t h = r.u32(o); size_t q;
            if ((h & 0x3f) == 1 && r.u16(o + 8) <= 1 && r.ptr(o + 0x20, q)) classic[q] = r.u32(o + 8) >> 16;
            std::vector<size_t> ch; ngp::sceneChildren(r, o, ch); path.push_back(o); for (size_t c : ch) go(r, c, path, classic); path.pop_back(); } };
        for (uint32_t i = 0; i < n; i++) { size_t root; if (!r.ptr(4 + 4 * i, root) || (r.u32(root) & 0x3f) == 7) continue; std::vector<size_t> path; Rec::go(r, root, path, classic); }
        std::vector<size_t> cs; for (auto& kv : classic) cs.push_back(kv.first);
        for (size_t a = 0; a < cs.size(); a++) { const size_t nxt = a + 1 < cs.size() ? cs[a + 1] : (size_t)-1; if (classic[cs[a]] == 2) for (auto& d2 : inst) if (cs[a] <= d2.ptr && d2.ptr < nxt) d2.layer = 2; }
        // split_type25: contenedor con u32 n @+0x28 y n punteros a cadenas VIF @+0x2c; cada una pasa a ser un inicio propio de las instancias del nodo
        for (size_t k = 0; k < inst.size(); k++) {
            const size_t p = inst[k].ptr; if ((r.u32(p) & 0x3f) != 25) continue; const uint32_t cnt = std::min<uint32_t>(r.u32(p + 0x28), 16);
            for (uint32_t i = 0; i < cnt; i++) { size_t q; if (!r.ptr(p + 0x2c + 4 * i, q) || q == p) continue; auto& v = byPtr[q]; if (std::find(v.begin(), v.end(), (long)k) == v.end()) v.push_back((long)k); }
        }
        starts.clear(); for (auto& kv : byPtr) starts.push_back(kv.first);
        return true;
    }
};

struct ScanOut { std::vector<float> V; std::vector<long> VO; std::vector<std::tuple<uint32_t, uint32_t, std::vector<uint8_t>>> textures; };
inline bool scanModel(const std::vector<uint8_t>& ngpD, const tex::Level& L, const Owners* OW, ScanOut& so) {
    ngp::Reader r{ngpD}; const size_t len = ngpD.size();
    auto& textures = so.textures; std::map<std::pair<uint32_t, uint32_t>, long> tindex;
    auto getTex = [&](const tex::Material& m) -> long {
        const auto key = std::make_pair(m.tid, m.cbp); auto it = tindex.find(key); if (it != tindex.end()) return it->second;
        uint32_t w, h; std::vector<uint8_t> px; long id = -1;
        if (tex::makeTexture(L, m, w, h, px)) { id = (long)textures.size(); textures.emplace_back(w, h, std::move(px)); }
        tindex[key] = id; return id;
    };
    auto& V = so.V; auto& VO = so.VO; long lastTid = -1; size_t i = 0, prevEnd = 0;
    while (len >= 16 && i < len - 16) {
        const uint32_t w = r.u32(i), cmd = w >> 24, num = (w >> 16) & 0xff; const size_t psz = 12 * (size_t)num;
        if ((cmd == 0x68 || cmd == 0x78) && num >= 3 && i + 4 + psz <= len) {
            std::vector<double> pos(3 * (size_t)num); bool fin = true;
            for (size_t k = 0; k < pos.size(); k++) { uint32_t u = r.u32(i + 4 + 4 * k); float f; std::memcpy(&f, &u, 4); if (!std::isfinite(f) || std::fabs(f) >= 2e5f) fin = false; pos[k] = f; }
            if (fin) {
                std::vector<const tex::Material*> group; for (const tex::Material& m : L.mats) if (prevEnd <= m.tex0Off && m.tex0Off < i) group.push_back(&m);
                if (gUiSwitch && group.size() > 1) { std::vector<const tex::Material*> near; for (auto* g : group) if (i - g->tex0Off <= 0x200) near.push_back(g); if (!near.empty()) group = near; }   // UI: el material del lote es el pegado a él; los lejanos son restos del hueco desde el lote anterior
                long tid = group.empty() ? lastTid : getTex(*group[0]);   // variant 0
                bool haveIdx = false, haveHdr = false; std::vector<uint8_t> idx; long hdr = 0; std::set<long> flag; std::vector<std::pair<int16_t, int16_t>> uvs; bool haveUv = false; std::vector<std::array<double, 4>> cols; bool haveCol = false;
                size_t end = i + 4 + psz;
                // instancias del grafo (una copia por visita: árboles, banderas...); sin dueño: tal cual
                std::vector<std::pair<long, std::vector<double>>> insts;
                if (OW) if (const std::vector<long>* ids = OW->ids(i)) for (long k : *ids) {
                    const Inst& in = OW->inst[(size_t)k]; std::vector<double> wp;
                    if (in.m == kIdent) wp = pos; else for (size_t j = 0; j + 2 < pos.size() + 0; j += 3) { const double x = pos[j], y = pos[j + 1], z = pos[j + 2]; wp.push_back(x * in.m[0] + y * in.m[4] + z * in.m[8] + in.m[12]); wp.push_back(x * in.m[1] + y * in.m[5] + z * in.m[9] + in.m[13]); wp.push_back(x * in.m[2] + y * in.m[6] + z * in.m[10] + in.m[14]); }
                    insts.emplace_back(k, std::move(wp));
                }
                if (insts.empty()) insts.emplace_back(-1, pos);
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
                                for (const auto& [kInst, wpos] : insts) for (int a = 0; a < 3; a++) {
                                    const double u = haveUv ? uvs[o[a]].first / 4096.0 : 0.0, vv = haveUv ? uvs[o[a]].second / 4096.0 : 0.0; const size_t pi = idx[o[a]];
                                    V.push_back((float)wpos[3 * pi]); V.push_back((float)wpos[3 * pi + 1]); V.push_back((float)wpos[3 * pi + 2]); V.push_back((float)u); V.push_back((float)(1.0 - vv));
                                    for (int k = 0; k < 4; k++) V.push_back((float)(haveCol ? cols[o[a]][k] : 1.0)); V.push_back((float)tid); VO.push_back(kInst);
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
    return true;
}

// save() de extract_model.py: grupos [(dueño, [triángulos])]; recompacta texturas por primera aparición en los vértices (id >= 0); chunks = DHM3 (tras los índices: u32 nchunks y por chunk first, n, nsel, sels).
inline std::vector<uint8_t> saveMdl(const ScanOut& so, const std::vector<std::pair<long, std::vector<size_t>>>& groups, const Owners* OW, bool chunks) {
    std::map<long, long> remap; std::vector<size_t> txOrder; std::vector<float> vs; std::vector<std::tuple<long, size_t, size_t>> tab;
    for (const auto& [own, tris] : groups) {
        const size_t first = vs.size() / 10;
        for (size_t t : tris) for (int k = 0; k < 3; k++) {
            float row[10]; std::memcpy(row, &so.V[(3 * t + k) * 10], 40); const long tid = (long)row[9];
            if (tid >= 0) { if (!remap.count(tid)) { remap[tid] = (long)txOrder.size(); txOrder.push_back((size_t)tid); } row[9] = (float)remap[tid]; }
            vs.insert(vs.end(), row, row + 10);
        }
        tab.emplace_back(own, first, vs.size() / 10 - first);
    }
    const uint32_t nvv = (uint32_t)(vs.size() / 10), ntex = (uint32_t)txOrder.size(); std::vector<uint8_t> mdl;
    mdl.assign({'D', 'H', 'M', (uint8_t)(chunks ? '3' : '2')}); auto put = [&](const void* p, size_t n) { const uint8_t* b = (const uint8_t*)p; mdl.insert(mdl.end(), b, b + n); };
    put(&ntex, 4); put(&nvv, 4); put(&nvv, 4);
    for (size_t t : txOrder) { const auto& [w, h, px] = so.textures[t]; put(&w, 4); put(&h, 4); put(px.data(), px.size()); }
    put(vs.data(), vs.size() * 4); for (uint32_t k = 0; k < nvv; k++) put(&k, 4);
    if (chunks) {
        const uint32_t nt = (uint32_t)tab.size(); put(&nt, 4);
        for (const auto& [own, first, n] : tab) {
            const uint32_t f = (uint32_t)first, c = (uint32_t)n; const std::vector<std::array<double, 5>>* sl = (OW && own >= 0) ? &OW->inst[(size_t)own].sels : nullptr; const uint32_t ns = sl ? (uint32_t)sl->size() : 0;
            put(&f, 4); put(&c, 4); put(&ns, 4); if (sl) for (const auto& e : *sl) for (int k = 0; k < 5; k++) { const float x = (float)e[k]; put(&x, 4); }
        }
    }
    return mdl;
}

// Modelo suelto (5a): sin Owners, un solo grupo, DHM2.
inline bool extractStatic(const std::vector<uint8_t>& ngpD, const tex::Level& L, std::vector<uint8_t>& mdl) {
    ScanOut so; if (!scanModel(ngpD, L, nullptr, so)) return false;
    std::vector<size_t> all(so.V.size() / 30); for (size_t t = 0; t < all.size(); t++) all[t] = t;
    mdl = saveMdl(so, {{-1, all}}, nullptr, false); return true;
}

// Nivel (5b): .mdl DHM3 + cielo (capa 2) + domo del horizonte, como el final de extract_model.py.
struct LevelFiles { std::vector<uint8_t> main, sky, dome; bool hasSky = false, hasDome = false; };
inline bool extractLevel(const std::vector<uint8_t>& ngpD, const tex::Level& L, LevelFiles& out) {
    Owners OW; if (!OW.build(ngpD)) return false;
    ScanOut so; if (!scanModel(ngpD, L, &OW, so)) return false;
    const size_t ntri = so.V.size() / 30; std::vector<long> order; std::map<long, std::vector<size_t>> byown;
    for (size_t t = 0; t < ntri; t++) { const long o = so.VO[3 * t]; auto it = byown.find(o); if (it == byown.end()) { order.push_back(o); byown[o] = {}; } byown[o].push_back(t); }
    std::set<long> sky, dome;
    for (long o : order) if (o >= 0 && OW.inst[(size_t)o].layer == 2) sky.insert(o);
    {   // panorama del horizonte (extract_model.py): raíz pequeña, de radio enorme, sin celdas de terreno y con todas las hojas en el origen
        std::vector<long> rootOrder; std::map<long, std::vector<long>> roots;
        for (size_t k = 0; k < OW.inst.size(); k++) { const long rk = OW.inst[k].root; if (!roots.count(rk)) rootOrder.push_back(rk); roots[rk].push_back((long)k); }
        for (double R : {3000.0, 2000.0}) {
            for (long rk : rootOrder) {
                const auto& grp = roots[rk]; if (grp.size() > 16) continue; double mr = -1e300; bool l3 = false, origin = true;
                for (long o : grp) { const Inst& in = OW.inst[(size_t)o]; mr = std::max(mr, in.rad); if (in.layer == 3) l3 = true; double mx = 0; for (int k = 12; k < 15; k++) mx = std::max(mx, std::fabs(in.m[k])); if (!(mx < 1500)) origin = false; }
                if (mr <= R || l3) continue;
                if (origin) for (long o : grp) if (byown.count(o)) dome.insert(o);
            }
            if (!dome.empty()) break;
        }
    }
    for (long o : dome) sky.erase(o);
    auto sortedTris = [&](const std::set<long>& owners) { std::vector<size_t> v; for (long o : owners) v.insert(v.end(), byown[o].begin(), byown[o].end()); std::sort(v.begin(), v.end()); return v; };
    out.hasDome = !dome.empty(); if (out.hasDome) out.dome = saveMdl(so, {{-1, sortedTris(dome)}}, nullptr, false);
    out.hasSky = !sky.empty(); if (out.hasSky) out.sky = saveMdl(so, {{-1, sortedTris(sky)}}, nullptr, false);
    std::vector<std::pair<long, std::vector<size_t>>> mainGroups; for (long o : order) if (!sky.count(o) && !dome.count(o)) mainGroups.emplace_back(o, byown[o]);
    out.main = saveMdl(so, mainGroups, &OW, true); return true;
}
}  // namespace mesh
