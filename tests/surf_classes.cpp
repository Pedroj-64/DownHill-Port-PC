// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Herramienta de análisis (docs/formats/audio.md, «superficie → material»): histograma de las clases del u16 de
// superficie de cada LVL/*.NGP de la ISO. El u16 del triángulo de colisión guarda DOS clases: bits 0-4 = clase
// física (indexa DAT_0077a3d8 en FUN_00136330, steer_forces.c) y bits 5-10 = clase de efecto (indexa la tabla
// por nivel DAT_003b7e00 en FUN_00131C90:339, FUN_001631e0:71; de ahí sale el polvo y el efecto de impacto).
// Uso: build/surf_classes imagen.iso [--full]   (--full lista también los u16 completos)
// No va a ctest: es una consulta, no un oráculo. La salida es una lista de recuentos (dato derivado, no del juego).
#include "../src/iso9660.hpp"
#include "../src/ie.hpp"
#include "../src/ngp.hpp"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <map>
#include <set>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    if (argc < 2) { std::fprintf(stderr, "uso: surf_classes imagen.iso [--full]\n"); return 2; }
    const bool full = argc > 2 && std::string(argv[2]) == "--full";
    iso9660::Image img; if (!img.open(argv[1])) { std::fprintf(stderr, "no se puede abrir %s\n", argv[1]); return 2; }
    std::vector<iso9660::Entry> es; if (!img.list(es)) return 1;
    std::vector<std::pair<std::string, const iso9660::Entry*>> lvls;
    for (const auto& e : es) { std::string k = e.path; for (char& c : k) c = (char)std::toupper((unsigned char)c);
        if (e.dir || k.rfind("LVL/", 0) != 0 || k.size() < 8 || k.substr(k.size() - 4) != ".NGP") continue; lvls.push_back({e.path, &e}); }
    std::sort(lvls.begin(), lvls.end());
    for (auto& [path, ep] : lvls) {
        std::vector<uint8_t> raw, ngp; if (!img.read(*ep, raw)) continue; if (!ie::unpack(raw, ngp)) ngp = std::move(raw);
        std::vector<ngp::ColTri> t; if (!ngp::collisionTrisInstanced(ngp, t)) { std::fprintf(stderr, "%s: colisión inválida\n", path.c_str()); continue; }
        std::map<int, long> h5, h6; std::map<uint32_t, long> hfull;
        for (auto& c : t) { h5[c.surface & 0x1F]++; h6[(c.surface >> 5) & 0x3F]++; hfull[c.surface]++; }
        std::string base = path; { size_t s = base.find_last_of('/'), d = base.find_last_of('.'); base = base.substr(s + 1, d - s - 1); }
        std::printf("%-11s tri=%-7zu física:", base.c_str(), t.size());
        for (auto& [k, v] : h5) std::printf(" %d:%ld", k, v);
        std::printf("  | efecto:");
        for (auto& [k, v] : h6) std::printf(" %d:%ld", k, v);
        std::printf("\n");
        if (full) { std::printf("            full:"); for (auto& [k, v] : hfull) std::printf(" %#06x:%ld", k, v); std::printf("\n"); }
    }
    return 0;
}
