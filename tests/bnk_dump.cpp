// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Herramienta de desarrollo (investigación del formato SND/*.BNK, docs/formats/audio.md): bnk_dump imagen.iso SND/PAUSE_S.BNK directorio_salida -> lista los nombres del banco y exporta cada muestra como WAV mono.
// Estructura (igual en los 5 bancos mirados): u32 {3, 2, off_SBlk = 0x18, fin_SBlk, off_datos = fin_SBlk + 0x18, tam_datos}; SBlk con los nombres y los parámetros de las «voces»; datos = ADPCM de SPU. Las muestras se cortan por las
// banderas de fin (bit 0) de los bloques (bandera 7 = marcador); la frecuencia real de cada voz está en tablas aún sin descifrar: los WAV salen a 44100 Hz (HIPÓTESIS). La salida son datos del juego: escríbela FUERA del repo (regla 4).
#include "../src/iso9660.hpp"
#include "../src/ie.hpp"
#include "../src/vag.hpp"
#include <cctype>
#include <cstdio>
#include <string>
static void wavWrite(const std::string& p, const std::vector<int16_t>& pcm, uint32_t rate) {
    FILE* f = std::fopen(p.c_str(), "wb"); if (!f) return; const uint32_t n = (uint32_t)pcm.size() * 2, br = rate * 2; const uint16_t one = 1, bits = 16, ba = 2; const uint32_t sixteen = 16, riff = 36 + n;
    std::fwrite("RIFF", 1, 4, f); std::fwrite(&riff, 4, 1, f); std::fwrite("WAVEfmt ", 1, 8, f); std::fwrite(&sixteen, 4, 1, f); std::fwrite(&one, 2, 1, f); std::fwrite(&one, 2, 1, f); std::fwrite(&rate, 4, 1, f); std::fwrite(&br, 4, 1, f); std::fwrite(&ba, 2, 1, f); std::fwrite(&bits, 2, 1, f);
    std::fwrite("data", 1, 4, f); std::fwrite(&n, 4, 1, f); std::fwrite(pcm.data(), 2, pcm.size(), f); std::fclose(f);
}
int main(int argc, char** argv) {
    iso9660::Image img; if (argc < 4 || !img.open(argv[1])) { std::fprintf(stderr, "uso: bnk_dump imagen.iso SND/X.BNK dir_salida\n"); return 2; }
    std::vector<iso9660::Entry> es; if (!img.list(es)) return 1; std::string want = argv[2]; for (char& c : want) c = (char)std::toupper((unsigned char)c); std::vector<uint8_t> d;
    for (const auto& e : es) { std::string k = e.path; for (char& c : k) c = (char)std::toupper((unsigned char)c); if (k == want && !e.dir) { if (!img.read(e, d)) return 1; break; } }
    if (d.size() < 0x30) { std::fprintf(stderr, "no está en la imagen\n"); return 1; }
    uint32_t h[6]; std::memcpy(h, d.data(), 24); if (h[0] != 3 || h[2] != 0x18 || h[4] != h[3] + 0x18 || (uint64_t)h[4] + h[5] != d.size() || std::memcmp(&d[0x18], "SBlk", 4) != 0) { std::fprintf(stderr, "no es un banco conocido\n"); return 1; }
    std::printf("SBlk %u B, datos @0x%x %u B (%u bloques)\nnombres:", h[3] - 0x18, h[4], h[5], h[5] / 16);
    std::string cur; for (size_t i = 0x18 + 0x100; i < h[3]; i++) { const unsigned char c = d[i]; if (std::isalnum(c) || c == '_' || c == '-' || c == '.') cur += (char)c; else { if (cur.size() >= 3) std::printf(" %s", cur.c_str()); cur.clear(); } } std::printf("\n");
    const uint8_t* data = &d[h[4]]; const size_t nb = h[5] / 16; size_t start = 0; int idx = 0; const std::string stem = std::string(argv[3]);
    for (size_t i = 0; i < nb; i++) {
        const int fl = data[i * 16 + 1] & 7;
        if (fl & 1) {
            if (fl == 7 && i == start) { start = i + 1; continue; }   // marcador suelto
            vag::State st; std::vector<int16_t> pcm; for (size_t b = start; b <= i; b++) { if ((data[b * 16 + 1] & 7) == 7) continue; int16_t s[28]; vag::block(&data[b * 16], st, s); pcm.insert(pcm.end(), s, s + 28); }
            char nm[64]; std::snprintf(nm, sizeof nm, "/muestra_%02d.wav", idx); wavWrite(stem + nm, pcm, 44100);
            std::printf("muestra %02d: bloques %zu..%zu (%zu) %.2f s a 44100 Hz, fin=0x%x%s\n", idx, start, i, i - start + 1, pcm.size() / 44100.0, fl, (fl & 2) ? " (bucle)" : ""); idx++; start = i + 1;
        }
    }
    if (start < nb) std::printf("(aviso: %zu bloques finales sin bandera de fin)\n", nb - start);
    return 0;
}
