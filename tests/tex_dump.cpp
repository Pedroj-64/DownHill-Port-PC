// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Herramienta de desarrollo: tex_dump imagen.iso DIR/NOMBRE salida/ -> vuelca cada textura decodificable de DIR/NOMBRE.{NGP,PTR,TEX,RTX} (p. ej. SHELL/UI, LVL/ALP2) como salida/NNNN_<id>_<cbp>_<ancho>x<alto>.rgba.
// La salida son datos del juego: va fuera del repo (regla 4).
#include "../src/nativelevel.hpp"
#include <cstdio>
#include <set>
int main(int argc, char** argv) {
    if (argc < 4) { std::fprintf(stderr, "uso: tex_dump imagen.iso DIR/NOMBRE salida/\n"); return 2; }
    nativelevel::IsoSource iso; if (!iso.open(argv[1])) { std::fprintf(stderr, "no se puede abrir %s\n", argv[1]); return 2; }
    const std::string b = argv[2]; std::vector<uint8_t> n, p, t, c;
    if (!iso.read(b + ".NGP", n) || !iso.read(b + ".PTR", p) || !iso.read(b + ".TEX", t) || !iso.read(b + ".RTX", c)) { std::fprintf(stderr, "faltan NGP/PTR/TEX/RTX de %s\n", b.c_str()); return 1; }
    tex::Level L; if (!tex::load(n, p, t, c, L)) { std::fprintf(stderr, "PTR/TEX inválido\n"); return 1; }
    std::set<std::pair<uint32_t, uint32_t>> seen; int k = 0, ok = 0;
    for (const tex::Material& m : L.mats) {
        if (!seen.insert({m.tid, m.cbp}).second) continue; uint32_t W, H; std::vector<uint8_t> px; k++;
        if (!tex::makeTexture(L, m, W, H, px)) continue;
        char f[512]; std::snprintf(f, sizeof f, "%s/%04d_%u_%u_%ux%u.rgba", argv[3], k, m.tid, m.cbp, W, H);
        if (FILE* o = std::fopen(f, "wb")) { std::fwrite(px.data(), 1, px.size(), o); std::fclose(o); ok++; }
    }
    std::printf("%d materiales distintos, %d texturas volcadas\n", k, ok); return 0;
}
