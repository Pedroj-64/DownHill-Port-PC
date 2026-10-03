// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Lector nativo de la cadena VIF del cuerpo del rider en un NGP.
// El formato de la cadena y el empaquetado de piel siguen docs/formats/rider.md.
#pragma once
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
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
    std::vector<uint32_t> batchMaterial; // palabra TEX0/selector del lote
};

struct TextureResolver {
    struct Material { size_t offset; uint16_t id; uint32_t cbp; bool decodable; };
    std::vector<Material> materials;
    mutable std::vector<std::pair<uint16_t, uint32_t>> firstKeys;

    int resolveGroup(uint32_t selector, size_t begin, size_t end, int inherited) const {
        std::vector<const Material*> group;
        for (const Material& material : materials)
            if (begin <= material.offset && material.offset < end) group.push_back(&material);
        if (group.empty()) return inherited;
        size_t slot = 0;
        if (selector) slot = static_cast<size_t>(std::countr_zero(selector) / 2);
        const Material& material = *group[std::min(slot, group.size() - 1)];
        if (!material.decodable) return -1;
        const auto key = std::make_pair(material.id, material.cbp);
        auto it = std::find(firstKeys.begin(), firstKeys.end(), key);
        if (it == firstKeys.end()) {
            firstKeys.push_back(key);
            return static_cast<int>(firstKeys.size() - 1);
        }
        return static_cast<int>(it - firstKeys.begin());
    }
};

class Reader {
    const std::vector<uint8_t>& d_;
public:
    explicit Reader(const std::vector<uint8_t>& d) : d_(d) {}
    bool range(size_t o, size_t n) const { return o <= d_.size() && n <= d_.size() - o; }
    bool ptr(size_t o, size_t& out) const {
        if (!range(o, 4)) return false;
        const uint32_t p = u32(o);
        if (p < 0xA00000u || p - 0xA00000u >= d_.size()) return false;
        out = p - 0xA00000u;
        return true;
    }
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
    if (cmd == 0x00 || cmd == 0x01 || cmd == 0x17 || cmd == 0x10 || cmd == 0x11 || cmd == 0x13 || cmd == 0x14 || cmd == 0x15) { size = 4; return true; }
    if (cmd == 0x20) { size = 8; return r.range(o, size); }
    if (cmd == 0x30 || cmd == 0x31) { size = 20; return r.range(o, size); }
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
                      const std::vector<std::array<float, 3>>& weights, uint8_t sourceIndex, size_t attributeIndex, int tex) {
    const float x = r.f32(pos + 16u * sourceIndex), y = r.f32(pos + 16u * sourceIndex + 4), z = r.f32(pos + 16u * sourceIndex + 8);
    float cr = 1, cg = 1, cb = 1, ca = 1;
    if (color) {
        const uint16_t c = r.u16(color + 2u * attributeIndex);
        cr = ((c >> 0) & 31) * 8.0f / 128.0f; cg = ((c >> 5) & 31) * 8.0f / 128.0f;
        cb = ((c >> 10) & 31) * 8.0f / 128.0f;
    }
    out.vertices.insert(out.vertices.end(), {x, y, z, uv ? r.s16(uv + 4u * attributeIndex) / 4096.0f : 0.0f,
        uv ? 1.0f - r.s16(uv + 4u * attributeIndex + 2) / 4096.0f : 0.0f, cr, cg, cb, ca, static_cast<float>(tex)});
    if (!ids.empty()) { out.boneIds.insert(out.boneIds.end(), ids[sourceIndex].begin(), ids[sourceIndex].end()); out.weights.insert(out.weights.end(), weights[sourceIndex].begin(), weights[sourceIndex].end()); }
}

