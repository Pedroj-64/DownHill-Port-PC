// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Verificación con datos reales (no va a CI): bnk_check imagen.iso -> parsea con src/bnk.hpp los bancos de SND/ de la imagen y comprueba la estructura (nombres = permutación de los sonidos, todos los tonos en el inicio exacto de una muestra);
// imprime una línea por banco y sale con 1 si alguno falla.
#include "../src/bnk.hpp"
#include "../src/iso9660.hpp"
#include <cstdio>
#include <cctype>
int main(int argc, char** argv) {
    iso9660::Image img; if (argc < 2 || !img.open(argv[1])) { std::fprintf(stderr, "uso: bnk_check imagen.iso\n"); return 2; }
    std::vector<iso9660::Entry> es; if (!img.list(es)) return 1; int bad = 0, n = 0;
    for (const auto& e : es) {
        std::string k = e.path; for (char& c : k) c = (char)std::toupper((unsigned char)c); if (e.dir || k.rfind("SND/", 0) != 0 || k.size() < 8 || k.substr(k.size() - 4) != ".BNK") continue;
        std::vector<uint8_t> raw; if (!img.read(e, raw)) { bad++; continue; } bnk::Bank b; const bool ok = b.parse(std::move(raw)); n++; bad += !ok; size_t tones = 0; for (auto& s : b.sounds) tones += s.tones.size();
        std::printf("%s %s sonidos=%zu muestras=%zu tonos=%zu\n", ok ? "OK  " : "MAL ", e.path.c_str(), b.sounds.size(), b.samples.size(), tones);
    }
    std::printf("%d bancos, %d con errores\n", n, bad); return bad != 0;
}
