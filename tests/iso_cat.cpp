// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Herramienta de desarrollo: iso_cat imagen.iso RUTA salida [--unpack] -> extrae un archivo de la imagen (sin distinguir mayúsculas; con --unpack pasa por los contenedores IE). La salida son datos del juego: escríbela FUERA del repo (regla 4).
#include "../src/iso9660.hpp"
#include "../src/ie.hpp"
#include <algorithm>
#include <cctype>
#include <cstdio>
int main(int argc, char** argv) {
    iso9660::Image img; if (argc < 4 || !img.open(argv[1])) { std::fprintf(stderr, "uso: iso_cat imagen.iso RUTA salida [--unpack]\n"); return 2; }
    std::vector<iso9660::Entry> es; if (!img.list(es)) return 1; std::string want = argv[2]; for (char& c : want) c = (char)std::toupper((unsigned char)c);
    for (const auto& e : es) { std::string k = e.path; for (char& c : k) c = (char)std::toupper((unsigned char)c); if (k != want || e.dir) continue;
        std::vector<uint8_t> raw, out; if (!img.read(e, raw)) return 1; if (argc > 4 && std::string(argv[4]) == "--unpack") { if (!ie::unpack(raw, out)) out = raw; } else out = std::move(raw);
        FILE* o = std::fopen(argv[3], "wb"); if (!o) return 2; std::fwrite(out.data(), 1, out.size(), o); std::fclose(o); return 0; }
    std::fprintf(stderr, "no está en la imagen: %s\n", argv[2]); return 1;
}