// Carga una cadena rider a partir de un offset que apunta al V4-32 de posiciones.
// `chainOffset` y todos los offsets internos son offsets de archivo, no punteros EE.
inline bool loadRange(const std::vector<uint8_t>& data, size_t rangeBegin, size_t rangeEnd, Mesh& out,
                      const TextureResolver* textures = nullptr) {
    out = Mesh(); Reader r(data); if (rangeBegin > rangeEnd || rangeEnd > data.size()) return false;
    const bool trace = std::getenv("DH_RIDER_TRACE") != nullptr;
    size_t scan = rangeBegin, previousEnd = rangeBegin;
    int inheritedTex = -1;
    while (scan < rangeEnd) {
        Unpack pos;
        if (!unpack(r, scan, pos) || (pos.cmd & 0xf) != 0xc || pos.count < 16 ||
            !finitePositions(r, scan + 4, pos.count)) { ++scan; continue; }
        size_t o = scan + 4 + pos.payload; Unpack normal;
        if (!unpack(r, o, normal) || (normal.cmd & 0xf) != 0xa || normal.count != pos.count) { ++scan; continue; }
        o += 4 + normal.payload; out.positionCount += pos.count;
        std::vector<std::array<uint8_t, 3>> ids(pos.count); std::vector<std::array<float, 3>> weights(pos.count);
        for (uint32_t i = 0; i < pos.count; ++i) {
            const uint32_t x = r.u32(scan + 4 + 16u * i), y = r.u32(scan + 8 + 16u * i), z = r.u32(scan + 12 + 16u * i);
            const float w = r.f32(scan + 16 + 16u * i), w2 = w * 2048.0f - std::floor(w * 2048.0f);
            ids[i] = {static_cast<uint8_t>((x & 127u) >> 2), static_cast<uint8_t>((y & 127u) >> 2), static_cast<uint8_t>((z & 127u) >> 2)};
            weights[i] = {w, w2, 1.0f - w - w2};
        }
        size_t vertexPos = scan + 4, color = 0, uv = 0; uint32_t hdr = 0, material = 0; int tex = textures ? textures->resolveGroup(0, previousEnd, scan, inheritedTex) : 0; bool haveBatch = false;
        inheritedTex = tex;
        std::vector<uint8_t> strip; std::vector<bool> adc;
        while (o < rangeEnd) {
        Unpack u;
        if (unpack(r, o, u)) {
            const uint8_t op = u.cmd & 0xf;
            if (op == 0xc && u.count >= 16) break;
            if (op == 0xc && u.count == 1) {
                hdr = u.imm & 0x3ff;
                if (r.range(o + 16, 4)) {
                    material = r.u32(o + 16); haveBatch = true;
                    tex = textures ? textures->resolveGroup(material, previousEnd, scan, tex) : (material ? static_cast<int>(std::countr_zero(material) / 2) : 0);
                }
            }
            else if (op == 0x2 && u.count >= 3 && strip.empty()) strip.assign(r.bytes(o + 4), r.bytes(o + 4 + u.count));
            else if (op == 0xf && !strip.empty() && u.count == strip.size()) { color = o + 4; }
            else if (op == 0x5 && !strip.empty() && u.count == strip.size()) { uv = o + 4; }
            else if (op == 0x2 && !strip.empty() && u.count <= 3 && hdr) {
                const int first = static_cast<int>(std::floor((static_cast<int>(u.imm & 0x3ff) - static_cast<int>(hdr + 3)) / 3.0));
                if (adc.empty()) adc.assign(strip.size(), false);
                if (first >= 0 && static_cast<size_t>(first) + u.count > adc.size())
                    adc.resize(static_cast<size_t>(first) + u.count, false);
                for (uint32_t i = 0; i < u.count && first + static_cast<int>(i) >= 0; ++i)
                    adc[static_cast<size_t>(first) + i] = true;
            }
            o += 4 + u.payload; continue;
        }
        uint8_t cmd; uint16_t imm; uint32_t count; size_t size;
        if (!command(r, o, cmd, imm, count, size)) break;
        if ((cmd == 0x17 || cmd == 0x14 || cmd == 0x15) && !strip.empty()) {
            size_t batchTriangles = 0;
            uint8_t maxIndex = 0;
            for (uint8_t index : strip) maxIndex = std::max(maxIndex, index);
            if (maxIndex < pos.count) {
                for (size_t j = 2; j < strip.size(); ++j) {
                    if (j < adc.size() && adc[j]) continue;
                    uint8_t a = strip[j - 2], b = strip[j - 1], c = strip[j];
                    if (j & 1) std::swap(b, c);
                    if (a == b || a == c || b == c) continue;
                    const size_t order[3] = {j - 2, j - 1, j};
                    const uint8_t tri[3] = {a, b, c};
                    if (j & 1) { const size_t attributeOrder[3] = {j - 2, j, j - 1}; for (int k = 0; k < 3; ++k) addVertex(out, r, vertexPos, color, uv, ids, weights, tri[k], attributeOrder[k], tex); }
                    else for (int k = 0; k < 3; ++k) addVertex(out, r, vertexPos, color, uv, ids, weights, tri[k], order[k], tex);
                    out.indices.insert(out.indices.end(), {static_cast<uint32_t>(out.indices.size()), static_cast<uint32_t>(out.indices.size() + 1), static_cast<uint32_t>(out.indices.size() + 2)});
                    ++batchTriangles;
                }
            }
            if (trace) {
                size_t flagCount = 0;
                std::fprintf(stderr, "rider batch off=0x%zx num=%u max=%u flags=", o, pos.count, maxIndex);
                for (size_t i = 0; i < adc.size(); ++i) if (adc[i]) { std::fprintf(stderr, "%zu,", i); ++flagCount; }
                std::fprintf(stderr, " count=%zu tris=%zu\n", flagCount, batchTriangles);
            }
            if (haveBatch) out.batchMaterial.push_back(material);
            strip.clear(); adc.clear(); color = uv = 0; hdr = 0; haveBatch = false; ++out.packetCount;
        }
        o += size;
        }
        previousEnd = scan + 4 + pos.payload + 4 + normal.payload;
        scan = o > scan ? o : scan + 4;
    }
    return !out.vertices.empty() && out.indices.size() == out.vertices.size() / 10;
}

