// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Verificación con datos reales (no va a CI): iso_check imagen.iso [--unpack] -> una línea por archivo «tamaño crc32 ruta» (y «DIR ruta» por directorio) con el lector nativo src/iso9660.hpp; con --unpack los datos pasan antes por src/ie.hpp
// (si una capa IE falla se usan los bytes originales, como unpack_ie.py). tools/iso_check.py lo compara con iso_extract/ (o unpacked/ con --unpack).
#include "../src/iso9660.hpp"
#include "../src/ie.hpp"
#include <cstdio>
#include <string>
static uint32_t crcUpdate(uint32_t c, const uint8_t* p, size_t n) { static uint32_t T[256]; static bool init = false; if (!init) { for (uint32_t i = 0; i < 256; i++) { uint32_t v = i; for (int k = 0; k < 8; k++) v = v & 1 ? 0xEDB88320u ^ (v >> 1) : v >> 1; T[i] = v; } init = true; } for (size_t i = 0; i < n; i++) c = T[(c ^ p[i]) & 255] ^ (c >> 8); return c; }
int main(int argc, char** argv) {
    iso9660::Image img; if (argc < 2 || !img.open(argv[1])) { std::fprintf(stderr, "no se puede abrir la imagen ISO9660: %s\n", argc > 1 ? argv[1] : "(sin argumento)"); return 2; }
    const bool unpack = argc > 2 && std::string(argv[2]) == "--unpack";
    std::vector<iso9660::Entry> es; if (!img.list(es)) { std::fprintf(stderr, "directorios inválidos\n"); return 1; }
    for (const auto& e : es) {
        if (e.dir) { std::printf("DIR %s\n", e.path.c_str()); continue; }
        uint32_t c = 0xFFFFFFFFu; std::vector<uint8_t> chunk;
        if (unpack) { std::vector<uint8_t> raw, out; if (!img.read(e, raw)) { std::fprintf(stderr, "lectura fuera de rango: %s\n", e.path.c_str()); return 1; } if (!ie::unpack(raw, out)) out = raw; c = crcUpdate(c, out.data(), out.size()); std::printf("%zu %08x %s\n", out.size(), ~c, e.path.c_str()); continue; }
        for (uint64_t off = 0; off < e.size; off += (1u << 22)) { const size_t n = (size_t)std::min<uint64_t>(1u << 22, e.size - off); if (!img.readRange(e, off, n, chunk)) { std::fprintf(stderr, "lectura fuera de rango: %s\n", e.path.c_str()); return 1; } c = crcUpdate(c, chunk.data(), chunk.size()); }
        std::printf("%u %08x %s\n", e.size, ~c, e.path.c_str());
    }
    return 0;
}
