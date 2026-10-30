// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Efectos de sonido (src/sfx.hpp) con material SINTÉTICO (sin datos del juego, sin dispositivo de audio):
//   - sfx::Library: banco de src/bnk.hpp construido aquí; clip de un sonido = suma de sus granos GRAIN_TONE (1/√n de capas), con
//     volumen y panorama por tono (DH_SFX_FLAT los anula), panoramas, afinación (989snd) y remuestreo; bucle heredado de la muestra,
//     caché y ausencias (banco/sonido que no existe -> null).
//   - sfx::Mixer: render() mezcla PCM S16 estéreo SIN dispositivo (identidad, suma de voces, recorte, tono, clips estéreo, bucle por
//     etiqueta y retirada de las voces de una sola vez). Es el oráculo del mezclador frente a la salida SDL.
#include "../src/sfx.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#define CHECK(c) do { if (!(c)) { std::printf("FALLO %s:%d %s\n", __FILE__, __LINE__, #c); std::exit(1); } } while (0)
using Bytes = std::vector<uint8_t>;
static void w32(Bytes& b, size_t o, uint32_t v) { std::memcpy(&b[o], &v, 4); }
// Banco de 2 sonidos: SND_A = 2 tonos (muestras 0 y 1), SND_B = 1 tono (muestra 1, con bucle). Igual que tests/bnk_test.cpp.
static Bytes makeBank() {
    const uint32_t offSnd = 0x3c, offTone = 0x54, nameStart = 0xcc, sbLen = 0x1a0, dataSize = 5 * 16, dataOff = 0x18 + sbLen;
    Bytes f(dataOff + dataSize, 0); const uint32_t sbEnd = dataOff - 0x18;
    w32(f, 0, 3); w32(f, 4, 2); w32(f, 8, 0x18); w32(f, 12, sbEnd); w32(f, 16, dataOff); w32(f, 20, dataSize);
    std::memcpy(&f[0x18], "SBlk", 4); w32(f, 0x1c, 1); w32(f, 0x20, 0x104); std::memcpy(&f[0x24], "TEST", 4);
    Bytes sb(f.begin() + 0x18, f.begin() + 0x18 + sbLen); auto s32 = [&](size_t o, uint32_t v) { std::memcpy(&sb[o], &v, 4); };
    s32(0x1c, offSnd); s32(0x20, offTone); s32(0x34, nameStart);
    s32(offSnd + 0, 0x50); s32(offSnd + 4, 0x40002); s32(offSnd + 8, 0);
    s32(offSnd + 12, 0x46); s32(offSnd + 16, 0x80010001); s32(offSnd + 20, 0x50);
    s32(offTone + 0 * 0x28 + 0, 1); s32(offTone + 1 * 0x28 + 0, 1); s32(offTone + 2 * 0x28 + 0, 1);   // GRAIN_TONE
    s32(offTone + 0 * 0x28 + 24, 0); s32(offTone + 1 * 0x28 + 24, 48); s32(offTone + 2 * 0x28 + 24, 48);
    std::memcpy(&sb[nameStart], "TEST", 4); sb[nameStart + 8] = 0x98;
    for (int i = 0; i < 6; i++) sb[nameStart + 0x14 + 2 * i] = (uint8_t)(i + 1);
    const size_t p = nameStart + 0x40; std::memcpy(&sb[p], "SND_A", 5); s32(p + 16, 0); std::memcpy(&sb[p + 60], "SND_B ", 6); s32(p + 60 + 16, 1);
    std::memcpy(&f[0x18], sb.data(), sb.size());
    uint8_t* d = &f[dataOff]; d[0] = 0xC0; d[2] = 0x11; d[16] = 0xC0; d[32] = 0xC0; d[33] = 1; d[48] = 0xC0; d[49] = 2; d[64] = 0xC0; d[65] = 3;
    return f;
}
static sfx::ClipPtr clip(std::initializer_list<int16_t> v, bool loop = false) { auto c = std::make_shared<sfx::Clip>(); c->pcm.assign(v.begin(), v.end()); c->loop = loop; return c; }
int main() {
    const Bytes file = makeBank();
    // --- Library: clip del sonido = suma de las muestras distintas de sus tonos ---
    { bnk::Bank ref; CHECK(ref.parse(file));
      sfx::Library lib([&](const std::string& p, std::vector<uint8_t>& out) { if (p != "SND/TEST.BNK") return false; out = file; return true; });
      CHECK(lib.hasBank("TEST") && !lib.hasBank("OTRO"));
      CHECK(lib.has("TEST", "SND_A") && lib.has("TEST", "SND_B ") && !lib.has("TEST", "nada"));
      auto a = lib.get("TEST", "SND_A"), b = lib.get("TEST", "SND_B ");
      CHECK(a && b && lib.get("TEST", "SND_A") == a);              // caché: mismo puntero
      CHECK(lib.get("TEST", "nada") == nullptr && lib.get("OTRO", "SND_A") == nullptr);
      // SND_B = 1 tono (muestra 1, con bucle): clip idéntico a la muestra
      std::vector<int16_t> p1; CHECK(ref.decode(1, p1)); CHECK(b->pcm.size() == p1.size() && b->loop);
      for (size_t i = 0; i < p1.size(); i++) CHECK(b->pcm[i] == p1[i]);
      // SND_A = 2 tonos distintos (muestras 0 y 1): suma escalada 1/√2; hereda el bucle de la muestra 1
      std::vector<int16_t> p0; CHECK(ref.decode(0, p0)); CHECK(a->loop && a->pcm.size() == p0.size());
      for (size_t i = 0; i < p0.size(); i++) { int32_t acc = p0[i] + (i < p1.size() ? p1[i] : 0); int16_t e = (int16_t)std::clamp((int)(acc * (1.0 / std::sqrt(2.0))), -32768, 32767); CHECK(a->pcm[i] == e); } }
    // --- Mixer: identidad (ganancia 1, maestro 1, tono 1) ---
    { sfx::Mixer mx; mx.setMasterVolume(1.f); auto c = clip({1000, -2000, 3000, -4000}); std::vector<int16_t> out;
      mx.play(c, 1.f, 1.f); CHECK(mx.activeVoices() == 1);
      mx.render(out, 4); CHECK(out.size() == 8); for (int i = 0; i < 4; i++) { CHECK(out[2 * i] == c->pcm[i]); CHECK(out[2 * i + 1] == c->pcm[i]); }
      CHECK(mx.activeVoices() == 0); }                              // la voz de una sola vez se retira al terminar
    // --- Mixer: suma de dos voces y recorte a 16 bits ---
    { sfx::Mixer mx; mx.setMasterVolume(1.f); std::vector<int16_t> out;
      static const int16_t e1[4] = {1500, -1500, 3500, -3500};
      mx.play(clip({1000, -2000, 3000, -4000}), 1.f); mx.play(clip({500, 500, 500, 500}), 1.f); mx.render(out, 4);
      for (int i = 0; i < 4; i++) CHECK(out[2 * i] == e1[i]);
      mx.play(clip({30000, 30000}), 1.f); mx.play(clip({30000, 30000}), 1.f); mx.render(out, 2);
      for (int i = 0; i < 4; i++) CHECK(out[i] == 32767); }
    // --- Mixer: volumen maestro y tono ---
    { sfx::Mixer mx; mx.setMasterVolume(0.5f); auto c = clip({2000, 4000, 6000, 8000}); std::vector<int16_t> out;
      mx.play(c, 1.f, 1.f); mx.render(out, 4); for (int i = 0; i < 4; i++) CHECK(out[2 * i] == (int16_t)(c->pcm[i] * 0.5f)); }
    { sfx::Mixer mx; mx.setMasterVolume(1.f); auto c = clip({10, 20, 30, 40, 50, 60, 70, 80}); std::vector<int16_t> out;
      mx.play(c, 1.f, 2.f); mx.render(out, 4); for (int i = 0; i < 4; i++) CHECK(out[2 * i] == c->pcm[2 * i]); }
    // --- Mixer: bucle por etiqueta (se queda hasta que la etiqueta se apaga con ganancia 0) ---
    { sfx::Mixer mx; mx.setMasterVolume(1.f); auto c = clip({100, 200, 300, 400}); std::vector<int16_t> out;
      mx.loop(7, c, 1.f, 1.f); CHECK(mx.activeVoices() == 1);
      mx.render(out, 8); CHECK(mx.activeVoices() == 1); for (int i = 0; i < 4; i++) { CHECK(out[2 * i] == c->pcm[i]); CHECK(out[2 * (i + 4)] == c->pcm[i]); }
      mx.loop(7, c, 1.f, 2.f); CHECK(mx.activeVoices() == 1);       // ajustar no crea otra voz
      mx.loop(7, nullptr, 0.f, 1.f); CHECK(mx.activeVoices() == 0); }
    // --- panoramas por tono (grados 0..360, 0 = centro) -> ganancias L/R normalizadas ---
    { auto c = sfx::panGains(0); CHECK(std::fabs(c.first - 1.f) < 1e-4f && std::fabs(c.second - 1.f) < 1e-4f);
      auto r = sfx::panGains(90); CHECK(std::fabs(r.first) < 1e-4f && std::fabs(r.second - 1.4142136f) < 1e-4f);
      auto l = sfx::panGains(270); CHECK(std::fabs(l.first - 1.4142136f) < 1e-4f && std::fabs(l.second) < 1e-4f);
      auto b = sfx::panGains(180); CHECK(std::fabs(b.first - 1.f) < 1e-4f && std::fabs(b.second - 1.f) < 1e-4f);
      auto m = sfx::panGains(45); CHECK(m.second > m.first);                 // 45° tira a la derecha
      auto d = sfx::panGains(-1); CHECK(d.first == 1.f && d.second == 1.f);  // negativo = registro/aleatorio: centrado
    }
    // --- afinación por tono (989snd PS1Note2Pitch con nota base 60) ---
    { bnk::Tone t; t.centerNote = 60; t.centerFine = 0; CHECK(std::fabs(sfx::tonePitchRatio(t) - 44100.f / 48000.f) < 1e-5f);
      t.centerNote = -60; CHECK(std::fabs(sfx::tonePitchRatio(t) - 1.f) < 1e-5f);   // negativa = muestra nativa del PS2
      t.centerNote = -60; t.centerFine = 64; CHECK(std::fabs(sfx::tonePitchRatio(t) - std::pow(2.f, 0.5f / 12.f)) < 1e-5f);   // el afinado SUMA (989snd tone_pitch: total += center_fine), no resta
      t.centerFine = 0; t.centerNote = 72; CHECK(std::fabs(sfx::tonePitchRatio(t) - 0.5f * 44100.f / 48000.f) < 1e-4f); }
    // --- remuestreo lineal a la relación de afinación ---
    { std::vector<int16_t> p = {0, 100, 200, 300, 400, 500, 600, 700};
      auto r = sfx::resample(p, 2.f); CHECK(r.size() == 4 && r[0] == 0 && r[1] == 200 && r[2] == 400 && r[3] == 600);
      CHECK(sfx::resample(p, 1.f) == p && sfx::resample(p, 0.f) == p); }
    // --- Library: volumen por tono (peso relativo) y panorama (clip estéreo); DH_SFX_FLAT lo anula ---
    { Bytes f = makeBank(); const size_t T = 0x18 + 0x54;   // SBlk + offTone
      f[0x18 + 0x1a0 + 3 * 16 + 0] = 0x00; f[0x18 + 0x1a0 + 3 * 16 + 2] = 0x01;   // muestra 1: primer valor distinto de cero
      // (0xC0 = desplazamiento 0 y PREDICTOR 12, inválido -> el bloque sale en silencio; con 0x00 se decodifica el nibble)
      w32(f, T + 0 * 0x28 + 8, 0x00006400); w32(f, T + 0 * 0x28 + 12, 0);     // grano 0: volumen 100, centrado
      w32(f, T + 1 * 0x28 + 8, 0x00003200); w32(f, T + 1 * 0x28 + 12, 90);    // grano 1: volumen 50, 90° (derecha)
      bnk::Bank ref; CHECK(ref.parse(f));
      std::vector<int16_t> p0, p1; CHECK(ref.decode(0, p0) && ref.decode(1, p1));
      auto rd = [&](const std::string& p, std::vector<uint8_t>& out) { if (p != "SND/TEST.BNK") return false; out = f; return true; };
      sfx::Library lib(rd); auto a = lib.get("TEST", "SND_A"); CHECK(a && a->stereo() && a->pcm.size() == p0.size());
      const float k = (1.f / std::sqrt(2.f)) / 100.f;
      for (size_t i = 0; i < a->pcm.size(); i++) {
          const int32_t l = (int32_t)std::lround(p0[i] * 100.f * 1.f);
          const int32_t r1 = i < p1.size() ? (int32_t)std::lround(p1[i] * 50.f * 1.41421356f) : 0;
          CHECK(a->pcm[i] == (int16_t)std::clamp((int)(l * k), -32768, 32767));
          CHECK(a->pcmR[i] == (int16_t)std::clamp((int)((l + r1) * k), -32768, 32767));
      }
      sfx::Library flat(rd, true); auto af = flat.get("TEST", "SND_A"); CHECK(af && !af->stereo()); }
    // --- el mismo sample con panoramas opuestos (así vienen los pares L/R del banco) queda centrado y en una sola capa por lado ---
    { Bytes f = makeBank(); const size_t T = 0x18 + 0x54;
      f[0x18 + 0x1a0 + 0] = 0x00; f[0x18 + 0x1a0 + 2] = 0x01;                // muestra 0 con contenido
      w32(f, T + 0 * 0x28 + 24, 0); w32(f, T + 1 * 0x28 + 24, 0);            // los dos granos -> muestra 0
      w32(f, T + 0 * 0x28 + 8, 0x00006e00); w32(f, T + 0 * 0x28 + 12, 270);  // izquierda
      w32(f, T + 1 * 0x28 + 8, 0x00006e00); w32(f, T + 1 * 0x28 + 12, 90);   // derecha
      bnk::Bank ref; CHECK(ref.parse(f)); std::vector<int16_t> p0; CHECK(ref.decode(0, p0));
      auto rd = [&](const std::string& p, std::vector<uint8_t>& out) { if (p != "SND/TEST.BNK") return false; out = f; return true; };
      sfx::Library lib(rd); auto a = lib.get("TEST", "SND_A"); CHECK(a && !a->stereo() && a->pcm.size() == p0.size());
      const float k = (1.f / std::sqrt(2.f)) / 110.f;
      for (size_t i = 0; i < a->pcm.size(); i++) { const int32_t l = (int32_t)std::lround(p0[i] * 110.f * 1.41421356f); CHECK(a->pcm[i] == (int16_t)std::clamp((int)(l * k), -32768, 32767)); } }
    // --- Mixer: clip estéreo (canal derecho distinto) ---
    { sfx::Mixer mx; mx.setMasterVolume(1.f); std::vector<int16_t> out;
      auto c = std::make_shared<sfx::Clip>(); c->pcm = {1000, 2000, 3000, 4000}; c->pcmR = {10, 20, 30, 40};
      CHECK(c->stereo()); mx.play(c, 1.f, 1.f); mx.render(out, 4);
      for (int i = 0; i < 4; i++) { CHECK(out[2 * i] == c->pcm[i]); CHECK(out[2 * i + 1] == c->pcmR[i]); } }
    std::printf("sfx_test OK\n"); return 0;
}
