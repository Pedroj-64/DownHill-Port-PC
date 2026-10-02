// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Colisión del suelo: malla de triángulos exportada por tools/collision.py (.col). Layout y citas del ELF: docs/formats/collision.md.
// .col = u32 n, n * { f32 v[9]; u16 surface; u16 pad }.  Y es arriba.
#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <vector>

struct GroundHit { bool hit = false; float height = 0, nx = 0, ny = 1, nz = 0; uint16_t surface = 0; uint32_t tri = 0; };

class Ground {
public:
    struct Tri { float v[9]; uint16_t surface; };
    static constexpr float kCell = 100.f;   // celda del índice XZ (la rejilla del juego también usa 100 u)

    bool load(const std::vector<uint8_t>& raw) {
        if (raw.size() < 4) return false;
        uint32_t n; std::memcpy(&n, raw.data(), 4);
        if (raw.size() != 4 + (size_t)n * 40) return false;
        tris_.resize(n);
        for (uint32_t i = 0; i < n; i++) { const uint8_t* p = raw.data() + 4 + (size_t)i * 40; std::memcpy(tris_[i].v, p, 36); std::memcpy(&tris_[i].surface, p + 36, 2); }
        index();
        return true;
    }
    void set(std::vector<Tri> t) { tris_ = std::move(t); index(); }
    const std::vector<Tri>& tris() const { return tris_; }

    // Equivalente estático del barrido vertical del juego (FUN_0021a908 + FUN_00217450: gana el impacto de menor tiempo, a igual tiempo el de menor dirección):
    // segmento descendente desde y+margin; el primer triángulo alcanzado es el más alto con altura <= y+margin. Sin historial no hay otra forma de elegir lámina.
    GroundHit query(float x, float y, float z, float margin = 30.f) const {
        GroundHit best; auto it = grid_.find(key(cellOf(x), cellOf(z))); if (it == grid_.end()) return best;
        const float top = y + margin;
        for (uint32_t i : it->second) {
            const float* a = tris_[i].v; const float *b = a + 3, *c = a + 6;
            float d = (b[2]-c[2])*(a[0]-c[0]) + (c[0]-b[0])*(a[2]-c[2]); if (std::fabs(d) < 1e-9f) continue;
            float l1 = ((b[2]-c[2])*(x-c[0]) + (c[0]-b[0])*(z-c[2])) / d, l2 = ((c[2]-a[2])*(x-c[0]) + (a[0]-c[0])*(z-c[2])) / d, l3 = 1 - l1 - l2;
            if (l1 < -1e-6f || l2 < -1e-6f || l3 < -1e-6f) continue;
            float h = l1*a[1] + l2*b[1] + l3*c[1];
            if (h > top || (best.hit && (h < best.height || (h == best.height && i > best.tri)))) continue;
            best.hit = true; best.height = h; best.tri = i; best.surface = tris_[i].surface;
        }
        if (best.hit) {
            const float* a = tris_[best.tri].v; const float *b = a + 3, *c = a + 6;
            float ux = b[0]-a[0], uy = b[1]-a[1], uz = b[2]-a[2], wx = c[0]-a[0], wy = c[1]-a[1], wz = c[2]-a[2];
            float nx = uy*wz - uz*wy, ny = uz*wx - ux*wz, nz = ux*wy - uy*wx, l = std::sqrt(nx*nx + ny*ny + nz*nz);
            if (l > 0) { if (ny < 0) { nx = -nx; ny = -ny; nz = -nz; } best.nx = nx / l; best.ny = ny / l; best.nz = nz / l; }   // el devanado es mixto: se orienta hacia +Y
        }
        return best;
    }

private:
    std::vector<Tri> tris_; std::unordered_map<uint64_t, std::vector<uint32_t>> grid_;
    static int cellOf(float v) { return (int)std::floor(v / kCell); }
    static uint64_t key(int cx, int cz) { return ((uint64_t)(uint32_t)cx << 32) | (uint32_t)cz; }
    void index() {
        grid_.clear();
        for (uint32_t i = 0; i < tris_.size(); i++) {
            const float* v = tris_[i].v;
            float lx = std::fmin(v[0], std::fmin(v[3], v[6])), hx = std::fmax(v[0], std::fmax(v[3], v[6])), lz = std::fmin(v[2], std::fmin(v[5], v[8])), hz = std::fmax(v[2], std::fmax(v[5], v[8]));
            for (int cz = cellOf(lz); cz <= cellOf(hz); cz++) for (int cx = cellOf(lx); cx <= cellOf(hx); cx++) grid_[key(cx, cz)].push_back(i);
        }
    }
};
