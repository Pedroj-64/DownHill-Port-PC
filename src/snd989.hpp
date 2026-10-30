// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Motor de «programas de sonido» de 989snd (la biblioteca de Sony que usa el juego) para los bancos SND/*.BNK (src/bnk.hpp): ejecuta los granos de control
// (bucles, registros, aleatorios, LFO, sonidos hijos, pitch bend) y mezcla los granos de tono con ADSR estilo SPU2. Sin hilos ni reloj: el VM avanza a 240 Hz
// según las muestras renderizadas (44100 Hz), así que la salida es determinista y no depende del tamaño de los trozos de render().
// ATRIBUCIÓN: reimplementación limpia hecha a partir de la LECTURA de Downhill Revived (github.com/skuza-oss/downhill-revived, GPL-3.0-or-later; port/hle/sound_grains.h,
// sound.c, sound_tables.h y docs/findings/022-audio.md) y de la semántica de 989snd (github.com/Ziemas/989snd). NO está validada al oído ni contra el hardware.
// Diferencias conocidas respecto a Revived: sin reverberación, sin rangos de voces por grupo (48 voces comunes), sin ducking ni paneo suave (la bandera 4 de setParams
// se trata como panorama inmediato), sin ADSR «directo» (los tonos de los bancos llevan registros SPU), PRNG xorshift propio (no R250) y envolvente a 48000 Hz
// emulada con un acumulador sobre la salida de 44100 Hz. Los tipos de grano o parámetros no soportados se cuentan en `unsupported[tipo]` ([45] = tipo desconocido o bucle sin espera).
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "bnk.hpp"

