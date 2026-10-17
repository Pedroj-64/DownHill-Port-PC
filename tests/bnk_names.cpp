// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Herramienta de análisis (docs/formats/audio.md): por cada banco SND/*.BNK de la ISO, lista índice de sonido,
// el u32 +0 (prioridad propia del material) y el nombre. Sirve para ver los bloques de material de 13 sonidos
// (R_BF_<MAT> + SUSP, BUMP, LAND_SM, LAND_BG, CRASH, PEDAL, COAST, SKID_L, SKID, BRAKE, TERRAIN, AI_*_RIDE) y
// comprobar que el u32 +0 es igual en todos los bancos para el mismo material aunque la base del bloque cambie.
// Uso: build/bnk_names imagen.iso [subcadena_del_banco]   (p. ej. ALPS_S)
#include "../src/iso9660.hpp"
#include "../src/bnk.hpp"
#include <cctype>
#include <cstdio>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    if (argc < 2) { std::fprintf(stderr, "uso: bnk_names imagen.iso [subcadena]\n"); return 2; }
    std::string filt = argc > 2 ? argv[2] : ""; for (char& c : filt) c = (char)std::toupper((unsigned char)c);
    iso9660::Image img; if (!img.open(argv[1])) { std::fprintf(stderr, "no se puede abrir %s\n", argv[1]); return 2; }
    std::vector<iso9660::Entry> es; if (!img.list(es)) return 1;
    for (const auto& e : es) {
        std::string k = e.path; for (char& c : k) c = (char)std::toupper((unsigned char)c);
        if (e.dir || k.rfind("SND/", 0) != 0 || k.size() < 8 || k.substr(k.size() - 4) != ".BNK") continue;
        if (!filt.empty() && k.find(filt) == std::string::npos) continue;
        std::vector<uint8_t> raw; if (!img.read(e, raw)) continue;
        bnk::Bank b; if (!b.parse(std::move(raw))) { std::printf("%s: no parsea\n", e.path.c_str()); continue; }
        std::printf("== %s (%zu sonidos) ==\n", e.path.c_str(), b.sounds.size());
        for (size_t i = 0; i < b.sounds.size(); i++) std::printf("  %3zu %08x %-20s tonos=%zu banderas=%08x\n", i, b.sounds[i].prio, b.sounds[i].name.c_str(), b.sounds[i].tones.size(), b.sounds[i].flags);
    }
    return 0;
}
