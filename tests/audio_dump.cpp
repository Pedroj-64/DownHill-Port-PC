// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Herramienta de desarrollo: audio_dump vag|vpk entrada salida.raw -> PCM s16le intercalado con src/vag.hpp (tools/audio_check.py lo compara con ffmpeg). La salida son datos del juego: escríbela FUERA del repo (regla 4).
#include "../src/vag.hpp"
#include <cstdio>
#include <string>
int main(int argc, char** argv) {
    if (argc < 4) { std::fprintf(stderr, "uso: audio_dump vag|vpk entrada salida.raw\n"); return 2; }
    FILE* f = std::fopen(argv[2], "rb"); if (!f) return 2; std::vector<uint8_t> d; std::fseek(f, 0, SEEK_END); d.resize((size_t)std::ftell(f)); std::rewind(f); if (std::fread(d.data(), 1, d.size(), f) != d.size()) return 2; std::fclose(f);
    vag::Track t; const bool ok = std::string(argv[1]) == "vag" ? vag::decodeVag(d, t) : vag::decodeVpk(d, t); if (!ok) { std::fprintf(stderr, "formato inválido\n"); return 1; }
    FILE* o = std::fopen(argv[3], "wb"); if (!o) return 2; std::fwrite(t.pcm.data(), 2, t.pcm.size(), o); std::fclose(o); std::fprintf(stderr, "%u Hz, %d canales, %zu muestras por canal\n", t.rate, t.channels, t.frames()); return 0;
}
