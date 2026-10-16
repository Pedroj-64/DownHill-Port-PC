// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Audio del juego (paso 4 del plan, docs/formats/game-layer.md): decodificador de PS2 ADPCM (bloques SPU de 16 B = 28 muestras) y de los contenedores VAGp (mono, cabecera de 0x30 B, big-endian) y VPK (estéreo,
// cabecera de 0x800 B little-endian «\x20KPV»: tamaño, offset, bloque, frecuencia, canales; los datos alternan trozos de bloque/canales bytes por canal).
// Aritmética idéntica a libavcodec adpcm_psx (verificada bit a bit con ffmpeg, tools/audio_check.py): muestra = ((nibble con signo << 12) >> shift) + (h1*f0 + h2*f1) / 64 (división C, trunca hacia 0), historial SIN recortar,
// salida recortada a 16 bits, los bloques con bandera >= 7 dan silencio. Coeficientes del filtro estándar de la SPU. Entrada no fiable: todos los tamaños se validan.
#pragma once
#include <cstdint>
#include <cstring>
#include <vector>

namespace vag {
struct State { int h1 = 0, h2 = 0; };
inline void block(const uint8_t* b, State& st, int16_t* out) {
    static const int F[5][2] = {{0, 0}, {60, 0}, {115, -52}, {98, -55}, {122, -60}};
    const int shift = b[0] & 15, pred = b[0] >> 4, flag = b[1] & 7;
    for (int i = 0; i < 28; i++) {
        int smp = 0;
        if (flag < 7 && pred < 5) {
            int n = (b[2 + i / 2] >> (4 * (i & 1))) & 15; if (n & 8) n -= 16;
            smp = ((n * 4096) >> shift) + (st.h1 * F[pred][0] + st.h2 * F[pred][1]) / 64;
        }
        out[i] = (int16_t)(smp > 32767 ? 32767 : smp < -32768 ? -32768 : smp);
        st.h2 = st.h1; st.h1 = smp;
    }
}
struct Track { uint32_t rate = 0; int channels = 0; std::vector<int16_t> pcm; size_t frames() const { return channels ? pcm.size() / (size_t)channels : 0; } };   // pcm intercalado (L R L R...)

inline uint32_t be32(const uint8_t* p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }
inline uint32_t le32(const uint8_t* p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

// VAGp mono: «VAGp», versión (BE), ..., tamaño (BE +0x0C), frecuencia (BE +0x10), nombre en +0x20; ADPCM desde +0x30.
inline bool decodeVag(const std::vector<uint8_t>& f, Track& t) {
    if (f.size() < 0x40 || std::memcmp(f.data(), "VAGp", 4) != 0) return false;
    t.rate = be32(&f[0x10]); t.channels = 1; if (t.rate == 0 || t.rate > 192000) return false;
    State st; t.pcm.clear(); t.pcm.reserve((f.size() - 0x30) / 16 * 28);
    for (size_t o = 0x30; o + 16 <= f.size(); o += 16) { int16_t s[28]; block(&f[o], st, s); t.pcm.insert(t.pcm.end(), s, s + 28); }
    return true;
}
// VPK: cabecera 0x800; datos = trozos de bloque/canales bytes por canal, alternando canales; el historial continúa entre trozos del mismo canal.
inline bool decodeVpk(const std::vector<uint8_t>& f, Track& t) {
    if (f.size() < 0x800 || std::memcmp(f.data(), " KPV", 4) != 0) return false;
    const uint32_t offset = le32(&f[8]), blockSize = le32(&f[12]), rate = le32(&f[16]), ch = le32(&f[20]);
    if (ch < 1 || ch > 2) return false; const uint32_t inter = blockSize / ch;   // blockSize = bloque de TODOS los canales (0xA000 = 2 x 0x5000)
    if (offset < 0x18 || offset > f.size() || inter < 16 || inter % 16 || rate == 0 || rate > 192000) return false;
    t.rate = rate; t.channels = (int)ch; std::vector<std::vector<int16_t>> chan(ch); std::vector<State> st(ch);
    size_t o = offset; size_t c = 0;
    while (o + 16 <= f.size()) {   // el último trozo puede ser parcial
        const size_t end = std::min<size_t>(f.size(), o + inter);
        for (; o + 16 <= end; o += 16) { int16_t s[28]; block(&f[o], st[c], s); chan[c].insert(chan[c].end(), s, s + 28); }
        o = end; c = (c + 1) % ch;
    }
    size_t n = chan[0].size(); for (auto& v : chan) n = std::min(n, v.size());
    t.pcm.resize(n * ch); for (size_t i = 0; i < n; i++) for (size_t k = 0; k < ch; k++) t.pcm[i * ch + k] = chan[k][i];
    return true;
}
}  // namespace vag
