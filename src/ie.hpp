// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Contenedores IE del juego (tools/unpack_ie.py): cabecera «IE\x03\x04» + variante u32 + nombre; variante 0x0A = otra capa IE sin comprimir; el resto = deflate crudo (RFC 1951). Inflate propio, sin zlib:
// acotado (límite de salida y de capas), sin lecturas fuera de rango. Mismo resultado que Python: si una capa falla, unpack() devuelve false y el llamador usa los bytes originales (como main() de unpack_ie.py).
#pragma once
#include <cstdint>
#include <cstring>
#include <vector>

namespace ie {
struct Bits {
    const uint8_t* p; size_t n, pos = 0; uint32_t buf = 0; int cnt = 0; bool err = false;
    int get(int k) { while (cnt < k) { if (pos >= n) { err = true; return 0; } buf |= (uint32_t)p[pos++] << cnt; cnt += 8; } const int v = (int)(buf & ((1u << k) - 1)); buf >>= k; cnt -= k; return v; }
};
struct Huff { uint16_t count[16] = {}, symbol[288] = {}; };
inline bool build(Huff& h, const uint8_t* len, int n) {
    std::memset(h.count, 0, sizeof h.count); for (int i = 0; i < n; i++) h.count[len[i]]++; h.count[0] = 0;
    int left = 1; for (int l = 1; l < 16; l++) { left <<= 1; left -= h.count[l]; if (left < 0) return false; }   // sobresuscrito
    uint16_t offs[16]; offs[1] = 0; for (int l = 1; l < 15; l++) offs[l + 1] = (uint16_t)(offs[l] + h.count[l]);
    for (int i = 0; i < n; i++) if (len[i]) h.symbol[offs[len[i]]++] = (uint16_t)i;
    return true;
}
inline int decodeSym(Bits& b, const Huff& h) {
    int code = 0, first = 0, index = 0;
    for (int l = 1; l < 16; l++) { code |= b.get(1); if (b.err) return -1; const int c = h.count[l]; if (code - c < first) return h.symbol[index + (code - first)]; index += c; first += c; first <<= 1; code <<= 1; }
    return -1;
}
// Deflate crudo desde `in[0..n)`; devuelve false si el flujo es inválido o supera `maxOut`. Los bytes sobrantes tras el último bloque se ignoran (como zlib.decompressobj).
inline bool inflate(const uint8_t* in, size_t n, std::vector<uint8_t>& out, size_t maxOut = (size_t)1 << 30) {
    static const uint16_t lbase[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258}, lext[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
    static const uint16_t dbase[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577}, dext[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
    Bits b{in, n}; out.clear(); int last;
    do {
        last = b.get(1); const int type = b.get(2); if (b.err) return false;
        if (type == 0) {
            b.buf = 0; b.cnt = 0; if (b.pos + 4 > n) return false; const unsigned len = in[b.pos] | in[b.pos + 1] << 8, nlen = in[b.pos + 2] | in[b.pos + 3] << 8; b.pos += 4;
            if (len != (~nlen & 0xffff) || b.pos + len > n || out.size() + len > maxOut) return false; out.insert(out.end(), in + b.pos, in + b.pos + len); b.pos += len;
        } else if (type == 1 || type == 2) {
            Huff lit, dist;
            if (type == 1) { uint8_t l[288]; for (int i = 0; i < 144; i++) l[i] = 8; for (int i = 144; i < 256; i++) l[i] = 9; for (int i = 256; i < 280; i++) l[i] = 7; for (int i = 280; i < 288; i++) l[i] = 8; build(lit, l, 288); uint8_t d[30]; for (int i = 0; i < 30; i++) d[i] = 5; build(dist, d, 30); }
            else {
                const int nlen = b.get(5) + 257, ndist = b.get(5) + 1, ncode = b.get(4) + 4; if (b.err || nlen > 286 || ndist > 30) return false;
                static const uint8_t order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15}; uint8_t l[320] = {}; for (int i = 0; i < ncode; i++) l[order[i]] = (uint8_t)b.get(3); if (b.err) return false;
                Huff lc; if (!build(lc, l, 19)) return false; uint8_t ln[320] = {}; int idx = 0;
                while (idx < nlen + ndist) {
                    int sym = decodeSym(b, lc); if (sym < 0) return false;
                    if (sym < 16) ln[idx++] = (uint8_t)sym;
                    else { int rep, val = 0; if (sym == 16) { if (idx == 0) return false; val = ln[idx - 1]; rep = 3 + b.get(2); } else if (sym == 17) rep = 3 + b.get(3); else rep = 11 + b.get(7); if (b.err || idx + rep > nlen + ndist) return false; while (rep--) ln[idx++] = (uint8_t)val; }
                }
                if (ln[256] == 0 || !build(lit, ln, nlen) || !build(dist, ln + nlen, ndist)) return false;
            }
            for (;;) {
                int sym = decodeSym(b, lit); if (sym < 0) return false;
                if (sym < 256) { if (out.size() + 1 > maxOut) return false; out.push_back((uint8_t)sym); }
                else if (sym == 256) break;
                else {
                    sym -= 257; if (sym >= 29) return false; const int len = lbase[sym] + b.get(lext[sym]); const int ds = decodeSym(b, dist); if (ds < 0 || ds >= 30 || b.err) return false;
                    const size_t d = (size_t)dbase[ds] + (size_t)b.get(dext[ds]); if (b.err || d > out.size() || out.size() + (size_t)len > maxOut) return false;
                    for (int k = 0; k < len; k++) out.push_back(out[out.size() - d]);
                }
            }
        } else return false;
    } while (!last);
    return true;
}
// unpack_ie_data(): desenvuelve capas IE hasta que los datos ya no empiezan por la magia. false = capa inválida (usar los bytes originales).
inline bool unpack(const std::vector<uint8_t>& src, std::vector<uint8_t>& out, int maxLayers = 64) {
    std::vector<uint8_t> data = src;
    for (int layer = 0; layer < maxLayers; layer++) {
        if (data.size() < 4 || std::memcmp(data.data(), "IE\x03\x04", 4) != 0) { out = std::move(data); return true; }
        if (data.size() < 0x1e) return false;
        uint32_t variant, nameLen; std::memcpy(&variant, &data[4], 4); std::memcpy(&nameLen, &data[0x1a], 4);
        const size_t start = 0x1e + (size_t)nameLen; if (start > data.size()) return false;
        if (variant == 0x0a) data.erase(data.begin(), data.begin() + (long)start);
        else { std::vector<uint8_t> o; if (!inflate(data.data() + start, data.size() - start, o)) return false; data = std::move(o); }
    }
    return false;
}
}  // namespace ie
