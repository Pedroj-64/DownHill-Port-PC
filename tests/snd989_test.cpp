// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Motor de programas de sonido (src/snd989.hpp) con bancos SINTÉTICOS (sin datos del juego): tono simple, pitch bend, registros + condicional, bucle 21/22,
// hijos 5/6, elección 25, LFO de volumen, determinismo entre tamaños de trozo y bucle de cero espera.
#include "../src/snd989.hpp"
#include <cstdio>
#include <cstdlib>
#include <set>
#define CHECK(c) do { if (!(c)) { std::printf("FALLO %s:%d %s\n", __FILE__, __LINE__, #c); std::exit(1); } } while (0)
using Bytes = std::vector<uint8_t>;
using Grain = std::array<uint32_t, 10>;

// --- constructores de granos (w[0] tipo, w[1] espera, w[2..9] parámetros desde el grano +8) ---
static Grain tone(int sample, uint32_t delay = 0, int8_t note = -60, int8_t pbHigh = 0, uint16_t adsr1 = 0x000F, uint16_t adsr2 = 0x0000, int8_t vol = 127) {
    Grain g{}; g[0] = 1; g[1] = delay;
    g[2] = (uint32_t)(uint8_t)0 | ((uint32_t)(uint8_t)vol << 8) | ((uint32_t)(uint8_t)note << 16);   // prioridad, volumen, nota central, afinado 0
    g[3] = 0;                                                    // panorama 0 = centro
    g[4] = ((uint32_t)(uint8_t)pbHigh << 8) | ((uint32_t)adsr1 << 16); g[5] = adsr2;
    g[6] = (uint32_t)sample;                                     // se sustituye por el desplazamiento de la muestra al construir
    return g;
}
static Grain ctl(uint32_t type, uint32_t delay = 0, int a = 0, int b = 0, int c = 0) {
    Grain g{}; g[0] = type; g[1] = delay; g[2] = (uint16_t)a | ((uint32_t)(uint16_t)b << 16); g[3] = (uint16_t)c; return g;
}
static Grain child(int soundIndex, uint32_t delay = 0) { Grain g{}; g[0] = 5; g[1] = delay; g[2] = 127; g[3] = 0; g[5] = (uint32_t)soundIndex; return g; }
static Grain stopChild(int soundIndex, uint32_t delay = 0) { Grain g{}; g[0] = 6; g[1] = delay; g[5] = (uint32_t)soundIndex; return g; }
static Grain lfoVolume(uint32_t step, int depth) {   // LFO 0, objetivo 1 (volumen), forma 1 (seno)
    Grain g{}; g[0] = 4; g[2] = 0u | (1u << 8) | (0u << 16) | (1u << 24); g[3] = (uint32_t)depth << 16; g[4] = 0; g[5] = step; return g;
}
struct SoundDef { std::string name; std::vector<Grain> grains; };
struct SampleDef { int blocks; bool loop; };

