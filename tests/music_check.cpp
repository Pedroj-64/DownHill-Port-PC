// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Verificación con datos reales (no va a CI): music_check imagen.iso -> para cada nivel (LVL/*.NGP) calcula la música que
// le toca con src/music.hpp (misma tabla que el ejecutable) y comprueba que el fichero está en VAG/ de la imagen y que el
// port lo decodifica (vag::decodeVag de la capa LEFT o vag::decodeVpk de la pista). Imprime una línea por nivel y sale
// con 1 si a alguno le falta o no decodifica.
#include "../src/music.hpp"
#include "../src/iso9660.hpp"
#include "../src/vag.hpp"
#include <cstdio>
#include <set>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    iso9660::Image img; if (argc < 2 || !img.open(argv[1])) { std::fprintf(stderr, "uso: music_check imagen.iso\n"); return 2; }
    std::vector<iso9660::Entry> es; if (!img.list(es)) return 1;
    std::set<std::string> vag;
    std::vector<std::string> lvls;
    for (const auto& e : es) {
        if (e.dir) continue;
        const std::string k = music::upper(e.path);
        if (k.rfind("VAG/", 0) == 0) vag.insert(k.substr(4));
        else if (k.rfind("LVL/", 0) == 0 && k.size() > 8 && k.compare(k.size() - 4, 4, ".NGP") == 0) lvls.push_back(k.substr(4, k.size() - 8));
    }
    int bad = 0;
    for (const std::string& l : lvls) {
        const music::Kind kind = music::levelKind(l);
        std::string det, falta;
        bool ok = true;
        if (kind == music::Kind::Tour) {
            const std::string th = music::themeLetter(music::themeIndex(l));
            for (char s : {'L', 'R'}) {
                const std::string n = music::layerName(th, "HIGH", s);
                if (!vag.count(n)) { falta += " " + n; ok = false; continue; }
                vag::Track t; std::vector<uint8_t> raw; bool got = false;
                for (const auto& e : es) if (music::upper(e.path) == "VAG/" + n) got = img.read(e, raw);
                if (!got || !vag::decodeVag(raw, t) || t.rate != 44100) { falta += " " + n + "(no decodifica)"; ok = false; }
            }
            det = "circuito  tema " + th + " capa HIGH (L+R)";
        } else {
            const std::string t = music::trackName(l);
            det = (kind == music::Kind::Tdh ? "tour DH   pista " : "MX        pista ") + t;
            if (!vag.count(t)) { falta = " " + t; ok = false; }
            else {
                vag::Track tr; std::vector<uint8_t> raw; bool got = false;
                for (const auto& e : es) if (music::upper(e.path) == "VAG/" + t) got = img.read(e, raw);
                if (!got || !vag::decodeVpk(raw, tr) || tr.channels != 2 || tr.rate != 44100) { falta = " " + t + "(no decodifica)"; ok = false; }
            }
        }
        std::printf("%s %-10s %-34s %s\n", ok ? "OK  " : "MAL ", l.c_str(), det.c_str(), ok ? "" : ("FALTA" + falta).c_str());
        bad += !ok;
    }
    std::printf("%zu niveles, %d con música que falta o no decodifica\n", lvls.size(), bad);
    return bad != 0;
}
