// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Verificación con datos reales (no va a CI): tex_check base_sin_extension -> decodifica con src/tex.hpp todos los materiales de base.NGP/.PTR/.TEX/.RTX y escribe, por material resuelto y en el archivo `salida`, "w h crc32-del-RGBA"
// (tools/tex_check.py compara cada textura del .mdl que genera el Python con esta lista).
#include "../src/tex.hpp"
#include <cstdio>
#include <string>
static bool rd(const std::string& p, std::vector<uint8_t>& r) { FILE* f = std::fopen(p.c_str(), "rb"); if (!f) return false; std::fseek(f, 0, SEEK_END); r.resize(std::ftell(f)); std::rewind(f); bool ok = std::fread(r.data(), 1, r.size(), f) == r.size(); std::fclose(f); return ok; }
static uint32_t crc32(const std::vector<uint8_t>& d) { static uint32_t T[256]; static bool init = false; if (!init) { for (uint32_t i = 0; i < 256; i++) { uint32_t c = i; for (int k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1; T[i] = c; } init = true; } uint32_t c = 0xFFFFFFFFu; for (uint8_t b : d) c = T[(c ^ b) & 255] ^ (c >> 8); return ~c; }
int main(int argc, char** argv) {
    std::vector<uint8_t> n, p, t, c; const std::string b = argc > 1 ? argv[1] : "";
    if (argc < 3 || !rd(b + ".NGP", n) || !rd(b + ".PTR", p) || !rd(b + ".TEX", t) || !rd(b + ".RTX", c)) { std::fprintf(stderr, "uso: tex_check base salida\n"); return 2; }
    tex::Level L; if (!tex::load(n, p, t, c, L)) { std::fprintf(stderr, "PTR/TEX inválido\n"); return 1; }
    FILE* o = std::fopen(argv[2], "w"); if (!o) return 2; size_t bad = 0;
    for (const tex::Material& m : L.mats) {
        uint32_t w, h; std::vector<uint8_t> px; if (!tex::makeTexture(L, m, w, h, px)) { bad++; continue; }
        std::fprintf(o, "%u %u %08x\n", w, h, crc32(px));
    }
    std::fclose(o); std::fprintf(stderr, "%zu materiales, %zu sin resolver\n", L.mats.size(), bad); return 0;
}