inline bool load(const std::vector<uint8_t>& data, size_t chainOffset, Mesh& out,
                 const TextureResolver* textures = nullptr) {
    return loadRange(data, chainOffset, data.size(), out, textures);
}

// Recorrido verificado en tools/export_riders.py: nodo tipo 25 -> grupo en +0x2c
// -> selector tipo 2 -> menor cadena VIF. El kind opcional evita elegir otro rider.
inline bool findChain(const std::vector<uint8_t>& data, size_t& chainOffset, uint32_t wantedKind = 0) {
    Reader r(data);
    for (size_t node = 0; node + 0x30 <= data.size(); node += 4) {
        const uint32_t head = r.u32(node);
        if ((head & 0x3f) != 25 || (wantedKind && (head >> 18) != wantedKind)) continue;
        size_t group;
        if (!r.range(node + 0x2c, 4) || !r.ptr(node + 0x2c, group) || !r.range(group, 0x24) || (r.u32(group) & 0x3f) != 1) continue;
        const uint32_t children = r.u32(group + 8);
        if (children > 64 || !r.range(group + 0x20, static_cast<size_t>(children) * 4)) continue;
        for (uint32_t i = 0; i < children; ++i) {
            size_t selector;
            if (!r.ptr(group + 0x20 + 4u * i, selector) || !r.range(selector, 0x2c) || (r.u32(selector) & 0x3f) != 2) continue;
            const uint32_t entries = r.u32(selector + 4);
            if (entries > 64 || !r.range(selector + 0x28, static_cast<size_t>(entries) * 8)) continue;
            size_t best = data.size();
            for (uint32_t k = 0; k < entries; ++k) {
                size_t chain;
                if (r.ptr(selector + 0x28 + 8u * k, chain)) best = std::min(best, chain);
            }
            if (best < selector) { chainOffset = best; return true; }
        }
    }
    return false;
}
} // namespace rider_mesh
