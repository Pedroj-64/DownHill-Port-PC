// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Lector nativo de .NGA (animaciones): tabla de clips, pistas y muestreo. Mismo algoritmo que tools/nga.py (docs/formats/nga.md, evaluadores FUN_00268968/00268A68/002691A8, stub 0020C5E0).
// Sólo las combinaciones (tipo, modo) presentes en los datos: (3,0) constante, (2,2) muestras u8, (4,2) hermite u8 con una tangente, (1,2) hermite u8 con dos. Todo con comprobación de límites.
#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <vector>

namespace nga {
struct Key { float t, v, a, b; };
struct Track { int type = 0, mode = 0, channel = 0; std::vector<Key> k; };
struct Clip { int id = 0; float dur = 0; std::vector<Track> tracks; };

struct File {
    std::vector<Clip> clips;
    const Clip* find(int id) const { for (auto& c : clips) if (c.id == id) return &c; return nullptr; }
    bool load(const std::vector<uint8_t>& d) {
        clips.clear(); uint32_t magic; if (!rd(d, 0, &magic, 4)) return false; uint32_t n = magic >> 16;
        std::vector<uint32_t> offs; std::vector<std::pair<int, uint32_t>> ent;
        for (uint32_t i = 0; i < n; i++) { uint16_t id; uint32_t o; if (!rd(d, 4 + 8*i, &id, 2) || !rd(d, 8 + 8*i, &o, 4) || o + 0x14 > d.size()) return false; ent.push_back({id, o}); offs.push_back(o); }
        for (auto& e : ent) {
            Clip c; c.id = e.first; uint32_t o = e.second; uint16_t cid, nt; if (!rd(d, o + 2, &cid, 2) || !rd(d, o + 8, &c.dur, 4) || !rd(d, o + 0xc, &nt, 2)) return false;
            size_t pos = o; bool ok = true;
            for (uint16_t k = 0; k < nt && ok; k++) {
                uint16_t cnt; if (!rd(d, o + 0x12 + 2*k, &cnt, 2) || (size_t)cnt * 4 > pos) { ok = false; break; }
                pos -= (size_t)cnt * 4; Track t; if (!track(d, pos, t)) { ok = false; break; } c.tracks.push_back(std::move(t));
            }
            if (!ok) continue;        // pista con evaluador desconocido: el clip se omite (el motor tiene más tipos; no aparecen en los datos)
            clips.push_back(std::move(c));
        }
        return true;
    }
    static float sample(const Track& t, float x) {
        const auto& k = t.k; if (k.empty()) return 0; if (t.type == 3 || x <= k.front().t) return k.front().v; if (x >= k.back().t) return k.back().v;
        size_t i = 0; while (i + 2 < k.size() && k[i + 1].t <= x) i++;
        float dt = k[i+1].t - k[i].t; if (dt <= 0) return k[i].v; float u = (x - k[i].t) / dt;
        if (t.type == 2) return k[i].v + (k[i+1].v - k[i].v) * u;
        float p0 = k[i].v, p1 = k[i+1].v, m0, m1;
        if (t.type == 1) { m0 = k[i].a; m1 = k[i].b; } else { m0 = k[i].a * dt; m1 = k[i+1].a * dt; }   // (1,2): tanA_k, tanB_k tal cual; (4,2): una tangente por clave * dt
        float dd = p1 - p0; return u * (u * (u * ((m0 + m1) - 2*dd) + (3*dd - (2*m0 + m1))) + m0) + p0;
    }
    // pose[canal] = valor de la pista en x (los canales sin pista no se tocan)
    static void pose(const Clip& c, float x, std::vector<float>& out) { for (auto& t : c.tracks) { if (t.channel >= 0 && (size_t)t.channel < out.size()) out[t.channel] = sample(t, x); } }
private:
    template <class T> static bool rd(const std::vector<uint8_t>& d, size_t o, T* p, size_t n) { if (o + n > d.size()) return false; std::memcpy(p, d.data() + o, n); return true; }
    static float tanw(uint16_t x) {      // FUN_00268EF8 y vecinas: |x| < 16384 -> x/16384; códigos 01 / 10 en los 2 bits altos -> 16384/(±32768 - x)
        int sx = x >= 32768 ? (int)x - 65536 : (int)x; int code = sx >> 14;
        if (code == -2) return 16384.f / (-32768.f - sx); if (code == 1) return 16384.f / (32768.f - sx); return sx * 6.1035156e-05f;
    }
    static bool track(const std::vector<uint8_t>& d, size_t pos, Track& t) {
        uint16_t fl, ch, n; if (!rd(d, pos, &fl, 2) || !rd(d, pos + 2, &ch, 2) || !rd(d, pos + 4, &n, 2)) return false;
        t.type = fl & 7; t.mode = fl >> 3 & 7; t.channel = ch;
        if (t.type == 3 && t.mode == 0) { float v; if (!rd(d, pos + 4, &v, 4)) return false; t.k = {{0, v, 0, 0}}; return true; }   // constante: el u16 n es la mitad alta del f32
        float h[4]; if (pos < 16 || !rd(d, pos - 16, h, 16)) return false;   // cabecera cuantizada: t0, dt, v0, dv
        if (t.mode != 2) return false;
        for (uint16_t i = 0; i < n; i++) {
            if (t.type == 2) { uint8_t q; if (!rd(d, pos + 6 + i, &q, 1)) return false; t.k.push_back({h[0] + i * h[1], q * h[3] + h[2], 0, 0}); }
            else if (t.type == 4) { uint8_t tq, vq; uint16_t a; size_t o = pos + 6 + 4*i; if (!rd(d, o, &tq, 1) || !rd(d, o + 1, &vq, 1) || !rd(d, o + 2, &a, 2)) return false; t.k.push_back({tq * h[1] + h[0], vq * h[3] + h[2], tanw(a), 0}); }
            else if (t.type == 1) { uint8_t tq, vq; uint16_t a, b; size_t o = pos + 6 + 6*i; if (!rd(d, o, &tq, 1) || !rd(d, o + 1, &vq, 1) || !rd(d, o + 2, &a, 2) || !rd(d, o + 4, &b, 2)) return false; t.k.push_back({tq * h[1] + h[0], vq * h[3] + h[2], tanw(a), tanw(b)}); }
            else return false;
        }
        return true;
    }
};
}  // namespace nga
