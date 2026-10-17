// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Parser de bancos SND/*.BNK (src/bnk.hpp) con un banco SINTÉTICO construido aquí (sin material del juego): tablas de sonidos/granos «SFX2»/«SFXGrain», parámetros decodificados del grano («Tone»), nombres en rejilla de 20 B, muestras por bandera de fin, bucles y rechazo de entradas inválidas.
#include "../src/bnk.hpp"
#include <cstdio>
#include <cstdlib>
#define CHECK(c) do { if (!(c)) { std::printf("FALLO %s:%d %s\n", __FILE__, __LINE__, #c); std::exit(1); } } while (0)
using Bytes = std::vector<uint8_t>;
static void w32(Bytes& b, size_t o, uint32_t v) { std::memcpy(&b[o], &v, 4); }
static Bytes makeBank(uint32_t tone2SampleOffset = 48, uint32_t idxB = 1, uint32_t tone2Type = 1) {
    const uint32_t offSnd = 0x3c, offTone = 0x54, nameStart = 0xcc, sbLen = 0x1a0 /* incluye la sección final de 0x18 */, dataSize = 5 * 16, dataOff = 0x18 + sbLen;
    Bytes f(dataOff + dataSize, 0); const uint32_t sbEnd = dataOff - 0x18;
    w32(f, 0, 3); w32(f, 4, 2); w32(f, 8, 0x18); w32(f, 12, sbEnd); w32(f, 16, dataOff); w32(f, 20, dataSize);
    std::memcpy(&f[0x18], "SBlk", 4); w32(f, 0x1c, 1); w32(f, 0x20, 0x104); std::memcpy(&f[0x24], "TEST", 4);
    Bytes sb(f.begin() + 0x18, f.begin() + 0x18 + sbLen); auto s32 = [&](size_t o, uint32_t v) { std::memcpy(&sb[o], &v, 4); };
    s32(0x1c, offSnd); s32(0x20, offTone); s32(0x34, nameStart);
    // sonido 0: 2 tonos (banderas 0x40000 | 2), sonido 1: 1 tono
    s32(offSnd + 0, 0x50); s32(offSnd + 4, 0x40002); s32(offSnd + 8, 0);
    s32(offSnd + 12, 0x46); s32(offSnd + 16, 0x80010001); s32(offSnd + 20, 0x50);
    // granos: w[0] = tipo (1 = GRAIN_TONE; 0x15 = GRAIN_LOOP_START), w[6] = desplazamiento de la muestra; el grano 0 lleva parámetros «Tone»
    s32(offTone + 0 * 0x28 + 0, 1); s32(offTone + 1 * 0x28 + 0, 1); s32(offTone + 2 * 0x28 + 0, tone2Type);
    s32(offTone + 0 * 0x28 + 8, 0x000a1902);   // w[2]: prioridad 2, volumen 25, nota central 10, afinado 0
    s32(offTone + 0 * 0x28 + 12, 0x0000005a);  // w[3] mitad baja: panorama 90 grados
    s32(offTone + 0 * 0x28 + 16, 0x80ff0000); s32(offTone + 0 * 0x28 + 20, 0x00079fcc);   // ADSR1 / ADSR2 + banderas
    s32(offTone + 0 * 0x28 + 24, 0); s32(offTone + 1 * 0x28 + 24, 48); s32(offTone + 2 * 0x28 + 24, tone2SampleOffset);
    // nombres: registro del banco (no es una ranura válida: el byte 8 no es cero), tabla de cubos, y dos ranuras con huecos
    std::memcpy(&sb[nameStart], "TEST", 4); sb[nameStart + 8] = 0x98;
    for (int i = 0; i < 6; i++) sb[nameStart + 0x14 + 2 * i] = (uint8_t)(i + 1);
    const size_t p = nameStart + 0x40; std::memcpy(&sb[p], "SND_A", 5); s32(p + 16, 0); std::memcpy(&sb[p + 60], "SND_B ", 6); s32(p + 60 + 16, idxB);
    std::memcpy(&f[0x18], sb.data(), sb.size());
    // datos: muestra 0 = 3 bloques (fin en el 3.º), muestra 1 = 1 bloque aislado con fin+bucle (bandera 3) tras 1 de cuerpo (bandera 2)
    uint8_t* d = &f[dataOff]; d[0] = 0xC0; d[2] = 0x11; d[16] = 0xC0; d[32] = 0xC0; d[33] = 1; d[48] = 0xC0; d[49] = 2; d[64] = 0xC0; d[65] = 3;
    return f;
}
int main() {
    { bnk::Bank b; CHECK(b.parse(makeBank())); CHECK(b.sounds.size() == 2 && b.samples.size() == 2); CHECK(b.sounds[0].name == "SND_A" && b.sounds[1].name == "SND_B ");
      CHECK(b.find("SND_A") == 0 && b.find("SND_B ") == 1 && b.find("nada") == -1);
      CHECK(b.sounds[0].tones.size() == 2 && b.sounds[1].tones.size() == 1 && b.sounds[0].tones[0].sample == 0 && b.sounds[0].tones[1].sample == 1 && b.sounds[1].tones[0].sample == 1);
      // parámetros decodificados (SFX2 del sonido y struct Tone del grano)
      CHECK(b.sounds[0].vol == 0x50 && b.sounds[0].volGroup == 0 && b.sounds[0].pan == 0 && b.sounds[0].numGrains == 2 && b.sounds[0].instanceLimit == 0 && b.sounds[0].sflags == 4);
      const bnk::Tone& t0 = b.sounds[0].tones[0];
      CHECK(t0.type == 1 && t0.isTone() && t0.priority == 2 && t0.vol == 25 && t0.centerNote == 10 && t0.centerFine == 0 && t0.pan == 90);
      CHECK(t0.adsr1 == 0x80ff && t0.adsr2 == 0x9fcc && t0.tflags == 7 && t0.sampleOffset == 0 && b.sounds[0].tones[1].isTone());
      CHECK(!b.samples[0].loop() && b.samples[1].loop() && b.samples[0].endBlock == 2 && b.samples[1].startBlock == 3 && b.samples[1].endBlock == 4);
      std::vector<int16_t> pcm; CHECK(b.decode(0, pcm) && pcm.size() == 3 * 28 && b.decode(1, pcm) && pcm.size() == 2 * 28 && !b.decode(2, pcm) && !b.decode(-1, pcm)); }
    { Bytes f = makeBank(); f[0] = 4; bnk::Bank b; CHECK(!b.parse(f)); }                       // versión desconocida
    { Bytes f = makeBank(); f[0x18] = 'X'; bnk::Bank b; CHECK(!b.parse(f)); }                  // sin «SBlk»
    { Bytes f = makeBank(); f.pop_back(); bnk::Bank b; CHECK(!b.parse(f)); }                   // tamaño no cuadra
    { bnk::Bank b; CHECK(!b.parse(makeBank(/*tono que no cae en el inicio de una muestra*/ 20))); }
    { bnk::Bank b; CHECK(!b.parse(makeBank(48, /*índice de sonido repetido*/ 0))); }
    // grano de control (GRAIN_LOOP_START = 0x15): no exige un desplazamiento de muestra válido y queda sin muestra
    { bnk::Bank b; CHECK(b.parse(makeBank(48, 1, 0x15))); CHECK(b.sounds[1].tones.size() == 1 && !b.sounds[1].tones[0].isTone() && b.sounds[1].tones[0].type == 0x15 && b.sounds[1].tones[0].sample == -1 && b.sounds[1].tones[0].sampleOffset == 48); }
    // el desplazamiento inválido se sigue rechazando cuando el grano SÍ es de tono
    { bnk::Bank b; CHECK(!b.parse(makeBank(20, 1, 1))); }
    { bnk::Bank b; CHECK(!b.parse(Bytes(10, 0))); }
    std::printf("bnk_test OK\n"); return 0;
}
