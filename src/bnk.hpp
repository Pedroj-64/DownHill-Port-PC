// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Bancos de sonido SND/*.BNK (docs/formats/audio.md). Estructura descifrada analizando los 50 bancos del disco y CONFIRMADA contra la decompilación de 989snd
// (github.com/Ziemas/989snd, la biblioteca del IOP que los parsea; el ejecutable no los toca):
//   cabecera 6 x u32 {3, 2, 0x18, fin_SBlk, off_datos = fin_SBlk + 0x18, tam_datos}; SBlk en 0x18: «SBlk», 1, 0x104, id, ...; palabras (relativas al SBlk) +0x1c = tabla de sonidos, +0x20 = tabla de granos, +0x34 = inicio de los nombres.
//   Tabla de sonidos: n x 12 B «SFX2» {i8 volumen, i8 grupo, i16 panorama, u8 nº de granos, u8 límite de instancias, u16 banderas, u32 desplazamiento a los granos}; n = (off_granos - off_sonidos) / 12.
//   Granos: registros de 0x28 B «SFXGrain» {u32 tipo, i32 espera, unión de parámetros de 0x20 B}. El tipo es la lista GRAIN_* de 989snd; SOLO 1 (GRAIN_TONE) y 9 (GRAIN_TONE2) llevan
//     muestra y su parámetro es el struct «Tone» (siguiente); el resto son control (bucles 0x15-0x17, parada 0x18, aleatorios 0x19-0x1b, registros 0x1e-0x22, marcadores 0x23-0x25,
//     LFO 4, hijos 5-6, salto 8, ...) y su +0x18 NO es una muestra (en los 50 bancos vale 0).
//   Parámetros de GRAIN_TONE («Tone» de 989snd, empieza en el grano +0x08): i8 prioridad(+0), i8 volumen(+1), i8 nota central(+2), i8 afinado central(+3),
//     i16 panorama(+4, GRADOS 0..360 con 0 = centro), i8 mapa bajo(+6)/alto(+7), i8 doblado bajo(+8)/alto(+9), u16 ADSR1(+0x0a), u16 ADSR2(+0x0c), i16 banderas(+0x0e),
//     u32 desplazamiento de la muestra(+0x10, o sea el grano +0x18). Con volumen/panorama >= 0 son valores literales (volumen 0..127); negativos = registro (-1 = registro 0, -5 = aleatorio).
//   Nombres: ranura de 20 B {nombre[16] relleno de ceros (puede incluir un espacio final), u32 índice de sonido}; las ranuras ocupan una rejilla de paso 20 con huecos vacíos; el índice es una permutación de 0..n-1.
//   Datos: ADPCM de la SPU (src/vag.hpp); una muestra termina en el bloque con la bandera de fin (bit 0); un bloque de bandera 7 aislado es un marcador. Banderas bit 1 = bucle.
// NO probado: la FRECUENCIA base de cada muestra (44100 Hz es HIPÓTESIS) y la afinación por tono (989snd usa PS1Note2Pitch(nota, afinado, 60, 0), con nota central negativa = muestra nativa
//   del PS2 y sin la conversión 44100/48000; en el puerto solo se aplica con DH_SFX_PITCH). Entrada no fiable: todo se valida.
#pragma once
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#include "vag.hpp"

