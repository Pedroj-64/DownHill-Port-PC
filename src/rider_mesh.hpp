// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Lector nativo de la cadena VIF del cuerpo del rider en un NGP.
// El formato de la cadena y el empaquetado de piel siguen docs/formats/rider.md.
#pragma once
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <utility>
#include <vector>

namespace rider_mesh {
struct Mesh {
    std::vector<float> vertices; // 10 f32 por vértice: xyz uv rgba textura
    std::vector<uint32_t> indices;
    std::vector<uint8_t> boneIds; // 3 por vértice
    std::vector<float> weights;   // 3 por vértice
    uint32_t positionCount = 0;
    uint32_t packetCount = 0;
};

class Reader {
    const std::vector<uint8_t>& d_;
public:
    explicit Reader(const std::vector<uint8_t>& d) : d_(d) {}
    bool range(size_t o, size_t n) const { return o <= d_.size() && n <= d_.size() - o; }
    uint32_t u32(size_t o) const { uint32_t v = 0; std::memcpy(&v, d_.data() + o, 4); return v; }
    uint16_t u16(size_t o) const { uint16_t v = 0; std::memcpy(&v, d_.data() + o, 2); return v; }
    int16_t s16(size_t o) const { return static_cast<int16_t>(u16(o)); }
    float f32(size_t o) const { float v = 0; std::memcpy(&v, d_.data() + o, 4); return v; }
    const uint8_t* bytes(size_t o) const { return d_.data() + o; }
};

struct Unpack {
    uint8_t cmd = 0;
    uint16_t imm = 0;
    uint32_t count = 0;
    size_t offset = 0;
    size_t payload = 0;
};

inline bool unpack(const Reader& r, size_t o, Unpack& u) {
    if (!r.range(o, 4)) return false;
    const uint32_t w = r.u32(o);
    u.cmd = static_cast<uint8_t>(w >> 24);
    u.imm = static_cast<uint16_t>(w & 0xffff);
    if ((u.cmd & 0x60) != 0x60) return false;
    const uint8_t fmt = u.cmd & 0xf;
    const uint32_t comps = fmt == 0x0 || fmt == 0x1 || fmt == 0x2 ? 1 :
                           fmt == 0x4 || fmt == 0x5 || fmt == 0x6 ? 2 :
                           fmt == 0x8 || fmt == 0x9 || fmt == 0xa ? 3 :
                           fmt == 0xc || fmt == 0xd || fmt == 0xe ? 4 :
                           fmt == 0xf ? 1 : 0;
    const uint32_t bits = fmt == 0x0 || fmt == 0x8 || fmt == 0xc ? 32 :
                          fmt == 0x1 || fmt == 0x5 || fmt == 0x9 || fmt == 0xd || fmt == 0xf ? 16 :
                          fmt == 0x2 || fmt == 0x6 || fmt == 0xa || fmt == 0xe ? 8 : 0;
    if (!comps || !bits) return false;
    u.count = (w >> 16) & 0xff;
    if (!u.count) u.count = 256;
    const uint64_t words = (static_cast<uint64_t>(u.count) * comps * bits + 31) / 32;
    if (words > (std::numeric_limits<size_t>::max() - 4) / 4) return false;
    u.offset = o;
    u.payload = static_cast<size_t>(words) * 4;
    return r.range(o + 4, u.payload);
}

inline bool command(const Reader& r, size_t o, uint8_t& cmd, uint16_t& imm, uint32_t& count, size_t& size) {
    if (!r.range(o, 4)) return false;
    const uint32_t w = r.u32(o); cmd = static_cast<uint8_t>(w >> 24); imm = static_cast<uint16_t>(w);
    count = (w >> 16) & 0xff; if (!count) count = 256;
    if ((cmd & 0x60) == 0x60) {
        Unpack u; if (!unpack(r, o, u)) return false;
        size = 4 + u.payload; return true;
    }
    if (cmd == 0x17 || cmd == 0x10 || cmd == 0x11 || cmd == 0x13 || cmd == 0x14 || cmd == 0x15) { size = 4; return true; }
    if (cmd == 0x20 || cmd == 0x30 || cmd == 0x31) { size = 4 + (cmd == 0x20 ? 4 : 16); return r.range(o, size); }
    return false;
}

inline bool finitePositions(const Reader& r, size_t p, uint32_t n) {
    for (uint32_t i = 0; i < n; ++i) for (int c = 0; c < 4; ++c) {
        const float v = r.f32(p + 16u * i + 4u * c);
        if (!std::isfinite(v) || std::fabs(v) > 2e5f) return false;
    }
    return true;
}

inline void addVertex(Mesh& out, const Reader& r, size_t pos, size_t color, size_t uv,
                      const std::vector<std::array<uint8_t, 3>>& ids,
                      const std::vector<std::array<float, 3>>& weights, uint8_t index, int tex) {
    const float x = r.f32(pos + 16u * index), y = r.f32(pos + 16u * index + 4), z = r.f32(pos + 16u * index + 8);
    float cr = 1, cg = 1, cb = 1, ca = 1;
    if (color) {
        const uint16_t c = r.u16(color + 2u * index);
        cr = ((c >> 0) & 31) * 8.0f / 128.0f; cg = ((c >> 5) & 31) * 8.0f / 128.0f;
        cb = ((c >> 10) & 31) * 8.0f / 128.0f;
    }
    out.vertices.insert(out.vertices.end(), {x, y, z, uv ? r.s16(uv + 4u * index) / 4096.0f : 0.0f,
        uv ? 1.0f - r.s16(uv + 4u * index + 2) / 4096.0f : 0.0f, cr, cg, cb, ca, static_cast<float>(tex)});
    if (!ids.empty()) { out.boneIds.insert(out.boneIds.end(), ids[index].begin(), ids[index].end()); out.weights.insert(out.weights.end(), weights[index].begin(), weights[index].end()); }
}

// Carga una cadena rider a partir de un offset que apunta al V4-32 de posiciones.
// `chainOffset` y todos los offsets internos son offsets de archivo, no punteros EE.
inline bool load(const std::vector<uint8_t>& data, size_t chainOffset, Mesh& out) {
    out = Mesh(); Reader r(data); Unpack pos, normal;
    if (!unpack(r, chainOffset, pos) || pos.cmd != 0x6c || pos.count < 16 ||
        !finitePositions(r, chainOffset + 4, pos.count)) return false;
    size_t o = chainOffset + 4 + pos.payload;
    if (!unpack(r, o, normal) || normal.cmd != 0x6a || normal.count != pos.count) return false;
    o += 4 + normal.payload; out.positionCount = pos.count;
    std::vector<std::array<uint8_t, 3>> ids(pos.count); std::vector<std::array<float, 3>> weights(pos.count);
    for (uint32_t i = 0; i < pos.count; ++i) {
        const uint32_t x = r.u32(chainOffset + 4 + 16u * i), y = r.u32(chainOffset + 8 + 16u * i), z = r.u32(chainOffset + 12 + 16u * i);
        const float w = r.f32(chainOffset + 16 + 16u * i), w2 = w * 2048.0f - std::floor(w * 2048.0f);
        ids[i] = {static_cast<uint8_t>((x & 127u) >> 2), static_cast<uint8_t>((y & 127u) >> 2), static_cast<uint8_t>((z & 127u) >> 2)};
        weights[i] = {w, w2, 1.0f - w - w2};
    }
    size_t vertexPos = chainOffset + 4, color = 0, uv = 0; uint32_t hdr = 0; int tex = 0;
    std::vector<uint8_t> strip; std::vector<bool> adc;
    while (o < data.size()) {
        Unpack u;
        if (unpack(r, o, u)) {
            if (u.cmd == 0x6c && u.count >= 16) break;
            if (u.cmd == 0x6c && u.count == 1) { hdr = u.imm & 0x3ff; if (r.range(o + 16, 4)) { const uint32_t sel = r.u32(o + 16); if (sel) tex = 0; } }
            else if (u.cmd == 0x62 && u.count >= 3 && strip.empty()) strip.assign(r.bytes(o + 4), r.bytes(o + 4 + u.count));
            else if (u.cmd == 0x6f && !strip.empty() && u.count == strip.size()) { color = o + 4; }
            else if (u.cmd == 0x65 && !strip.empty() && u.count == strip.size()) { uv = o + 4; }
            else if (u.cmd == 0x62 && !strip.empty() && u.count <= 3 && hdr) {
                const int first = static_cast<int>((u.imm & 0x3ff) - (hdr + 3)) / 3;
                adc.assign(pos.count, false); for (uint32_t i = 0; i < u.count && first + static_cast<int>(i) >= 0 && first + static_cast<int>(i) < static_cast<int>(adc.size()); ++i) adc[first + i] = true;
            }
            o += 4 + u.payload; continue;
        }
        uint8_t cmd; uint16_t imm; uint32_t count; size_t size;
        if (!command(r, o, cmd, imm, count, size)) break;
        if ((cmd == 0x17 || cmd == 0x14 || cmd == 0x15) && !strip.empty()) {
            for (size_t j = 2; j < strip.size(); ++j) {
                if (j < adc.size() && adc[j]) continue;
                uint8_t a = strip[j - 2], b = strip[j - 1], c = strip[j];
                if (j & 1) std::swap(b, c);
                if (a >= pos.count || b >= pos.count || c >= pos.count || a == b || a == c || b == c) continue;
                const uint8_t tri[3] = {a, b, c};
                for (uint8_t k : tri) addVertex(out, r, vertexPos, color, uv, ids, weights, k, tex);
                out.indices.insert(out.indices.end(), {static_cast<uint32_t>(out.indices.size()), static_cast<uint32_t>(out.indices.size() + 1), static_cast<uint32_t>(out.indices.size() + 2)});
            }
            strip.clear(); adc.clear(); color = uv = 0; hdr = 0; ++out.packetCount;
        }
        o += size;
    }
    return !out.vertices.empty() && out.indices.size() == out.vertices.size() / 10;
}
} // namespace rider_mesh