namespace snd989 {
namespace detail {
// Tablas matemáticas (Revived las lista generadas fuera de línea): nota = floor(32768·2^(n/12)) (12) + floor(32768·2^(n/1536)) (128); panorama = floor(16383·sin(π·n/360)); seno LFO = round(32767·sin(π·n/1024)).
struct Tables {
    int note[140]; int pan[181]; int16_t lfoSine[2048];
    Tables() {
        for (int i = 0; i < 12; i++) note[i] = (int)std::floor(32768.0 * std::pow(2.0, i / 12.0) + 1e-6);
        for (int i = 0; i < 128; i++) note[12 + i] = (int)std::floor(32768.0 * std::pow(2.0, i / 1536.0) + 1e-6);
        for (int i = 0; i <= 180; i++) pan[i] = (int)std::floor(16383.0 * std::sin(3.14159265358979323846 * i / 360.0) + 1e-6);
        for (int i = 0; i < 2048; i++) lfoSine[i] = (int16_t)std::lround(32767.0 * std::sin(3.14159265358979323846 * i / 1024.0));
    }
};
inline const Tables& tables() { static const Tables t; return t; }
inline int bound(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
inline uint32_t r32(const uint8_t* p) { uint32_t v; std::memcpy(&v, p, 4); return v; }
inline uint16_t r16(const uint8_t* p) { uint16_t v; std::memcpy(&v, p, 2); return v; }

// Pitch SPU (0x1000 = muestra a su frecuencia nativa de 48000 Hz) de un tono; unidades de pmod/afinado = 1/128 semitono (989snd tone_pitch).
inline int tonePitch(const bnk::Tone& t, int pmod, int bend) {
    const Tables& tb = tables(); const int center = t.centerNote;
    int total = 60 * 128 + pmod + t.centerFine;
    total += (int)((int64_t)(bend >= 0 ? t.pbHigh : t.pbLow) * bend * 128 / (bend >= 0 ? 32767 : 32768));
    total -= (center < 0 ? -center : center) * 128;
    int semi = total / 128, fine = total % 128; if (fine < 0) { fine += 128; semi--; }
    int octave = semi / 12, note = semi % 12; if (note < 0) { note += 12; octave--; }
    uint32_t pitch = ((uint32_t)tb.note[note] * (uint32_t)tb.note[fine + 12]) >> 16;
    const int shift = octave - 2;
    if (shift >= 0) pitch = shift > 1 ? 0x3FFF : pitch << shift;
    else if (shift < -31) pitch = 0;
    else pitch = (pitch + (1u << (-shift - 1))) >> -shift;
    if (center >= 0) pitch = pitch * 44100 / 48000;   // nota central positiva = muestra PS1 (44100 Hz)
    return (int)(pitch > 0x3FFF ? 0x3FFF : pitch);
}
// Amplitud 0..32766 + panorama en grados (0 = centro) -> ganancia izquierda/derecha.
inline void panVolume(int amp, int pan, int32_t out[2]) {
    const Tables& tb = tables(); int angle = pan % 360; if (angle < 0) angle += 360;
    int at = (angle + 90) % 360;
    if (at < 180) { out[0] = amp * tb.pan[180 - at] / 16383; out[1] = amp * tb.pan[at] / 16383; }
    else { at -= 180; out[1] = amp * tb.pan[180 - at] / 16383; out[0] = amp * tb.pan[at] / 16383; }
}
// Ganancia de grupo: cuadrática para los grupos 0..14 (los demás pasan tal cual). Los volúmenes de grupo del puerto son 1024 (sin atenuar).
inline int32_t groupGain(int32_t v, int group) {
    if (group < 0 || group >= 15) return v;
    if (v >= 32767) v = 32766;
    return v * v / 32766;
}
// Volumen de aplicación: 1024 = pleno; negativo = absoluto 0..127.
inline int applicationVolume(int old, int v) {
    if (v == INT32_MAX) return old;
    if (v < 0) { const int64_t a = -(int64_t)v; v = a >= 127 ? 1024 : (int)(a * 1024 / 127); }
    return v > 1024 ? 1024 : v;
}
// Envolvente ADSR de la SPU2 (registros adsr1/adsr2 de 16 bits), un paso por tick de 48000 Hz.
struct Env {
    uint16_t a1 = 0, a2 = 0; int sustain = 0, phase = 4, level = 0, ratePhase = 0, change = 0; uint32_t counter = 0, inc = 0; unsigned flags = 0;
    void prepare(int ph) {
        int shift, step, exp, dec = ph != 0;
        if (ph == 0) { shift = (a1 >> 10) & 31; step = 7 - ((a1 >> 8) & 3); exp = (a1 >> 15) & 1; }
        else if (ph == 1) { shift = (a1 >> 4) & 15; step = -8; exp = 1; }
        else if (ph == 2) { shift = (a2 >> 8) & 31; step = 7 - ((a2 >> 6) & 3); dec = (a2 >> 14) & 1; exp = (a2 >> 15) & 1; if (dec) step = ~step; }
        else { shift = a2 & 31; step = -8; exp = (a2 >> 5) & 1; }
        inc = 0x8000u >> (shift > 11 ? shift - 11 : 0); change = step * (1 << (shift < 11 ? 11 - shift : 0));
        flags = (unsigned)exp | ((unsigned)dec << 1); ratePhase = ph;
    }
    void start(uint16_t r1, uint16_t r2) { a1 = r1; a2 = r2; sustain = (int)((r1 & 15) + 1) * 2048; level = 0; counter = 0; phase = 0; prepare(0); }
    void release() { if (phase < 3) { phase = 3; counter = 0; } }
    void kill() { phase = 4; level = 0; }
    int tick() {
        if (phase >= 4) return level;
        if (ratePhase != phase) prepare(phase);
        uint32_t i = inc; if (flags == 1 && level > 0x6000) i >>= 2; if (!i) i = 1;
        counter += i;
        if (counter >= 0x8000) {
            int ch = change; if (flags == 3) ch = (int)(((int64_t)ch * level) >> 15);
            counter = 0; level = bound(level + ch, 0, 32767);
        }
        if (phase == 0 && level == 32767) phase = 1;
        else if (phase == 1 && level <= sustain) phase = 2;
        else if (phase >= 2 && !level) phase = 4;
        return level;
    }
};
}  // namespace detail

class Engine {
public:
    static constexpr int kRate = 44100, kVoices = 48, kHandlers = 128, kMaxDepth = 8;
    uint32_t unsupported[46] = {};   // por tipo de grano; [45] = tipo desconocido o bucle sin espera cortado por el presupuesto
    int8_t globalRegs[32] = {};      // registros globales (grano 30-33/40/44 con índice negativo); visibles para las pruebas

