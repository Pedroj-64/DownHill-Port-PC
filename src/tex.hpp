// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Paso 4 del loader nativo (docs/native-loader-plan.md): texturas. Direccionamiento de la memoria del GS (PSMCT32/PSMT8/PSMT4/PSMT8H, tablas estándar públicas, tools/gs.py) y decodificación a RGBA con paleta CLUT,
// igual que tools/extract_model.py (_make_texture). Lee .PTR/.TEX/.RTX y los TEX0 del .NGP. Todos los accesos están acotados (entrada no fiable): un registro corrupto se omite, no lee fuera.
#pragma once
#include "ngp.hpp"
#include <algorithm>
#include <map>

namespace tex {
inline constexpr uint16_t kBLK[4][8] = {{0,1,4,5,16,17,20,21},{2,3,6,7,18,19,22,23},{8,9,12,13,24,25,28,29},{10,11,14,15,26,27,30,31}};
inline constexpr uint16_t kCOL32[8][8] = {{0,1,4,5,8,9,12,13},{2,3,6,7,10,11,14,15},{16,17,20,21,24,25,28,29},{18,19,22,23,26,27,30,31},{32,33,36,37,40,41,44,45},{34,35,38,39,42,43,46,47},{48,49,52,53,56,57,60,61},{50,51,54,55,58,59,62,63}};
inline constexpr uint16_t kCOL8[16][16] = {{0,4,16,20,32,36,48,52,2,6,18,22,34,38,50,54},{8,12,24,28,40,44,56,60,10,14,26,30,42,46,58,62},{33,37,49,53,1,5,17,21,35,39,51,55,3,7,19,23},{41,45,57,61,9,13,25,29,43,47,59,63,11,15,27,31},{96,100,112,116,64,68,80,84,98,102,114,118,66,70,82,86},{104,108,120,124,72,76,88,92,106,110,122,126,74,78,90,94},{65,69,81,85,97,101,113,117,67,71,83,87,99,103,115,119},{73,77,89,93,105,109,121,125,75,79,91,95,107,111,123,127},{128,132,144,148,160,164,176,180,130,134,146,150,162,166,178,182},{136,140,152,156,168,172,184,188,138,142,154,158,170,174,186,190},{161,165,177,181,129,133,145,149,163,167,179,183,131,135,147,151},{169,173,185,189,137,141,153,157,171,175,187,191,139,143,155,159},{224,228,240,244,192,196,208,212,226,230,242,246,194,198,210,214},{232,236,248,252,200,204,216,220,234,238,250,254,202,206,218,222},{193,197,209,213,225,229,241,245,195,199,211,215,227,231,243,247},{201,205,217,221,233,237,249,253,203,207,219,223,235,239,251,255}};
inline constexpr uint16_t kBLK4[8][4] = {{0,2,8,10},{1,3,9,11},{4,6,12,14},{5,7,13,15},{16,18,24,26},{17,19,25,27},{20,22,28,30},{21,23,29,31}};
inline constexpr uint16_t kCOL4[16][32] = {{0,8,32,40,64,72,96,104,2,10,34,42,66,74,98,106,4,12,36,44,68,76,100,108,6,14,38,46,70,78,102,110},{16,24,48,56,80,88,112,120,18,26,50,58,82,90,114,122,20,28,52,60,84,92,116,124,22,30,54,62,86,94,118,126},{65,73,97,105,1,9,33,41,67,75,99,107,3,11,35,43,69,77,101,109,5,13,37,45,71,79,103,111,7,15,39,47},{81,89,113,121,17,25,49,57,83,91,115,123,19,27,51,59,85,93,117,125,21,29,53,61,87,95,119,127,23,31,55,63},{192,200,224,232,128,136,160,168,194,202,226,234,130,138,162,170,196,204,228,236,132,140,164,172,198,206,230,238,134,142,166,174},{208,216,240,248,144,152,176,184,210,218,242,250,146,154,178,186,212,220,244,252,148,156,180,188,214,222,246,254,150,158,182,190},{129,137,161,169,193,201,225,233,131,139,163,171,195,203,227,235,133,141,165,173,197,205,229,237,135,143,167,175,199,207,231,239},{145,153,177,185,209,217,241,249,147,155,179,187,211,219,243,251,149,157,181,189,213,221,245,253,151,159,183,191,215,223,247,255},{256,264,288,296,320,328,352,360,258,266,290,298,322,330,354,362,260,268,292,300,324,332,356,364,262,270,294,302,326,334,358,366},{272,280,304,312,336,344,368,376,274,282,306,314,338,346,370,378,276,284,308,316,340,348,372,380,278,286,310,318,342,350,374,382},{321,329,353,361,257,265,289,297,323,331,355,363,259,267,291,299,325,333,357,365,261,269,293,301,327,335,359,367,263,271,295,303},{337,345,369,377,273,281,305,313,339,347,371,379,275,283,307,315,341,349,373,381,277,285,309,317,343,351,375,383,279,287,311,319},{448,456,480,488,384,392,416,424,450,458,482,490,386,394,418,426,452,460,484,492,388,396,420,428,454,462,486,494,390,398,422,430},{464,472,496,504,400,408,432,440,466,474,498,506,402,410,434,442,468,476,500,508,404,412,436,444,470,478,502,510,406,414,438,446},{385,393,417,425,449,457,481,489,387,395,419,427,451,459,483,491,389,397,421,429,453,461,485,493,391,399,423,431,455,463,487,495},{401,409,433,441,465,473,497,505,403,411,435,443,467,475,499,507,405,413,437,445,469,477,501,509,407,415,439,447,471,479,503,511}};
inline size_t addr32(size_t x, size_t y, size_t w) { return ((y / 32) * (w / 64) + x / 64) * 2048 + kBLK[(y % 32) / 8][(x % 64) / 8] * 64 + kCOL32[y % 8][x % 8]; }       // en palabras de 32 bits
inline size_t addr8(size_t x, size_t y, size_t w) { return ((y / 64) * (w / 128) + x / 128) * 8192 + kBLK[(y % 64) / 16][(x % 128) / 16] * 256 + kCOL8[y % 16][x % 16]; }      // en bytes
inline size_t addr4(size_t x, size_t y, size_t w) { return ((y / 128) * (w / 128) + x / 128) * 16384 + kBLK4[(y % 128) / 16][(x % 128) / 32] * 512 + kCOL4[y % 16][x % 32]; }  // en nibbles

// Transferencia lineal CT32 -> memoria del GS (páginas completas de 8 KB). `data` debe tener w*h*4 bytes.
inline std::vector<uint8_t> upload32(const std::vector<uint8_t>& data, size_t w, size_t h) {
    std::vector<uint8_t> mem(((h + 31) / 32) * std::max<size_t>(w / 64, 1) * 8192, 0);
    for (size_t y = 0; y < h; y++) for (size_t x = 0; x < w; x++) { const size_t a = addr32(x, y, w) * 4, s = (y * w + x) * 4; if (a + 4 <= mem.size() && s + 4 <= data.size()) std::memcpy(&mem[a], &data[s], 4); }
    return mem;
}
// false si la dirección cae fuera de la memoria (el Python lanza IndexError y deja la textura sin resolver)
inline bool read8(const std::vector<uint8_t>& mem, size_t w, size_t h, std::vector<uint8_t>& o) { o.resize(w * h); for (size_t y = 0; y < h; y++) for (size_t x = 0; x < w; x++) { const size_t a = addr8(x, y, w); if (a >= mem.size()) return false; o[y * w + x] = mem[a]; } return true; }
inline bool read4(const std::vector<uint8_t>& mem, size_t w, size_t h, std::vector<uint8_t>& o) { o.resize(w * h); for (size_t y = 0; y < h; y++) for (size_t x = 0; x < w; x++) { const size_t a = addr4(x, y, w); if ((a >> 1) >= mem.size()) return false; o[y * w + x] = (mem[a >> 1] >> ((a & 1) * 4)) & 15; } return true; }
inline bool read8h(const std::vector<uint8_t>& mem, size_t w, size_t h, std::vector<uint8_t>& o) { o.resize(w * h); for (size_t y = 0; y < h; y++) for (size_t x = 0; x < w; x++) { const size_t a = addr32(x, y, w) * 4 + 3; if (a >= mem.size()) return false; o[y * w + x] = mem[a]; } return true; }

struct Rec { size_t off; uint32_t lo, hi; };
// Cadena de registros de 16 B: [next, h, lo, hi]; next en palabras de 16 B, 0 = último (extract_model.py records()).
inline std::vector<Rec> records(const std::vector<uint8_t>& b, size_t first) {
    ngp::Reader r{b}; std::vector<Rec> out; size_t off = first;
    while (off + 16 <= b.size() && out.size() < (1u << 20)) { out.push_back({off, r.u32(off + 8), r.u32(off + 12)}); const uint32_t nxt = r.u32(off); if (nxt == 0) break; const size_t step = (size_t)(nxt & ~3u) * 4; if (step == 0) break; off += step; }
    return out;
}
struct Tex { uint32_t w, h, fmt; std::vector<uint8_t> data; };
struct Material { size_t tex0Off; uint32_t tid, psm, w, h, cbp; };
struct Level { std::map<uint32_t, Tex> texs; std::map<uint32_t, std::vector<uint8_t>> cluts; std::vector<Material> mats; };

inline bool load(const std::vector<uint8_t>& ngpD, const std::vector<uint8_t>& ptr, const std::vector<uint8_t>& texD, const std::vector<uint8_t>& rtx, Level& L) {
    ngp::Reader pr{ptr}, tr{texD}; if (texD.size() < 8 || ptr.size() < 8) return false;
    std::vector<Rec> recs = records(texD, (size_t)tr.u32(4) * 16); std::vector<size_t> offs; for (auto& r : recs) offs.push_back(r.off); offs.push_back(texD.size());
    for (size_t i = 0; i < recs.size(); i++) {
        const uint32_t w = 1u << (recs[i].hi >> 8 & 15), h = 1u << (recs[i].hi >> 12 & 15), fmt = recs[i].hi & 0x3f;
        const size_t size = fmt == 19 ? (size_t)w * h : fmt == 20 ? (size_t)w * h / 2 : (size_t)w * h * 4, start = recs[i].off + 0x80;
        const size_t avail = offs[i + 1] > start ? offs[i + 1] - start : 0, take = std::min(size, avail);
        Tex t{w, h, fmt, {}}; if (start <= texD.size()) t.data.assign(texD.begin() + start, texD.begin() + std::min(texD.size(), start + take));
        L.texs[recs[i].lo & 0xffff] = std::move(t);
    }
    for (const Rec& r : records(rtx, 0)) { const uint32_t w = 1u << (r.hi >> 8 & 15), h = 1u << (r.hi >> 12 & 15); if (w * h == 256 || w * h == 16) { const size_t s = r.off + 16, n = (size_t)w * h * 4; if (s <= rtx.size()) L.cluts[r.lo >> 16].assign(rtx.begin() + s, rtx.begin() + std::min(rtx.size(), s + n)); } }
    const uint32_t n0 = pr.u32(0); const size_t q = 4 + 4 * (size_t)n0; if (q + 4 > ptr.size()) return false;
    const uint32_t nt = pr.u32(q); const size_t q2 = q + 4 + 4 * (size_t)nt; if (q2 + 4 > ptr.size()) return false; const uint32_t nc = pr.u32(q2);
    for (uint32_t i = 0; i < std::min(nt, nc); i++) {
        const size_t to = pr.u32(q + 4 + 4 * i), co = pr.u32(q2 + 4 + 4 * i); if (to + 2 > ngpD.size() || co + 8 > ngpD.size()) continue;
        uint64_t v; std::memcpy(&v, ngpD.data() + co, 8); uint16_t tid; std::memcpy(&tid, ngpD.data() + to, 2);
        L.mats.push_back({co, tid, (uint32_t)(v >> 20 & 63), 1u << (v >> 26 & 15), 1u << (v >> 30 & 15), (uint32_t)(v >> 37 & 0x3fff)});
    }
    return true;
}
// RGBA de un material (extract_model.py _make_texture): paleta CLUT con el intercambio de bits 3/4 en las de 8 bpp; alfa 0x80 -> 255, si no a*2. false = sin resolver (el Python devuelve None o lanza IndexError).
inline bool makeTexture(const Level& L, const Material& m, uint32_t& W, uint32_t& H, std::vector<uint8_t>& rgba) {
    if (!(m.psm == 0x13 || m.psm == 0x14 || m.psm == 0x1b)) return false;
    auto ti = L.texs.find(m.tid); auto ci = L.cluts.find(m.cbp); if (ti == L.texs.end() || ci == L.cluts.end()) return false;
    const Tex& t = ti->second; const std::vector<uint8_t>& pal = ci->second; bool swap = true; std::vector<uint8_t> idx; W = m.w; H = m.h;
    if (t.fmt == 19) { W = t.w; H = t.h; idx = t.data; idx.resize((size_t)W * H, 0); }
    else if (t.fmt == 20) { W = t.w; H = t.h; std::vector<uint8_t> d = t.data; d.resize((size_t)W * H / 2, 0); for (uint8_t b : d) { idx.push_back(b & 15); idx.push_back(b >> 4); } swap = false; }
    else {
        std::vector<uint8_t> d = t.data; d.resize((size_t)t.w * t.h * 4, 0); const std::vector<uint8_t> mem = upload32(d, t.w, t.h); bool ok;
        if (m.psm == 0x1b) ok = read8h(mem, W, H, idx); else if (m.psm == 0x13) ok = read8(mem, W, H, idx); else { ok = read4(mem, W, H, idx); swap = false; }
        if (!ok) return false;
    }
    rgba.clear(); rgba.reserve(idx.size() * 4);
    for (uint8_t p : idx) {
        const size_t i = swap ? ((p & ~0x18) | ((p & 8) << 1) | ((p & 16) >> 1)) : p, e = i * 4; if (e + 4 > pal.size()) return false;   // IndexError del Python
        rgba.push_back(pal[e]); rgba.push_back(pal[e + 1]); rgba.push_back(pal[e + 2]); rgba.push_back(pal[e + 3] >= 0x80 ? 255 : (uint8_t)(pal[e + 3] * 2));
    }
    return true;
}
}  // namespace tex