// Banco: muestras ADPCM constantes (nibble 7, cabecera 0x00 -> 28672), última con bandera de fin (1) o fin+bucle (3).
static Bytes makeBank(const std::vector<SampleDef>& samples, std::vector<SoundDef> sounds) {
    const size_t n = sounds.size(); size_t ng = 0; for (auto& s : sounds) ng += s.grains.size();
    const uint32_t offSnd = 0x3c, offTone = offSnd + (uint32_t)(12 * n), nameStart = offTone + (uint32_t)(0x28 * ng) + 8;
    const uint32_t sbLen = nameStart + 20 * ((uint32_t)n + 2) + 0x40; size_t dataSize = 0; std::vector<uint32_t> sampleOff;
    for (auto& s : samples) { sampleOff.push_back((uint32_t)dataSize); dataSize += (size_t)s.blocks * 16; }
    const uint32_t dataOff = 0x18 + sbLen; Bytes f(dataOff + dataSize, 0);
    auto w32 = [&](size_t o, uint32_t v) { std::memcpy(&f[o], &v, 4); };
    w32(0, 3); w32(4, 2); w32(8, 0x18); w32(12, sbLen); w32(16, dataOff); w32(20, (uint32_t)dataSize);
    std::memcpy(&f[0x18], "SBlk", 4); w32(0x18 + 4, 1); w32(0x18 + 8, 0x104);
    w32(0x18 + 0x1c, offSnd); w32(0x18 + 0x20, offTone); w32(0x18 + 0x34, nameStart);
    uint32_t goff = 0;
    for (size_t i = 0; i < n; i++) {
        const size_t so = 0x18 + offSnd + 12 * i;
        w32(so, 100u | (1u << 8) | (0u << 16)); w32(so + 4, (uint32_t)sounds[i].grains.size()); w32(so + 8, goff);   // vol 100, grupo 1, pan 0
        for (size_t k = 0; k < sounds[i].grains.size(); k++) {
            Grain g = sounds[i].grains[k]; if (g[0] == 1 || g[0] == 9) g[6] = sampleOff.at(g[6]);
            for (int j = 0; j < 10; j++) w32(0x18 + offTone + goff + 4 * j, g[(size_t)j]);
            goff += 0x28;
        }
        const size_t no = 0x18 + nameStart + 20 * (i + 1); std::memcpy(&f[no], sounds[i].name.data(), sounds[i].name.size()); w32(no + 16, (uint32_t)i);
    }
    for (size_t k = 0; k < samples.size(); k++) {
        uint8_t* d = &f[dataOff + sampleOff[k]];
        for (int b = 0; b < samples[k].blocks; b++) { std::memset(d + 16 * b + 2, 0x77, 14); d[16 * b + 1] = 0; }
        d[16 * (samples[k].blocks - 1) + 1] = samples[k].loop ? 3 : 1;
    }
    return f;
}
static std::shared_ptr<const bnk::Bank> load(const std::vector<SampleDef>& s, std::vector<SoundDef> d) {
    auto b = std::make_shared<bnk::Bank>(); CHECK(b->parse(makeBank(s, std::move(d)))); return b;
}
static std::vector<int16_t> renderAll(snd989::Engine& e, size_t frames, size_t chunk) {
    std::vector<int16_t> out(frames * 2);
    for (size_t o = 0; o < frames; o += chunk) e.render(&out[o * 2], std::min(chunk, frames - o));
    return out;
}
static size_t lastNonZero(const std::vector<int16_t>& v) { size_t l = 0; for (size_t i = 0; i < v.size(); i += 2) if (v[i] || v[i + 1]) l = i / 2; return l; }
static double rms(const std::vector<int16_t>& v, size_t f0, size_t f1) { double s = 0; for (size_t i = f0; i < f1; i++) s += (double)v[2 * i] * v[2 * i]; return std::sqrt(s / (double)(f1 - f0)); }