    Engine() { rng_ = 0x2545F491u; }
    void setBank(const std::string& name, std::shared_ptr<const bnk::Bank> bank) {
        if (!bank) { banks_.erase(name); return; }
        auto s = std::make_shared<Slot>(); s->bank = std::move(bank); banks_[name] = std::move(s);
    }
    // Arranca el sonido `soundName` del banco `bank`. volume: 1024 = pleno (negativo = absoluto 0..127); pan: -1/-2 = el del sonido, si no grados. Devuelve 0 si no existe.
    uint32_t play(const std::string& bank, const std::string& soundName, int volume = 1024, int pan = -1, int pmod = 0, int pbend = 0) {
        auto it = banks_.find(bank); if (it == banks_.end()) return 0;
        int idx = it->second->bank->find(soundName); if (idx < 0) idx = it->second->bank->find(soundName + " ");
        if (idx < 0) return 0;
        Handler* h = start(it->second, idx, volume, pan, pmod, pbend, nullptr, -1, -1);
        return h ? h->handle : 0;
    }
    // flags: 1 volumen, 2 panorama inmediato (4 se trata igual), 8 pmod, 16 pitch bend (como el comando SetSoundParams 0x21 de 989snd).
    void setParams(uint32_t handle, unsigned flags, int volume, int pan, int pmod, int pbend) {
        Handler* h = find(handle); if (!h) return;
        if (flags & 1) h->volume = detail::applicationVolume(h->volume, volume);
        if ((flags & 6) && pan != -2) h->pan = pan == -1 ? h->sound->pan : pan;
        if (flags & 8) h->pmod = pmod;
        if (flags & 16) h->pbend = detail::bound(pbend, -32768, 32767);
        update(*h); updateChildren(*h, flags & 27);
    }
    void stop(uint32_t handle) { if (Handler* h = find(handle)) stopHandler(*h); }
    void stopAll() { for (auto& h : handlers_) if (h.used) stopHandler(h); for (auto& v : voices_) if (v.used) v.env.release(); }
    size_t activeVoices() const { size_t n = 0; for (const auto& v : voices_) n += v.used; return n; }
    size_t activeHandlers() const { size_t n = 0; for (const auto& h : handlers_) n += h.used; return n; }

    // Mezcla `frames` fotogramas estéreo S16 entrelazados (sobrescribe `out`).
    void render(int16_t* out, size_t frames) {
        for (size_t f = 0; f < frames; f++) {
            tickAcc_ += 240;
            if (tickAcc_ >= (uint32_t)kRate) { tickAcc_ -= kRate; tickVm(); }
            envAcc_ += 48000; int envTicks = 0; while (envAcc_ >= (uint32_t)kRate) { envAcc_ -= kRate; envTicks++; }
            int32_t mixL = 0, mixR = 0;
            for (auto& v : voices_) {
                if (!v.used || v.paused || v.env.phase == 4) continue;
                int level = v.env.level; for (int k = 0; k < envTicks; k++) level = v.env.tick();
                if (v.env.phase == 4) continue;
                const std::vector<int16_t>& p = *v.pcm; const size_t i0 = (size_t)(v.pos >> 16);
                const int fr = (int)(v.pos & 0xFFFF); const int s0 = p[i0], s1 = i0 + 1 < p.size() ? p[i0 + 1] : (v.loop ? p[0] : p[i0]);
                int32_t s = s0 + (int32_t)(((int64_t)(s1 - s0) * fr) >> 16);
                s = (s * level) >> 15;
                if (!(v.tone.tflags & 16)) { mixL += (s * v.volL) >> 15; mixR += (s * v.volR) >> 15; }   // bit 16 = solo reverberación (sin soporte): nada en seco
                v.pos += v.step;
                const uint64_t end = (uint64_t)p.size() << 16;
                if (v.pos >= end) { if (v.loop) v.pos %= end; else v.env.kill(); }
            }
            out[2 * f] = (int16_t)std::clamp(mixL, -32768, 32767); out[2 * f + 1] = (int16_t)std::clamp(mixR, -32768, 32767);
            samples_++;
        }
    }

private:
    struct Slot {
        std::shared_ptr<const bnk::Bank> bank;
        std::vector<std::shared_ptr<const std::vector<int16_t>>> pcm;   // muestras decodificadas bajo demanda
        std::map<std::pair<int, int>, int> choice;                      // memoria de los granos de elección (sonido, grano)
        std::shared_ptr<const std::vector<int16_t>> sample(int i) {
            if (pcm.size() < bank->samples.size()) pcm.resize(bank->samples.size());
            if (i < 0 || (size_t)i >= pcm.size()) return nullptr;
            if (!pcm[(size_t)i]) { auto v = std::make_shared<std::vector<int16_t>>(); if (!bank->decode(i, *v) || v->empty()) return nullptr; pcm[(size_t)i] = std::move(v); }
            return pcm[(size_t)i];
        }
    };
    struct Lfo { uint32_t phase = 0, step = 0, origStep = 0; int target = 0, extra = 0, shape = 0, depth = 0, origDepth = 0, flags = 0, duty = 0, hold = 0, half = 0, tick = 0; };
    struct Handler {
        bool used = false, paused = false, done = false;
        int pc = 0, delay = 0, depth = 0, volume = 0, pan = 0, pmod = 0, pbend = 0, origVolume = 0;
        int lfoVol = 0, lfoPan = 0, lfoPmod = 0, lfoBend = 0, playCount = 0, skipCount = 0, soundIndex = 0;
        Lfo lfo[4]; int8_t regs[4] = {};
        uint32_t handle = 0, parent = 0; uint64_t createdAt = 0;
        std::shared_ptr<Slot> slot; const bnk::Sound* sound = nullptr;
    };
    struct Voice {
        bool used = false, paused = false, loop = false; uint32_t owner = 0; int zeroTicks = 4, toneVol = 0, tonePan = 0, volL = 0, volR = 0;
        bnk::Tone tone; std::shared_ptr<const std::vector<int16_t>> pcm; uint64_t pos = 0, step = 0; detail::Env env;
    };
    std::map<std::string, std::shared_ptr<Slot>> banks_;
    std::array<Handler, kHandlers> handlers_; std::array<Voice, kVoices> voices_;
    uint32_t rng_ = 1, nextHandle_ = 1, tickAcc_ = 0, envAcc_ = 0; uint64_t samples_ = 0;

