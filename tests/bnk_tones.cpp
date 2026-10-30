// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Herramienta de análisis (docs/formats/audio.md): volcado de los parámetros DECODIFICADOS de un banco SND/*.BNK.
// Por sonido: «SFX2» (volumen, grupo, panorama, nº de granos, límite de instancias, banderas) y, por grano, el tipo
// GRAIN_* de 989snd (1 = GRAIN_TONE, 9 = GRAIN_TONE2 llevan muestra; el resto son control) y, si es de tono, el
// struct «Tone»: prioridad, volumen, nota/afinado central, panorama (grados), ADSR de la SPU y banderas.
// Sirve para verificar que solo los granos de tono apuntan a muestras y para documentar volúmenes y panoramas reales.
// Uso: build/bnk_tones imagen.iso [BANCO] [subcadena_del_sonido]   (p. ej. ALPS_S B_DIRT_TERRAIN)
#include "../src/iso9660.hpp"
#include "../src/bnk.hpp"
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    if (argc < 2) { std::fprintf(stderr, "uso: bnk_tones imagen.iso [banco] [subcadena]\n"); return 2; }
    std::string bank = argc > 2 ? argv[2] : "", filt = argc > 3 ? argv[3] : "";
    for (char& c : bank) c = (char)std::toupper((unsigned char)c); for (char& c : filt) c = (char)std::toupper((unsigned char)c);
    iso9660::Image img; if (!img.open(argv[1])) { std::fprintf(stderr, "no se puede abrir %s\n", argv[1]); return 2; }
    std::vector<iso9660::Entry> es; if (!img.list(es)) return 1;
    for (const auto& e : es) {
        std::string k = e.path; for (char& c : k) c = (char)std::toupper((unsigned char)c);
        if (e.dir || k.rfind("SND/", 0) != 0 || k.size() < 8 || k.substr(k.size() - 4) != ".BNK") continue;
        if (!bank.empty() && k.find(bank) == std::string::npos) continue;
        std::vector<uint8_t> raw; if (!img.read(e, raw)) continue;
        bnk::Bank b; if (!b.parse(std::move(raw))) { std::printf("%s: no parsea\n", e.path.c_str()); continue; }
        std::printf("== %s (%zu sonidos) ==\n", e.path.c_str(), b.sounds.size());
        for (const bnk::Sound& s : b.sounds) {
            if (!filt.empty() && s.name.find(filt) == std::string::npos) continue;
            std::printf("  %-20s vol=%3u grupo=%u pan=%3d granos=%u(%zu) limite=%u banderas=%04x\n", s.name.c_str(), s.vol, s.volGroup, s.pan, s.numGrains, s.tones.size(), s.instanceLimit, s.sflags);
            for (size_t k = 0; k < s.tones.size(); k++) {
                const bnk::Tone& t = s.tones[k];
                if (!t.isTone()) { std::printf("    g%zu control tipo=%#04x espera=%d\n", k, t.type, t.delay); continue; }
                std::printf("    g%zu TONO vol=%3d prio=%3d nota=%4d afina=%4d pan=%4d pbL=%d pbH=%d adsr1=%04x adsr2=%04x band=%04x espera=%d muestra=%d\n",
                            k, t.vol, t.priority, t.centerNote, t.centerFine, t.pan, t.pbLow, t.pbHigh, t.adsr1, t.adsr2, (uint16_t)t.tflags, t.delay, t.sample);
            }
        }
    }
    return 0;
}
