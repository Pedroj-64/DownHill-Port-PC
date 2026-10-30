// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Efectos de sonido: mezclador por software (estéreo S16 44100 Hz sobre un flujo de SDL3) y biblioteca de clips sacados de los bancos SND/*.BNK (src/bnk.hpp). Un clip = las muestras de los
// granos GRAIN_TONE del sonido, sumadas con el volumen de cada tono (y su panorama si no es centrado). DH_SFX_FLAT=1 vuelve al comportamiento antiguo (todas las capas iguales y centradas);
// DH_SFX_PITCH=1 aplica además la afinación por tono. HIPÓTESIS: los efectos suenan a 44100 Hz (el banco no guarda la frecuencia base). Sin hilos: `update()` desde el bucle principal.
#pragma once
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "bnk.hpp"

namespace sfx {
// Panorama de un tono (GRADOS 0..360, 0 = centro) -> ganancias L/R normalizadas (centro = 1,1), equivalente a snd_MakeVolumes con gPanTable1 de 989snd (cos/sin en pasos de 0,5°).
// Panorama negativo = indirección por registro (-1 = registro 0, -5 = aleatorio): se deja centrado.
inline std::pair<float, float> panGains(int16_t pan) {
    if (pan < 0) return {1.f, 1.f};
    int idx; bool swap = false; const int p = pan % 360;
    if (p >= 270) idx = p - 270; else { const int q = p + 90; if (q < 180) idx = q; else { idx = q - 180; swap = true; } }
    const float a = (float)idx * 0.5f * 3.14159265358979f / 180.f;
    const float l = 1.41421356f * std::cos(a), r = 1.41421356f * std::sin(a);
    return swap ? std::make_pair(r, l) : std::make_pair(l, r);
}
// Afinación de un grano TONE según 989snd: PS1Note2Pitch(nota central, afinado central, nota base 60, 0). Nota central NEGATIVA = muestra nativa del PS2 (sin el factor 44100/48000).
// HIPÓTESIS: la nota base de los efectos es 60 (snd_GetSoundOriginalPitch devuelve 0x1E00 para los bloques SFX). Solo se usa con DH_SFX_PITCH.
inline float tonePitchRatio(const bnk::Tone& t) {
    const int cn = t.centerNote; const bool ps1 = cn >= 0; const int note = ps1 ? cn : -cn;
    double p = std::pow(2.0, ((60 - note) + ((double)t.centerFine) / 128.0) / 12.0);
    if (ps1) p *= 44100.0 / 48000.0;
    return (float)p;
}
// Remuestreo lineal de una muestra a la relación `ratio` (>1 = más agudo y más corta).
inline std::vector<int16_t> resample(const std::vector<int16_t>& p, float ratio) {
    if (p.empty() || !(ratio > 0.f)) return p;
    const size_t n = (size_t)std::max(1.0, std::floor((double)p.size() / (double)ratio)); std::vector<int16_t> o(n);
    for (size_t i = 0; i < n; i++) { const double sp = (double)i * (double)ratio; const size_t i0 = (size_t)sp; const double fr = sp - (double)i0;
        const double s0 = p[i0], s1 = i0 + 1 < p.size() ? p[i0 + 1] : p[i0]; o[i] = (int16_t)std::clamp((int)std::lround(s0 + (s1 - s0) * fr), -32768, 32767); }
    return o;
}

struct Clip { std::vector<int16_t> pcm; std::vector<int16_t> pcmR; bool loop = false; float bendSemis = 0.f;   // semitonos del pitch bend a fondo (+32767) del tono más fuerte: tono = 2^(bend/32767*bendSemis/12)
     bool stereo() const { return !pcmR.empty(); } };
using ClipPtr = std::shared_ptr<const Clip>;

class Library {
public:
    using Reader = std::function<bool(const std::string&, std::vector<uint8_t>&)>;
    // flat: ignora volumen/panorama por tono (comportamiento antiguo). pitch: aplica la afinación por tono (hipótesis).
    explicit Library(Reader rd, bool flat = false, bool pitch = false) : rd_(std::move(rd)), flat_(flat), pitch_(pitch) {}
    // Clip del sonido `name` del banco `bank` (p. ej. "PCKP_S", "P_CHECKPOINT"); null si no existe. Se guarda en caché.
    ClipPtr get(const std::string& bank, const std::string& name) {
        const std::string key = bank + "/" + name; if (auto it = clips_.find(key); it != clips_.end()) return it->second;
        ClipPtr out; if (const bnk::Bank* b = bank_(bank)) { const int si = b->find(name); if (si >= 0) out = build_(*b, (size_t)si); }
        clips_[key] = out; return out;
    }
    bool hasBank(const std::string& bank) { return bank_(bank) != nullptr; }
    bool has(const std::string& bank, const std::string& name) { const bnk::Bank* b = bank_(bank); return b && b->find(name) >= 0; }
private:
    Reader rd_; bool flat_ = false, pitch_ = false; std::map<std::string, std::unique_ptr<bnk::Bank>> banks_; std::map<std::string, ClipPtr> clips_;
    const bnk::Bank* bank_(const std::string& name) {
        auto it = banks_.find(name); if (it != banks_.end()) return it->second.get();
        std::vector<uint8_t> raw; auto b = std::make_unique<bnk::Bank>(); if (!rd_("SND/" + name + ".BNK", raw) || !b->parse(std::move(raw))) b.reset();
        return (banks_[name] = std::move(b)).get();
    }
    // Clip del sonido: suma de las muestras de sus granos GRAIN_TONE, con el volumen por tono (normalizado al mayor) y el panorama por tono.
    ClipPtr build_(const bnk::Bank& b, size_t si) const {
        auto c = std::make_shared<Clip>(); struct Layer { std::vector<int16_t> p; float w, gl, gr; }; std::vector<Layer> layers; std::set<std::pair<int, int>> done; float bendW = -1.f;
        for (const bnk::Tone& t : b.sounds[si].tones) {
            if (!t.isTone() || t.sample < 0) continue;   // los granos de control no llevan muestra
            // Los bancos repiten la MISMA muestra con panoramas opuestos (90/270, 45/315): se cuentan como capas distintas
            // (si no, un par L/R quedaría convertido en un tono duro a un lado). Con DH_SFX_FLAT se vuelve al criterio antiguo (por muestra).
            const int pkey = (!flat_ && t.pan > 0) ? t.pan : 0;
            if (!done.insert({t.sample, pkey}).second) continue;
            std::vector<int16_t> p; if (!b.decode(t.sample, p) || p.empty()) continue;
            if (b.samples[(size_t)t.sample].loop()) c->loop = true;
            float w = 1.f, gl = 1.f, gr = 1.f;
            if (!flat_) { w = t.vol >= 0 ? (float)t.vol : 127.f; const auto lr = panGains(t.pan); gl = lr.first; gr = lr.second; }
            if (pitch_) p = resample(p, tonePitchRatio(t) * (48000.f / 44100.f));   // tonePitchRatio es relativo a 48000 Hz (SPU2 0x1000); el mezclador sale a 44100
            if (t.pbHigh != 0 && (float)w >= bendW) { bendW = w; c->bendSemis = (float)t.pbHigh; }
            layers.push_back({std::move(p), w, gl, gr});
        }
        if (layers.empty()) return nullptr;
        size_t n = 0; float wmax = 0.f; for (const Layer& l : layers) { n = std::max(n, l.p.size()); wmax = std::max(wmax, l.w); }
        if (wmax <= 0.f) { wmax = 1.f; for (Layer& l : layers) l.w = 1.f; }
        const float k = (layers.size() > 1 ? 1.f / std::sqrt((float)layers.size()) : 1.f) / wmax;
        std::vector<int32_t> aL(n, 0), aR(n, 0); bool st = false;
        for (const Layer& l : layers) { if (l.gl != l.gr) st = true; for (size_t i = 0; i < l.p.size(); i++) { aL[i] += (int32_t)std::lround(l.p[i] * l.w * l.gl); aR[i] += (int32_t)std::lround(l.p[i] * l.w * l.gr); } }
        c->pcm.resize(n); for (size_t i = 0; i < n; i++) c->pcm[i] = (int16_t)std::clamp((int)(aL[i] * k), -32768, 32767);
        if (st && aL == aR) st = false;   // los pares L/R del banco se cancelan: el clip queda mono
        if (st) { c->pcmR.resize(n); for (size_t i = 0; i < n; i++) c->pcmR[i] = (int16_t)std::clamp((int)(aR[i] * k), -32768, 32767); }
        return c;
    }
};

class Mixer {
public:
    Mixer() = default; Mixer(const Mixer&) = delete; Mixer& operator=(const Mixer&) = delete;
    ~Mixer() { close(); }
    // Cierra el dispositivo (destruye el flujo). Hay que llamarlo ANTES de SDL_Quit(): destruir un flujo tras SDL_Quit es comportamiento indefinido.
    void close() { if (stream_) { SDL_DestroyAudioStream(stream_); stream_ = nullptr; } }
    // Abre el dispositivo de audio. El mezclador funciona igual sin dispositivo (render() es puro): así se prueba sin tarjeta.
    bool open(float masterVolume) {
        if (!SDL_WasInit(SDL_INIT_AUDIO) && !SDL_InitSubSystem(SDL_INIT_AUDIO)) return false;
        SDL_AudioSpec spec{}; spec.format = SDL_AUDIO_S16; spec.channels = 2; spec.freq = kRate; stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
        if (!stream_) return false; master_ = masterVolume; SDL_ResumeAudioStreamDevice(stream_); return true;
    }
    bool isOpen() const { return stream_ != nullptr; }
    void setMasterVolume(float v) { master_ = v; }
    // Una vez: clip, volumen (0..1+), tono relativo (1 = original).
    void play(const ClipPtr& c, float gain = 1.f, float pitch = 1.f) { if (!c || c->pcm.empty()) return; if (voices_.size() > 32) voices_.erase(voices_.begin()); voices_.push_back({c, 0.0, pitch, gain, false, -1}); }
    // Bucle identificado por `tag` (rodadura, viento...): crea, ajusta o detiene (gain <= 0) según el estado.
    void loop(int tag, const ClipPtr& c, float gain, float pitch) {
        for (auto it = voices_.begin(); it != voices_.end(); ++it) if (it->tag == tag) { if (gain <= 0.001f || !c) { voices_.erase(it); return; } it->gain = gain; it->pitch = pitch; return; }
        if (c && !c->pcm.empty() && gain > 0.001f) voices_.push_back({c, 0.0, pitch, gain, true, tag});
    }
    // Mezcla `frames` fotogramas estéreo (S16 entrelazado) SIN tocar el dispositivo: avanza las voces y retira las terminadas. Es el oráculo del mezclador.
    void render(std::vector<int16_t>& out, int frames) {
        std::vector<int32_t> acc((size_t)frames * 2, 0);
        for (auto& v : voices_) {
            const auto& p = v.clip->pcm; const bool st = v.clip->stereo(); const auto& pr = st ? v.clip->pcmR : v.clip->pcm;
            const double n = (double)p.size(); const bool lp = v.loop || v.clip->loop;
            for (int i = 0; i < frames; i++) {
                if (v.pos >= n) { if (lp) { v.pos = std::fmod(v.pos, n); } else break; }
                const size_t i0 = (size_t)v.pos; const double fr = v.pos - (double)i0; const size_t i1 = i0 + 1 < p.size() ? i0 + 1 : (lp ? 0 : i0);
                const double sl = (p[i0] + (p[i1] - p[i0]) * fr) * v.gain * master_, sr = (pr[i0] + (pr[i1] - pr[i0]) * fr) * v.gain * master_;
                acc[(size_t)i * 2] += (int32_t)sl; acc[(size_t)i * 2 + 1] += (int32_t)sr; v.pos += v.pitch;
            }
        }
        voices_.erase(std::remove_if(voices_.begin(), voices_.end(), [](const Voice& v) { return !v.loop && !v.clip->loop && v.pos >= (double)v.clip->pcm.size(); }), voices_.end());
        out.resize(acc.size()); for (size_t i = 0; i < acc.size(); i++) out[i] = (int16_t)std::clamp(acc[i], -32768, 32767);
    }
    void update() {
        if (!stream_) return; const int target = (int)(kRate * 4 * 0.15);   // ~0.15 s en cola
        while (SDL_GetAudioStreamQueued(stream_) < target) { render(chunk_, kRate / 20); if (!SDL_PutAudioStreamData(stream_, chunk_.data(), (int)(chunk_.size() * 2))) return; }
    }
    size_t activeVoices() const { return voices_.size(); }
    static constexpr int kRate = 44100;
private:
    struct Voice { ClipPtr clip; double pos; float pitch, gain; bool loop; int tag; };
    SDL_AudioStream* stream_ = nullptr; float master_ = 0.7f; std::vector<Voice> voices_; std::vector<int16_t> chunk_;
};

// Banco de ambiente/bici de un nivel (decompilación FUN_001CAD90: nivel -> banco; aquí por prefijo del nombre; los niveles sin banco propio usan ALPS_S).
inline std::string levelBank(std::string n) {
    for (char& c : n) c = (char)std::toupper((unsigned char)c);
    auto has = [&](const char* s) { return n.find(s) != std::string::npos; };
    if (n.rfind("CITYMX", 0) == 0) return "CITYMX_S"; if (n.rfind("CITYTD", 0) == 0) return "CITYTD_S"; if (n.rfind("CITY", 0) == 0) return "CITY_S";
    if (n.rfind("GLAC", 0) == 0) return (has("GLACIERT") || has("GLACT") ) ? "GLACTD_S" : "GLAC_S";
    if (n.rfind("AUB", 0) == 0) return "AUBE_S"; if (n.rfind("BC", 0) == 0) return "BC_S"; if (n.rfind("FUJ", 0) == 0) return "FUJI_S"; if (n.rfind("JUN", 0) == 0) return "JUNGLE_S";
    if (n.rfind("MOA", 0) == 0) return "MOAB_S"; if (n.rfind("PER", 0) == 0) return "PERU_S"; if (n.rfind("MOSH", 0) == 0) return "MOSH_S"; return "ALPS_S";
}
}  // namespace sfx