    uint32_t random(uint32_t n) { rng_ ^= rng_ << 13; rng_ ^= rng_ >> 17; rng_ ^= rng_ << 5; return n ? (rng_ >> 4) % n : 0; }   // xorshift32 sembrado fijo
    Handler* find(uint32_t handle) { if (!handle) return nullptr; for (auto& h : handlers_) if (h.used && h.handle == handle) return &h; return nullptr; }
    int8_t* reg(Handler& h, int index) {
        if (index >= 0 && index < 4) return &h.regs[index];
        if (index < 0 && index >= -32) return &globalRegs[-index - 1];
        return nullptr;
    }
    // Valor de un parámetro de tono/hijo: >= 0 literal; -5 aleatorio; negativos = registro (0..3 locales, 5..36 globales).
    int grainValue(Handler& h, int value, bool pan) {
        if (value >= 0) return value;
        if (value == -5) return (int)random(pan ? 360 : 128);
        const int index = -value - 1;
        if (index < 4) value = h.regs[index]; else if (index >= 5 && index < 37) value = globalRegs[index - 5]; else value = 0;
        return pan ? 360 * detail::bound(value, 0, 127) / 127 : detail::bound(value, 0, 127);
    }
    void voiceUpdate(const Handler& h, Voice& v) {
        const int sv = detail::bound(h.volume * h.origVolume / 1024 + h.lfoVol, 0, 127);
        const int amp = 32766 * sv / 127 * v.toneVol / 127;
        int32_t g[2]; detail::panVolume(amp, h.pan + h.lfoPan + v.tonePan, g);
        const int grp = h.sound->volGroup;
        v.volL = detail::bound((detail::groupGain(g[0], grp) >> 1) * 2, 0, 32767); v.volR = detail::bound((detail::groupGain(g[1], grp) >> 1) * 2, 0, 32767);
        const uint64_t spu = (uint64_t)detail::tonePitch(v.tone, h.pmod + h.lfoPmod, detail::bound(h.pbend + h.lfoBend, -32768, 32767)) << 4;   // 1<<16 = 1 muestra por tick de 48 kHz
        v.step = spu * 48000 / kRate;
    }
    void update(Handler& h) { for (auto& v : voices_) if (v.used && v.owner == h.handle) voiceUpdate(h, v); }
    void updateChildren(Handler& h, unsigned flags) {
        for (auto& c : handlers_) if (c.used && c.parent == h.handle) {
            if (flags & 1) c.volume = detail::applicationVolume(c.volume, h.volume * h.origVolume / 127);
            if (flags & 2) c.pan = h.pan;
            if (flags & 8) c.pmod = h.pmod;
            if (flags & 16) c.pbend = h.pbend;
            update(c); updateChildren(c, flags);
        }
    }
    void modulate(Handler& h, int which) {
        bool changed = false;
        for (int i = which < 0 ? 0 : which; i < (which < 0 ? 4 : which + 1); i++) {
            Lfo& l = h.lfo[i];
            if (!l.target || !(++l.tick & 1)) continue;
            const int phase = (int)(l.phase >> 16); int value = 0;
            l.phase = (l.phase + l.step * 2) & 0x7FFFFFF;
            switch (l.shape) {
            case 1: value = detail::tables().lfoSine[phase]; break;
            case 2: value = phase < l.duty ? 32767 : -32767; break;
            case 3: value = phase < 512 ? 32767 * phase / 512 : phase >= 1536 ? 32767 * (phase - 1536) / 512 - 32767 : 32767 - 65534 * (phase - 512) / 1024; break;
            case 4: value = phase < 1024 ? 32767 * phase / 1023 : 32767 * (phase - 1024) / 1024 - 32767; break;
            case 5:
                if ((phase < 1024) != (l.half != 0)) {
                    l.half = phase < 1024;
                    l.hold = !l.half ? 2 * ((int)random(32768) - 16383) : -(int)random(32768) * (int)random(2);
                }
                value = l.hold; break;
            }
            if (l.flags & 1) value = -value;
            if (l.target >= 5) {   // un LFO modula la velocidad (5) o la profundidad (6) de otro
                Lfo& o = h.lfo[l.extra];
                if (l.target == 5) { const uint32_t range = ((o.origStep >> 8) * (uint32_t)l.depth) >> 10; o.step = o.origStep + (uint32_t)(((int64_t)range * value >> 15) * 256); }
                else { const int range = o.origDepth * l.depth >> 10; o.depth = (uint16_t)(o.origDepth + ((int64_t)range * value >> 15)); }
                continue;
            }
            int* target = l.target == 1 ? &h.lfoVol : l.target == 2 ? &h.lfoPan : l.target == 3 ? &h.lfoPmod : &h.lfoBend;
            int range = l.target == 1 ? h.origVolume : l.target == 2 ? 180 : l.target == 3 ? 6096 : 32767;
            range = range * l.depth >> 10;
            const int result = l.target == 1 ? (range * (value - 32767) >> 16) : (int)((int64_t)range * value >> 15);
            if (*target != result) { *target = result; changed = true; }
        }
        if (changed) update(h);
    }
    bool hasVoices(const Handler& h) const { for (const auto& v : voices_) if (v.used && v.owner == h.handle) return true; return false; }
    void keyOff(Handler& h, bool kill) { for (auto& v : voices_) if (v.used && v.owner == h.handle) { if (kill) v.used = false; else v.env.release(); } }
    void stopHandler(Handler& h) {
        h.done = true; h.paused = false; keyOff(h, false);
        for (auto& c : handlers_) if (c.used && c.parent == h.handle) stopHandler(c);
    }
    Voice* allocVoice(int priority) {
        Voice* victim = nullptr;
        for (auto& v : voices_) { if (!v.used) return &v; if (!victim || v.tone.priority < victim->tone.priority) victim = &v; }
        return victim && victim->tone.priority <= priority ? victim : nullptr;   // robo de la voz de menor prioridad
    }
    static size_t slotIndex(uint32_t type) { return type < 45 ? type : 45; }