namespace bnk {
// Grano de 0x28 B: tipo (+0, GRAIN_* de 989snd), espera (+4) y, si es GRAIN_TONE/TONE2, los parámetros «Tone» en +8.
struct Tone {
    uint32_t w[10] = {};                    // registro en bruto
    uint32_t type = 0;                      // w[0]: 1 = GRAIN_TONE, 9 = GRAIN_TONE2; el resto son granos de control (sin muestra)
    int32_t delay = 0;                      // w[1]: espera (unidades del secuenciador)
    int8_t priority = 0, vol = 0;           // w[2] bytes 0 y 1 (vol < 0 = registro/aleatorio)
    int8_t centerNote = 0, centerFine = 0;  // w[2] bytes 2 y 3 (afinado; ver la cabecera)
    int16_t pan = 0;                        // w[3] mitad baja: grados 0..360 (0 = centro; < 0 = registro)
    uint16_t adsr1 = 0, adsr2 = 0;          // w[4] mitad alta, w[5] mitad baja (registros ADSR de la SPU)
    int16_t tflags = 0;                     // w[5] mitad alta (bit 3 = voz de ruido)
    uint32_t sampleOffset = 0; int sample = -1;
    bool isTone() const { return type == 1 || type == 9; }   // solo estos granos referencian una muestra
};
// «SFX2» de 12 B: volumen/grupo/panorama del sonido + nº de granos, límite de instancias, banderas y desplazamiento a los granos.
struct Sound {
    std::string name;
    uint32_t flags = 0, prio = 0;           // crudo: prio = {i8 vol, i8 grupo, i16 pan}; flags = {u8 nº granos, u8 límite, u16 banderas}
    uint8_t vol = 0, volGroup = 0, numGrains = 0, instanceLimit = 0; int16_t pan = 0; uint16_t sflags = 0;
    std::vector<Tone> tones;
};
struct Sample { size_t startBlock = 0, endBlock = 0; int endFlag = 0; bool loop() const { return (endFlag & 2) != 0; } };

class Bank {
public:
    std::vector<Sound> sounds; std::vector<Sample> samples;
    bool parse(std::vector<uint8_t> file) {
        file_ = std::move(file); sounds.clear(); samples.clear(); names_.clear();
        if (file_.size() < 0x30) return false; uint32_t h[6]; std::memcpy(h, file_.data(), 24);
        if (h[0] != 3 || h[2] != 0x18 || h[3] < 0x40 || h[4] != h[3] + 0x18 || (uint64_t)h[4] + h[5] != file_.size() || std::memcmp(&file_[0x18], "SBlk", 4) != 0 || h[5] % 16) return false;
        dataOff_ = h[4]; dataSize_ = h[5]; const size_t sbLen = h[4] - 0x18; const uint8_t* sb = &file_[0x18];
        auto u = [&](size_t o) -> uint32_t { uint32_t v = 0; if (o + 4 <= sbLen) std::memcpy(&v, sb + o, 4); return v; };
        const uint32_t offSnd = u(0x1c), offTone = u(0x20), nameStart = u(0x34);
        if (offSnd < 0x38 || offTone <= offSnd || (offTone - offSnd) % 12 || nameStart <= offTone || nameStart > sbLen) return false;
        const size_t n = (offTone - offSnd) / 12; if (n == 0 || n > 4096) return false;
        // muestras: cortes por la bandera de fin
        const size_t nb = dataSize_ / 16; size_t st = 0;
        for (size_t i = 0; i < nb; i++) { const int fl = file_[dataOff_ + i * 16 + 1] & 7; if (fl & 1) { if (fl == 7 && i == st) { st = i + 1; continue; } samples.push_back({st, i, fl}); st = i + 1; } }
        std::map<uint32_t, int> byOffset; for (size_t k = 0; k < samples.size(); k++) byOffset[(uint32_t)(samples[k].startBlock * 16)] = (int)k;
        sounds.resize(n);
        for (size_t i = 0; i < n; i++) {
            const uint32_t flags = u(offSnd + 12 * i + 4), off = u(offSnd + 12 * i + 8), prio = u(offSnd + 12 * i); const uint32_t nt = flags & 0xff;
            Sound& s = sounds[i]; s.flags = flags; s.prio = prio;
            s.vol = (uint8_t)(prio & 0xff); s.volGroup = (uint8_t)((prio >> 8) & 0xff); s.pan = (int16_t)(prio >> 16);
            s.numGrains = (uint8_t)(flags & 0xff); s.instanceLimit = (uint8_t)((flags >> 8) & 0xff); s.sflags = (uint16_t)(flags >> 16);
            if (nt == 0) continue;   // alias (sin granos propios)
            if ((uint64_t)offTone + off + (uint64_t)nt * 0x28 > nameStart) return false;
            for (uint32_t k = 0; k < nt; k++) {
                Tone t; const size_t o = (size_t)offTone + off + (size_t)k * 0x28; for (int j = 0; j < 10; j++) t.w[j] = u(o + 4 * j);
                t.type = t.w[0]; t.delay = (int32_t)t.w[1];
                t.priority = (int8_t)(t.w[2] & 0xff); t.vol = (int8_t)((t.w[2] >> 8) & 0xff);
                t.centerNote = (int8_t)((t.w[2] >> 16) & 0xff); t.centerFine = (int8_t)((t.w[2] >> 24) & 0xff);
                t.pan = (int16_t)(t.w[3] & 0xffff); t.adsr1 = (uint16_t)(t.w[4] >> 16); t.adsr2 = (uint16_t)(t.w[5] & 0xffff); t.tflags = (int16_t)(t.w[5] >> 16);
                t.sampleOffset = t.w[6];   // solo es el desplazamiento de una muestra si el grano es GRAIN_TONE/TONE2
                if (t.isTone()) { auto it = byOffset.find(t.sampleOffset); if (it == byOffset.end()) return false; t.sample = it->second; }
                s.tones.push_back(t);
            }
        }
        // nombres: rejilla de ranuras de 20 B
        auto isName = [&](unsigned char c) { return std::isalnum(c) || c == '_' || c == '-' || c == ' ' || c == '.'; };
        auto slot = [&](size_t p, std::string& name, uint32_t& idx) -> bool {
            if (p + 20 > sbLen || sb[p] == 0) return false; size_t e = 0; while (e < 16 && sb[p + e]) e++;
            for (size_t k = 0; k < e; k++) if (!isName(sb[p + k])) return false; for (size_t k = e; k < 16; k++) if (sb[p + k]) return false;
            name.assign((const char*)sb + p, e); idx = u(p + 16); return true;
        };
        auto empty = [&](size_t p) { if (p + 20 > sbLen) return false; for (size_t k = 0; k < 20; k++) if (sb[p + k]) return false; return true; };
        long first = -1; std::string nm; uint32_t ix, ix2; std::string nm2;
        for (size_t p = nameStart; p + 20 <= sbLen; p++) { if (slot(p, nm, ix) && nm.size() >= 3 && ix < n && (slot(p + 20, nm2, ix2) || empty(p + 20)) && sb[p - 1] == 0) { first = (long)p; break; } }
        if (first < 0) return false;
        while (first - 20 >= (long)nameStart && slot((size_t)first - 20, nm, ix)) first -= 20;
        std::vector<char> seen(n, 0); size_t count = 0;
        for (size_t p = (size_t)first; p + 20 <= sbLen; p += 20) { if (!slot(p, nm, ix)) continue; if (ix >= n || seen[ix]) return false; seen[ix] = 1; sounds[ix].name = nm; names_[nm] = (int)ix; count++; }
        return count == n;
    }
    int find(const std::string& name) const { auto it = names_.find(name); return it == names_.end() ? -1 : it->second; }
    // PCM mono (44100 Hz salvo que se decida otra frecuencia) de una muestra; el historial del ADPCM empieza a cero en cada muestra.
    bool decode(int sample, std::vector<int16_t>& pcm) const {
        if (sample < 0 || (size_t)sample >= samples.size()) return false; const Sample& s = samples[(size_t)sample]; vag::State st; pcm.clear();
        for (size_t b = s.startBlock; b <= s.endBlock; b++) { const uint8_t* blk = &file_[dataOff_ + b * 16]; if ((blk[1] & 7) == 7) continue; int16_t o[28]; vag::block(blk, st, o); pcm.insert(pcm.end(), o, o + 28); }
        return true;
    }
private:
    std::vector<uint8_t> file_; size_t dataOff_ = 0, dataSize_ = 0; std::map<std::string, int> names_;
};
}  // namespace bnk