int main() {
    // --- tono simple: suena y termina por fin de muestra; el motor libera voz y handler ---
    { auto b = load({{8, false}}, {{"SIMPLE", {tone(0)}}}); snd989::Engine e; e.setBank("T", b);
      CHECK(e.play("T", "SIMPLE") != 0 && e.play("T", "NOEXISTE") == 0 && e.play("X", "SIMPLE") == 0);
      CHECK(e.activeVoices() == 1);
      e.stopAll(); renderAll(e, 4410, 4410); CHECK(e.activeVoices() == 0);
      auto h = e.play("T", "SIMPLE"); CHECK(h != 0 && e.activeVoices() == 1);
      auto o = renderAll(e, 4410, 512);
      CHECK(rms(o, 0, 100) > 1000 && lastNonZero(o) > 100 && lastNonZero(o) < 400);   // 8 bloques = 224 muestras a 48/44,1
      CHECK(e.activeVoices() == 0 && e.activeHandlers() == 0); }
    // --- pitch bend: pbHigh = 12 y bend = 32767 -> una octava arriba (la muestra dura la mitad) ---
    { auto b = load({{256, false}}, {{"BEND", {tone(0, 0, -60, 12)}}}); snd989::Engine e1, e2; e1.setBank("T", b); e2.setBank("T", b);
      e1.play("T", "BEND"); e2.play("T", "BEND", 1024, -1, 0, 32767);
      const size_t l1 = lastNonZero(renderAll(e1, 20000, 1000)), l2 = lastNonZero(renderAll(e2, 20000, 1000));
      const double r = (double)l1 / (double)l2; CHECK(l1 > 6000 && r > 1.9 && r < 2.1); }
    // --- registro + condicional: reg0 = 5; si reg0 >= 3 salta el grano siguiente ---
    for (int v : {5, 1}) {
        auto b = load({{4, false}}, {{"COND", {ctl(30, 0, 0, v), ctl(34, 0, 0, 0, 3), ctl(30, 0, -1, 1), ctl(30, 0, -2, 2)}}});   // 30: reg a := b (índices negativos = globales)
        snd989::Engine e; e.setBank("T", b); e.play("T", "COND");
        CHECK(e.globalRegs[1] == 2 && e.globalRegs[0] == (v >= 3 ? 0 : 1)); }
    // --- bucle 21/22: repite hasta que el contador global llega a N ---
    { const int N = 7;
      auto b = load({{4, false}}, {{"LOOP", {ctl(30, 0, -1, 0), ctl(21), ctl(40, 0, 1, -1), ctl(34, 0, -1, 0, N), ctl(22), ctl(24)}}});
      snd989::Engine e; e.setBank("T", b); e.play("T", "LOOP"); CHECK(e.globalRegs[0] == N); CHECK(e.unsupported[45] == 0); }
    // --- hijo (5) y parada (6): el hijo es un bucle de muestra; stop lo libera ---
    { auto b = load({{16, true}}, {{"PADRE", {child(1), stopChild(1, 24)}}, {"HIJO", {tone(0)}}});
      snd989::Engine e; e.setBank("T", b); e.play("T", "PADRE");
      CHECK(e.activeVoices() == 1 && e.activeHandlers() == 2);
      renderAll(e, 2000, 256); CHECK(e.activeVoices() == 1);          // 2000 fotogramas = ~10 ticks < 24: sigue sonando
      auto o = renderAll(e, 8000, 256); CHECK(e.activeVoices() == 0 && e.activeHandlers() == 0); (void)o; }
    // --- elección (25): nunca repite la rama anterior ---
    { auto b = load({{4, false}}, {{"ELIGE", {ctl(25, 0, 3, 1, 0), ctl(30, 0, -1, 1), ctl(30, 0, -1, 2), ctl(30, 0, -1, 3)}}});
      snd989::Engine e; e.setBank("T", b); int prev = 0; std::set<int> seen;
      for (int i = 0; i < 60; i++) { e.globalRegs[0] = 0; e.play("T", "ELIGE"); const int r = e.globalRegs[0]; CHECK(r >= 1 && r <= 3 && (i == 0 || r != prev)); prev = r; seen.insert(r); }
      CHECK(seen.size() == 3); }
    // --- LFO de volumen: la amplitud oscila (ciclo de ~0,1 s) ---
    { auto b = load({{200, true}}, {{"LFO", {lfoVolume(5592405, 1024), tone(0)}}});
      snd989::Engine e; e.setBank("T", b); e.play("T", "LFO"); auto o = renderAll(e, 15000, 441);
      double lo = 1e9, hi = 0; for (size_t f = 4410; f + 441 <= 15000; f += 441) { const double r = rms(o, f, f + 441); lo = std::min(lo, r); hi = std::max(hi, r); }
      CHECK(hi > 2000 && hi > 2.0 * lo); }
    // --- determinismo: mismo audio con trozos de 64 y de 4096 fotogramas (aleatorios, espera aleatoria, LFO con fase aleatoria) ---
    { auto b = load({{40, true}, {6, false}}, {{"MIX", {ctl(25, 0, 2, 1, 0), ctl(30, 0, -1, 1), ctl(30, 0, -1, 2), tone(0, 0, -60, 12), ctl(26, 3, 20), tone(1, 5), ctl(43, 30)}}});
      std::vector<int16_t> a, c;
      { snd989::Engine e; e.setBank("T", b); e.play("T", "MIX"); e.play("T", "MIX", 700, 90, 100, 5000); a = renderAll(e, 30000, 64); }
      { snd989::Engine e; e.setBank("T", b); e.play("T", "MIX"); e.play("T", "MIX", 700, 90, 100, 5000); c = renderAll(e, 30000, 4096); }
      CHECK(a == c); CHECK(rms(a, 0, 3000) > 100); }
    // --- bucle de cero espera + tipo desconocido: no cuelga y se cuenta ---
    { auto b = load({{4, false}}, {{"CERO", {ctl(21), ctl(22)}}, {"RARO", {ctl(31337), tone(0)}}});
      snd989::Engine e; e.setBank("T", b); e.play("T", "CERO"); renderAll(e, 4096, 4096); CHECK(e.unsupported[45] > 0 && e.activeHandlers() == 1);
      e.stopAll(); renderAll(e, 4096, 4096); CHECK(e.activeHandlers() == 0);
      const uint32_t before = e.unsupported[45]; e.play("T", "RARO"); CHECK(e.unsupported[45] == before + 1); }
    std::puts("snd989_test OK");
    return 0;
}