    Handler* start(const std::shared_ptr<Slot>& slot, int soundIndex, int volume, int pan, int pmod, int bend, Handler* parent, int origVolume, int origPan) {
        const bnk::Sound& snd = slot->bank->sounds[(size_t)soundIndex];
        if (snd.tones.empty() || (parent && parent->depth >= kMaxDepth)) return nullptr;
        Handler* h = nullptr; for (auto& c : handlers_) if (!c.used) { h = &c; break; }
        if (!h) return nullptr;
        *h = Handler(); h->used = true; h->slot = slot; h->sound = &snd; h->soundIndex = soundIndex;
        h->handle = nextHandle_++; if (!nextHandle_) nextHandle_ = 1;
        h->createdAt = samples_;
        h->origVolume = origVolume < 0 ? snd.vol : origVolume;
        h->volume = detail::applicationVolume(1024, volume);
        h->pan = pan == -1 || pan == -2 ? (origPan < 0 ? snd.pan : origPan) : pan;
        h->pmod = pmod; h->pbend = bend;
        if (parent) { h->parent = parent->handle; h->depth = parent->depth + 1; std::memcpy(h->regs, parent->regs, 4); }
        h->delay = snd.tones[0].delay;
        run(*h);
        return h;
    }
    void startTone(Handler& h, const bnk::Tone& tone) {
        if (tone.tflags & 8) { unsupported[slotIndex(tone.type)]++; return; }   // voz de ruido: sin soporte
        auto pcm = h.slot->sample(tone.sample);
        Voice* v = pcm ? allocVoice(tone.priority) : nullptr;
        if (!v) { unsupported[slotIndex(tone.type)]++; return; }
        *v = Voice(); v->used = true; v->owner = h.handle; v->tone = tone; v->pcm = pcm; v->loop = h.slot->bank->samples[(size_t)tone.sample].loop();
        // Los parámetros por registro/aleatorios se capturan al asignar la voz y se mantienen (como en el IOP).
        v->toneVol = detail::bound(grainValue(h, tone.vol, false), 0, 127); v->tonePan = grainValue(h, tone.pan, true);
        v->env.start(tone.adsr1, tone.adsr2);
        voiceUpdate(h, *v);
    }
    // Ejecuta el programa hasta que toque esperar. Presupuesto de 256 granos por llamada: un bucle sin espera no cuelga nada.
    void run(Handler& h) {
        using detail::r16; using detail::r32;
        const auto& tones = h.sound->tones; const int n = (int)tones.size(); int budget = 256;
        while (!h.done && h.delay <= 0 && budget--) {
            const bnk::Tone& t = tones[(size_t)h.pc];
            const uint8_t* p = reinterpret_cast<const uint8_t*>(t.w) + 8;   // parámetros del grano (en +8, pequeño endian como bnk.hpp)
            const int type = (int)t.type, pc = h.pc; int next = pc + 1, extra = 0;
            const int a = (int16_t)r16(p), b = (int16_t)r16(p + 2), c = (int16_t)r16(p + 4);
            int8_t* rg;
            switch (type) {
            case 0: case 20: case 21: case 35: break;
            case 4: {
                if (p[0] >= 4 || p[1] > 6 || p[3] > 5 || (p[1] >= 5 && p[2] >= 4)) { unsupported[4]++; break; }
                Lfo& l = h.lfo[p[0]]; l = Lfo();
                l.target = p[1]; l.extra = p[2]; l.shape = p[3]; l.duty = r16(p + 4); l.depth = l.origDepth = r16(p + 6); l.flags = r16(p + 8);
                l.phase = ((l.flags & 2) ? random(2048) : (uint32_t)(r16(p + 10) & 2047)) << 16;
                l.step = l.origStep = r32(p + 12);
                if (l.shape == 5) { l.hold = -(int)random(32768) * (int)random(2); l.half = 1; }
                modulate(h, p[0]); break;
            }
            case 1: case 9: startTone(h, t); break;
            case 5: {   // sonido hijo del mismo banco; hereda registros y parámetros de aplicación
                const int id = (int)r32(p + 12), vol = grainValue(h, (int)r32(p), false); int child = -1;
                if (id >= 0 && (size_t)id < h.slot->bank->sounds.size()) child = id;
                else if (id < 0) {
                    char nm[17] = {}; std::memcpy(nm, p + 16, 16); child = h.slot->bank->find(nm);
                    if (child < 0) { std::string s(nm); while (!s.empty() && s.back() == ' ') s.pop_back(); child = h.slot->bank->find(s); }
                }
                if (child >= 0) start(h.slot, child, h.volume * h.origVolume / 127, h.pan, h.pmod, h.pbend, &h, vol, grainValue(h, (int)r32(p + 4), true));
                else unsupported[5]++;
                break;
            }
            case 6: for (auto& ch : handlers_) if (ch.used && ch.parent == h.handle && ch.soundIndex == (int)r32(p + 12)) stopHandler(ch); break;
            case 22: { int i = pc - 1; while (i >= 0 && tones[(size_t)i].type != 21) i--; if (i >= 0) next = i; else unsupported[22]++; break; }
            case 23: for (int i = pc + 1; i < n; i++) if (tones[(size_t)i].type == 22) { next = i + 1; break; } break;
            case 24: h.done = true; break;
            case 25: case 39: {   // elección: a ramas de b granos; 25 aleatoria sin repetir, 39 secuencial
                const auto key = std::make_pair(h.soundIndex, pc); auto it = h.slot->choice.find(key);
                int prev = it != h.slot->choice.end() ? it->second : c;
                if (a <= 0 || b <= 0 || a * b > n - next) { unsupported[type]++; h.done = true; break; }
                int pick = type == 25 ? (int)random((uint32_t)a) : prev;
                if (type == 25 && pick == prev) pick = (pick + 1) % a;
                pick = detail::bound(pick, 0, a - 1);
                h.slot->choice[key] = type == 25 ? pick : (pick + 1) % a;
                next += pick * b; h.playCount = b + 1; h.skipCount = (a - 1 - pick) * b;
                break;
            }
            case 26: extra = (int)random(r32(p) + 1u); break;
            case 27: { const int bend = (int)(65535u * random(32767) / 32767) - 32768; h.pbend = (int16_t)(a * bend / 100); update(h); break; }
            case 28: h.pbend = detail::bound(a * 32767 / 127, -32768, 32767); update(h); break;
            case 29: h.pbend = detail::bound(h.pbend + a * 32767 / 127, -32768, 32767); update(h); break;
            case 30: case 31: case 32: case 33:
                rg = reg(h, a);
                if (!rg) { unsupported[type]++; break; }
                if (type == 30) *rg = (int8_t)b;
                if (type == 31 && c >= b) *rg = (int8_t)(b + (int)random((uint32_t)(c - b + 1)));
                if (type == 32) *rg = (int8_t)detail::bound(*rg + 1, -128, 127);
                if (type == 33) *rg = (int8_t)detail::bound(*rg - 1, -128, 127);
                break;
            case 34:   // condicional: salta el grano siguiente si se cumple (b: 0 reg >= c, 1 reg != c, >=2 reg <= c)
                rg = reg(h, a);
                if (rg && ((b == 0 && *rg >= c) || (b == 1 && *rg != c) || (b >= 2 && *rg <= c))) next++;
                break;
            case 36: case 37: {
                const int mark = type == 37 && b >= a ? a + (int)random((uint32_t)(b - a + 1)) : a; int i = 0;
                for (; i < n; i++) if (tones[(size_t)i].type == 35 && (int16_t)r16(reinterpret_cast<const uint8_t*>(tones[(size_t)i].w) + 8) == mark) { next = i; break; }
                if (i == n) unsupported[type]++;
                break;
            }
            case 38: if (hasVoices(h)) { next = pc; extra = 1; } break;
            case 40: rg = reg(h, b); if (rg) *rg = (int8_t)detail::bound(*rg + a, -128, 127); break;
            case 41: keyOff(h, false); break;
            case 42: keyOff(h, true); break;
            case 43: next = n - 1; break;
            case 44: { int8_t* s = reg(h, a); int8_t* d = reg(h, b); if (s && d) *d = *s; break; }
            default: unsupported[slotIndex(t.type)]++; break;
            }
            if (h.playCount && --h.playCount == 0) next += h.skipCount;
            h.pc = next;
            if (next >= n || next < 0) { h.done = true; break; }
            h.delay = tones[(size_t)next].delay + extra;
        }
        if (!h.done && h.delay <= 0) { h.delay = 1; unsupported[45]++; }   // bucle de cero espera: ceder el tick
    }
    // Tick del VM a 240 Hz: libera voces cuya envolvente lleva 4 ticks en cero y avanza los programas.
    void tickVm() {
        for (auto& v : voices_) {
            if (!v.used || !v.owner || v.paused) continue;
            if (v.env.level) v.zeroTicks = 4; else if (!--v.zeroTicks) v.used = false;
        }
        for (auto& h : handlers_) {
            if (!h.used || h.paused || h.createdAt == samples_) continue;
            modulate(h, -1);
            if (!h.done && --h.delay <= 0) run(h);
            if (!h.done || hasVoices(h)) continue;
            bool kids = false; for (const auto& c : handlers_) if (c.used && c.parent == h.handle) { kids = true; break; }
            if (!kids) { h.used = false; h.slot.reset(); }
        }
    }
};
}  // namespace snd989
