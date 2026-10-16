// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Reproductor de música (SDL3): decodifica una pista VAG/VPK del juego (src/vag.hpp) y la envía a un flujo de audio. Sin hilos propios: `update()` rellena el flujo desde el bucle principal. Si no hay dispositivo de audio
// (SDL_AUDIODRIVER=dummy o sin tarjeta) el juego sigue sin sonido. La música sale de la imagen del jugador en ejecución; nunca se guarda ni se versiona (regla 4).
#pragma once
#include <SDL3/SDL.h>
#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>
#include "vag.hpp"

namespace audio {
class Music {
public:
    Music() = default; Music(const Music&) = delete; Music& operator=(const Music&) = delete;
    ~Music() { stop(); }
    // Carga y empieza a sonar `file` (bytes de un .VPK o .VAG). volume 0..1. false si el formato no es válido o no hay dispositivo.
    bool play(const std::vector<uint8_t>& file, float volume, bool loop = true) {
        stop(); vag::Track t;
        if (!(vag::decodeVpk(file, t) || vag::decodeVag(file, t))) return false;
        if (!SDL_WasInit(SDL_INIT_AUDIO) && !SDL_InitSubSystem(SDL_INIT_AUDIO)) return false;
        SDL_AudioSpec spec{}; spec.format = SDL_AUDIO_S16; spec.channels = t.channels; spec.freq = (int)t.rate;
        stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
        if (!stream_) return false;
        track_ = std::move(t); pos_ = 0; loop_ = loop; SDL_SetAudioStreamGain(stream_, volume); SDL_ResumeAudioStreamDevice(stream_); return true;
    }
    // Mantiene ~1 s de audio en cola; al llegar al final vuelve al principio si `loop`.
    void update() {
        if (!stream_ || track_.pcm.empty()) return;
        const size_t chunk = (size_t)track_.rate / 4 * (size_t)track_.channels;   // 0.25 s
        while (SDL_GetAudioStreamQueued(stream_) < (int)(track_.rate * track_.channels * 2)) {
            if (pos_ >= track_.pcm.size()) { if (!loop_) return; pos_ = 0; }
            const size_t n = std::min(chunk, track_.pcm.size() - pos_);
            if (!SDL_PutAudioStreamData(stream_, &track_.pcm[pos_], (int)(n * 2))) return;
            pos_ += n;
        }
    }
    void setVolume(float v) { if (stream_) SDL_SetAudioStreamGain(stream_, v); }
    void stop() { if (stream_) { SDL_DestroyAudioStream(stream_); stream_ = nullptr; } track_ = vag::Track(); }
    bool playing() const { return stream_ != nullptr; }
    double seconds() const { return track_.rate ? (double)track_.frames() / track_.rate : 0.0; }
private:
    SDL_AudioStream* stream_ = nullptr; vag::Track track_; size_t pos_ = 0; bool loop_ = true;
};
}  // namespace audio
