// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Verificación con datos reales (no va a CI): ui_check base_sin_extension hud|lbr salida -> src/uiextract.hpp (HUD de un nivel o imagen de una pantalla de carga LOADBAR); tools/ui_check.py compara los bytes con hud_export.py y loadbar_export.py.
#include "../src/uiextract.hpp"
#include <cstdio>
#include <string>
static bool rd(const std::string& p, std::vector<uint8_t>& r) { FILE* f = std::fopen(p.c_str(), "rb"); if (!f) return false; std::fseek(f, 0, SEEK_END); r.resize(std::ftell(f)); std::rewind(f); bool ok = std::fread(r.data(), 1, r.size(), f) == r.size(); std::fclose(f); return ok; }
int main(int argc, char** argv) {
    std::vector<uint8_t> n, p, t, c; const std::string b = argc > 3 ? argv[1] : "", mode = argc > 3 ? argv[2] : "";
    if (argc < 4 || !rd(b + ".NGP", n) || !rd(b + ".PTR", p) || !rd(b + ".TEX", t) || !rd(b + ".RTX", c)) { std::fprintf(stderr, "uso: ui_check base hud|lbr salida\n"); return 2; }
    tex::Level L; if (!tex::load(n, p, t, c, L)) return 1; std::vector<uint8_t> out; const bool ok = mode == "hud" ? uiextract::hudDat(L, out) : uiextract::loadbarLbr(L, out); if (!ok) return 3;
    FILE* o = std::fopen(argv[3], "wb"); if (!o) return 2; std::fwrite(out.data(), 1, out.size(), o); std::fclose(o); return 0;
}
