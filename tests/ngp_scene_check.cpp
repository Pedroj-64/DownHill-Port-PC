// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Verificación con datos reales (no va a CI): ngp_scene_check nivel.NGP -> imprime raíces, tipos de nodo y «kinds» del recorrido nativo (src/ngp.hpp) en líneas canónicas que tools/ngp_scene_check.py compara con tools/scene.py.
#include "../src/ngp.hpp"
#include <cstdio>
int main(int argc, char** argv) {
    std::vector<uint8_t> d; FILE* f = argc > 1 ? std::fopen(argv[1], "rb") : nullptr; if (!f) { std::fprintf(stderr, "uso: ngp_scene_check nivel.NGP\n"); return 2; }
    std::fseek(f, 0, SEEK_END); d.resize(std::ftell(f)); std::rewind(f); if (std::fread(d.data(), 1, d.size(), f) != d.size()) return 2; std::fclose(f);
    ngp::SceneStats st; if (!ngp::sceneStats(d, st)) { std::fprintf(stderr, "NGP inválido\n"); return 1; }
    std::printf("roots"); for (size_t o : st.roots) std::printf(" %zu", o); std::printf("\nvisited %zu\ntypes", st.visited); for (auto& [t, n] : st.types) std::printf(" %u:%u", t, n);
    std::printf("\nkinds"); for (auto& [k, n] : st.kinds) std::printf(" %u/%u:%u", k.first, k.second, n); std::printf("\n"); return 0;
}
