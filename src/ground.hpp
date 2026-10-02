// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Colisión del suelo: malla de triángulos del nivel (tools/collision.py -> .col) y barrido de esfera como lo hace el motor original.
// Layout, citas del ELF y diferencias con el original: docs/formats/collision.md.
// .col = u32 n, n * { f32 v[9]; u16 surface; u16 pad } en el espacio del NGP (Z arriba). load() lo convierte a Y arriba: (x,y,z) -> (x,z,-y).
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <vector>

struct V3 { float x = 0, y = 0, z = 0; };
inline V3 operator+(V3 a, V3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline V3 operator-(V3 a, V3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline V3 operator*(V3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
inline float dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline V3 cross(V3 a, V3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }

// Impacto de un barrido. Los campos corresponden al registro de 0x30 B del motor (FUN_00219970): punto +0x10, fracción +0x1C, normal +0x20, surface u16 en +6.
// frac < 0 (kOverlap) = la esfera ya solapaba al empezar (el motor guarda -FLT_MAX: DAT_002C7C64 / DAT_002C7C68).
struct SweepHit { bool hit = false; float frac = 1; V3 point, normal; float pen = 0; uint16_t surface = 0; uint32_t tri = 0; };
inline constexpr float kOverlap = -3.4028235e38f;
inline constexpr float kSkin = 0.01f;   // FUN_002193C0 / FUN_00218E00: holgura 0.01 u (0x3C23D70A)

class Ground {
public:
    struct Tri { float v[9]; uint16_t surface; };
    static constexpr float kCell = 100.f;   // celda del índice XZ propio (el motor usa su rejilla de objetos de 100 u; ver collision.md)

    bool load(const std::vector<uint8_t>& raw, bool zup = true) {
        if (raw.size() < 4) return false;
        uint32_t n; std::memcpy(&n, raw.data(), 4);
        if (raw.size() != 4 + (size_t)n * 40) return false;
        tris_.resize(n);
        for (uint32_t i = 0; i < n; i++) {
            const uint8_t* p = raw.data() + 4 + (size_t)i * 40; std::memcpy(tris_[i].v, p, 36); std::memcpy(&tris_[i].surface, p + 36, 2);
            if (zup) for (int k = 0; k < 3; k++) { float y = tris_[i].v[3*k+1], z = tris_[i].v[3*k+2]; tris_[i].v[3*k+1] = z; tris_[i].v[3*k+2] = -y; }
        }
        index();
        return true;
    }
    void set(std::vector<Tri> t) { tris_ = std::move(t); index(); }
    const std::vector<Tri>& tris() const { return tris_; }
    V3 boundsMin() const { return bmin_; }
    V3 boundsMax() const { return bmax_; }

    // Barrido de esfera de radio r de p0 a p1: el impacto de MENOR fracción (FUN_00217450; a igualdad, el de menor índice) entre todos los triángulos cuya caja
    // toca la caja barrida [min(p0,p1)-r, max(p0,p1)+r] (FUN_0021A908 / FUN_00219970). Un triángulo sólo bloquea por su cara frontal (FUN_002193C0: dist0 < 0 -> sin impacto).
    SweepHit sweep(V3 p0, V3 p1, float r) const {
        SweepHit best; V3 D = p1 - p0;
        V3 lo{std::fmin(p0.x, p1.x) - r, std::fmin(p0.y, p1.y) - r, std::fmin(p0.z, p1.z) - r}, hi{std::fmax(p0.x, p1.x) + r, std::fmax(p0.y, p1.y) + r, std::fmax(p0.z, p1.z) + r};
        float tmax = 1.f;
        std::vector<uint32_t> cand; gather(lo, hi, cand);
        for (uint32_t i : cand) {
            const float* f = tris_[i].v; V3 v[3] = {{f[0], f[1], f[2]}, {f[3], f[4], f[5]}, {f[6], f[7], f[8]}};
            if (std::fmin(v[0].x, std::fmin(v[1].x, v[2].x)) > hi.x || std::fmax(v[0].x, std::fmax(v[1].x, v[2].x)) < lo.x ||
                std::fmin(v[0].y, std::fmin(v[1].y, v[2].y)) > hi.y || std::fmax(v[0].y, std::fmax(v[1].y, v[2].y)) < lo.y ||
                std::fmin(v[0].z, std::fmin(v[1].z, v[2].z)) > hi.z || std::fmax(v[0].z, std::fmax(v[1].z, v[2].z)) < lo.z) continue;
            SweepHit h;
            if (!sweepTri(v, p0, p1, D, r, tmax, lo, hi, h)) continue;
            if (!best.hit || h.frac < best.frac || (h.frac == best.frac && i < best.tri)) {
                best = h; best.hit = true; best.tri = i; best.surface = tris_[i].surface; tmax = std::fmax(h.frac, 0.f);
            }
        }
        return best;
    }

    // Altura/normal/superficie bajo (x,y,z): barrido de esfera r hacia abajo `reach` unidades. Sin constantes ocultas: r y reach los decide quien llama
    // (en el motor son el radio del punto de contacto de la rueda y el desplazamiento del sub-paso, FUN_001340D8).
    struct GroundInfo { bool hit = false; float height = 0; V3 normal{0, 1, 0}; uint16_t surface = 0; float frac = 1; };
    GroundInfo groundQuery(float x, float y, float z, float r, float reach) const {
        SweepHit h = sweep({x, y, z}, {x, y - reach, z}, r); GroundInfo g;
        if (!h.hit) return g;
        g.hit = true; g.normal = h.normal; g.surface = h.surface; g.frac = h.frac; g.height = h.frac < 0 ? y : y - reach * h.frac - r;   // centro -> altura del suelo bajo el centro de la esfera
        return g;
    }

    // Alturas de TODOS los triángulos (sin importar su cara) bajo (x,z): sólo para validación/herramientas (la malla visual tiene devanado mixto).
    std::vector<float> verticalHeights(float x, float z) const {
        std::vector<float> out; auto it = grid_.find(key(cellOf(x), cellOf(z))); if (it == grid_.end()) return out;
        for (uint32_t i : it->second) {
            const float* a = tris_[i].v; const float *b = a + 3, *c = a + 6;
            float d = (b[2]-c[2])*(a[0]-c[0]) + (c[0]-b[0])*(a[2]-c[2]); if (std::fabs(d) < 1e-9f) continue;
            float l1 = ((b[2]-c[2])*(x-c[0]) + (c[0]-b[0])*(z-c[2])) / d, l2 = ((c[2]-a[2])*(x-c[0]) + (a[0]-c[0])*(z-c[2])) / d, l3 = 1 - l1 - l2;
            if (l1 >= 0 && l2 >= 0 && l3 >= 0) out.push_back(l1*a[1] + l2*b[1] + l3*c[1]);
        }
        return out;
    }

private:
    std::vector<Tri> tris_; std::unordered_map<uint64_t, std::vector<uint32_t>> grid_; V3 bmin_, bmax_;
    static int cellOf(float v) { return (int)std::floor(v / kCell); }
    static uint64_t key(int cx, int cz) { return ((uint64_t)(uint32_t)cx << 32) | (uint32_t)cz; }
    void index() {
        grid_.clear(); bmin_ = {1e30f, 1e30f, 1e30f}; bmax_ = {-1e30f, -1e30f, -1e30f};
        for (uint32_t i = 0; i < tris_.size(); i++) {
            const float* v = tris_[i].v;
            for (int k = 0; k < 3; k++) { bmin_.x = std::fmin(bmin_.x, v[3*k]); bmax_.x = std::fmax(bmax_.x, v[3*k]); bmin_.y = std::fmin(bmin_.y, v[3*k+1]); bmax_.y = std::fmax(bmax_.y, v[3*k+1]); bmin_.z = std::fmin(bmin_.z, v[3*k+2]); bmax_.z = std::fmax(bmax_.z, v[3*k+2]); }
            float lx = std::fmin(v[0], std::fmin(v[3], v[6])), hx = std::fmax(v[0], std::fmax(v[3], v[6])), lz = std::fmin(v[2], std::fmin(v[5], v[8])), hz = std::fmax(v[2], std::fmax(v[5], v[8]));
            for (int cz = cellOf(lz); cz <= cellOf(hz); cz++) for (int cx = cellOf(lx); cx <= cellOf(hx); cx++) grid_[key(cx, cz)].push_back(i);
        }
    }
    void gather(V3 lo, V3 hi, std::vector<uint32_t>& out) const {
        for (int cz = cellOf(lo.z); cz <= cellOf(hi.z); cz++) for (int cx = cellOf(lo.x); cx <= cellOf(hi.x); cx++) { auto it = grid_.find(key(cx, cz)); if (it != grid_.end()) out.insert(out.end(), it->second.begin(), it->second.end()); }
        std::sort(out.begin(), out.end()); out.erase(std::unique(out.begin(), out.end()), out.end());
    }

    // punto dentro del triángulo (FUN_002193C0): c0=(C-v0)x(C-v1), c1=(C-v1)x(C-v2), c2=(C-v2)x(C-v0); c0.c1 > 0 y c0.c2 > 0 (cualquier devanado)
    static bool inside(const V3 v[3], V3 c) {
        V3 c0 = cross(c - v[0], c - v[1]), c1 = cross(c - v[1], c - v[2]), c2 = cross(c - v[2], c - v[0]);
        return dot(c0, c1) > 0.f && dot(c0, c2) > 0.f;
    }
    // FUN_00218E00: la esfera ya solapa la arista (v,v+E) al empezar. a=E.E, b=S.E, S=p0-v
    static bool edgeOverlap(float a, float b, float r, V3 v, V3 E, V3 S, V3 p0, SweepHit& o) {
        if (!(b < a)) return false;
        V3 closest = b < 0 ? v : v + E * (b / a), n = p0 - closest; float l2 = dot(n, n);
        if (r * r < l2) return false;
        float l = std::sqrt(l2); if (l == 0) return false;
        o.normal = n * (1.f / l); o.pen = (r - l) + kSkin; o.point = closest + o.normal * kSkin; o.frac = kOverlap; return true;
    }
    // FUN_00218FB8: barrido contra la arista (v,w) con prefiltro de cajas: solape inicial, cilindro (cuadrática) y, si falla, la esfera del vértice v
    static bool sweepEdge(float r, float tmax, V3 p0, V3 D, V3 lo, V3 hi, V3 v, V3 w, SweepHit& o) {
        float vl[3] = {v.x, v.y, v.z}, wl[3] = {w.x, w.y, w.z}, bl[3] = {lo.x, lo.y, lo.z}, bh[3] = {hi.x, hi.y, hi.z};
        for (int a = 0; a < 3; a++) { if (std::fmax(vl[a], wl[a]) < bl[a] || bh[a] < std::fmin(vl[a], wl[a])) return false; }
        V3 E = w - v, S = p0 - v; float a = dot(E, E), b = dot(S, E);
        if (edgeOverlap(a, b, r, v, E, S, p0, o)) return true;
        float ED = dot(E, D), SD = dot(S, D), SS = dot(S, S), DD = dot(D, D);
        float A = ED * ED - a * DD, B = 2.f * (b * ED - SD * a);
        if (std::fabs(A) > 1e-4f) {
            float disc = B * B - 4.f * A * ((b * b + r * r * a) - SS * a);
            if (disc <= 0.f) return false;
            float sq = std::sqrt(disc), t1 = (-B - sq) / (2.f * A), t2 = (-B + sq) / (2.f * A), t = std::fmin(t1, t2);
            if (t < 0.f || tmax < t) return false;
            float s = dot((p0 + D * t) - v, E) / a;
            if (s >= 0.f && s <= 1.f) { o.point = v + E * s; o.frac = t; return true; }
        }
        float B2 = 2.f * SD;
        if (std::fabs(DD) < 1e-4f) return false;
        float disc = B2 * B2 - 4.f * DD * (SS - r * r);
        if (disc <= 0.f) return false;
        float sq = std::sqrt(disc), t = std::fmin((-B2 - sq) / (2.f * DD), (-B2 + sq) / (2.f * DD));
        if (t < 0.f) return false;
        if (t <= tmax) { o.point = v; o.frac = t; return true; }
        return false;
    }
    // FUN_002193C0: cara (un solo lado; normal = (v2-v1)x(v0-v1), signo elegido empíricamente, ver collision.md), luego aristas/vértices
    static bool sweepTri(const V3 v[3], V3 p0, V3 p1, V3 D, float r, float tmax, V3 lo, V3 hi, SweepHit& out) {
        V3 n = cross(v[2] - v[1], v[0] - v[1]); float l = std::sqrt(dot(n, n)); if (l == 0) return false;
        n = n * (1.f / l); float d1 = dot(n, v[1]);
        float dist0 = dot(n, p0) - d1;
        if (dist0 < 0.f) return false;
        float dist = dist0 - r;
        if (dist <= 0.f) {                                                    // ya solapa el plano al empezar
            V3 c = p0 + n * ((kSkin - dist) - r);
            if (inside(v, c)) { out.point = c; out.normal = n; out.pen = kSkin - dist; out.frac = kOverlap; return true; }
        } else {
            float dist1 = dot(n, p1) - d1 - r;
            if (dist1 > 0.f) return false;                                    // el final sigue por delante del plano: no hay impacto (ni aristas)
            float t = dist / (dist - dist1);
            if (t < 0.f || t > 1.f) return false;
            float tb = t - kSkin / (dist - dist1);
            if (tmax < tb) return false;
            V3 base = p0 - n * r;
            if (inside(v, base + D * t)) { float f = std::fmax(tb, 0.f); out.point = base + D * f; out.normal = n; out.pen = kSkin - dist1; out.frac = f; return true; }
        }
        if (r == 0.f) return false;
        SweepHit best; bool any = false; float tcur = tmax;
        for (int i = 0; i < 3; i++) {
            SweepHit h;
            if (sweepEdge(r, tcur, p0, D, lo, hi, v[i], v[(i + 1) % 3], h) && (!any || h.frac < best.frac)) { best = h; any = true; tcur = std::fmax(h.frac, 0.f); }
        }
        if (!any) return false;
        if (best.frac >= 0.f) { V3 pt = p0 + D * best.frac; best.normal = (pt - best.point) * (1.f / r); best.pen = 0; }
        out = best; return true;
    }
};
