// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Oráculo de src/music.hpp: la clasificación de niveles debe coincidir con las tres ramas del gestor de música del ELF
// (FUN_001e2cc0: FUN_001e2c80 = circuito, FUN_001e2c40 = tour de downhill, resto = MX) para los 32 identificadores de
// nivel de la tabla de nombres (VA 0x2760D0) y para los 54 ficheros LVL/*.NGP de la imagen; las tres tablas de nombres
// (letras en VA 0x2AEB60, pistas TDH en +0xcc y MX en +0xf4 de FUN_001e2280) deben estar copiadas tal cual.
#include "../src/music.hpp"
#include <cstdio>
#include <cstring>
#include <set>
#include <string>

static int fails = 0;
static void ck(bool c, const std::string& what) { if (!c) { std::printf("MAL %s\n", what.c_str()); fails++; } }

int main() {
    using music::Kind;
    // Tabla del ELF: id -> nombre (VA 0x2760D0) y los dos conjuntos de FUN_001e2c80 / FUN_001e2c40.
    const char* kNames[32] = {"ALPINE", "MOAB", "JUNGLE", "PERU", "PERUTDH", "GLACIER", "ALPINEMX", "MOABMX", "JUNGLEMX",
        "PERUMX", "JUNGLETD", "AUBERST", "FUJI", "MOABTDH", "GLACIERX", "AUBERMX", "AUBERSTD", "CITYMX", "CITYTDH", "BC",
        "CITY", "DUAL", "GLACIERT", "MOSH", "ALPINET", "BCTDX", "FUJIPRAC", "MONKEY", "TRAINER", "PODSUP", "PODSPE", "PODARC"};
    const std::set<int> kTour = {0, 1, 2, 3, 5, 0xb, 0xc, 0x13, 0x14};                        // FUN_001e2c80: música por capas
    const std::set<int> kTdh = {4, 0xa, 0xd, 0x10, 0x12, 0x16, 0x18, 0x19};                    // FUN_001e2c40: <tema>TDH.VPK
    for (int i = 0; i < 32; i++) {
        const Kind want = kTour.count(i) ? Kind::Tour : kTdh.count(i) ? Kind::Tdh : Kind::Mx;
        ck(music::levelKind(kNames[i]) == want, std::string("id ") + std::to_string(i) + " (" + kNames[i] + ") mal clasificado");
    }
    // Los 54 ficheros de LVL/ (variantes de dos jugadores y de maqueta incluidas): todos deben caer en su nivel.
    struct { const char* f; Kind k; } kFiles[] = {
        {"ALPINE", Kind::Tour}, {"ALP2", Kind::Tour}, {"ALPT2", Kind::Tdh}, {"ALPX2", Kind::Mx}, {"ALPINEMX", Kind::Mx},
        {"ALPINET", Kind::Tdh}, {"MOAB", Kind::Tour}, {"MOAB2", Kind::Tour}, {"MOABMX", Kind::Mx}, {"MOABMX2", Kind::Mx},
        {"MOABTDH", Kind::Tdh}, {"MOABTDH2", Kind::Tdh}, {"JUNGLE", Kind::Tour}, {"JUNGLE2", Kind::Tour},
        {"JUNGLET2", Kind::Tdh}, {"JUNGLEX2", Kind::Mx}, {"JUNGLEMX", Kind::Mx}, {"JUNGLETD", Kind::Tdh},
        {"PERU", Kind::Tour}, {"PERU2", Kind::Tour}, {"PERUMX", Kind::Mx}, {"PERUMX2", Kind::Mx}, {"PERUTDH", Kind::Tdh},
        {"PERUTDH2", Kind::Tdh}, {"GLACIER", Kind::Tour}, {"GLAC2P", Kind::Tour}, {"GLACT2P", Kind::Tdh},
        {"GLACX2P", Kind::Mx}, {"GLACIERT", Kind::Tdh}, {"GLACIERX", Kind::Mx}, {"AUBERST", Kind::Tour},
        {"AUBERST2", Kind::Tour}, {"AUBERSTD", Kind::Tdh}, {"AUBTDH2", Kind::Tdh}, {"AUBERMX", Kind::Mx},
        {"AUBERMX2", Kind::Mx}, {"FUJI", Kind::Tour}, {"FUJI2", Kind::Tour}, {"BC", Kind::Tour}, {"BC2", Kind::Tour},
        {"BCTDX", Kind::Tdh}, {"BCTDX2", Kind::Tdh}, {"CITY", Kind::Tour}, {"CITY2", Kind::Tour}, {"CITYTDH", Kind::Tdh},
        {"CITYMX", Kind::Mx}, {"DUAL", Kind::Mx}, {"MOSH", Kind::Mx}, {"MOSH2", Kind::Mx}, {"MONKEY", Kind::Mx},
        {"TRAINER", Kind::Mx}, {"PODSUP", Kind::Mx}, {"PODSPE", Kind::Mx}, {"PODARC", Kind::Mx},
    };
    ck(sizeof kFiles / sizeof kFiles[0] == 54, "la lista de ficheros no tiene 54 entradas");
    for (auto& t : kFiles) ck(music::levelKind(t.f) == t.k, std::string("fichero ") + t.f + " mal clasificado (" +
        (music::levelKind(t.f) == Kind::Tour ? "Tour" : music::levelKind(t.f) == Kind::Tdh ? "TDH" : "MX") + ")");
    // Las tres tablas del ELF, copiadas tal cual.
    const char* kElfThemes[10] = {"C", "E", "E2", "G", "HH", "O", "P", "T1", "T2", "T"};
    for (int i = 0; i < 10; i++) ck(std::strcmp(music::kThemes[i], kElfThemes[i]) == 0, "letra de tema " + std::to_string(i));
    const char* kElfTdh[10] = {"CTDH.VPK", "ETDH.VPK", "GTDH.VPK", "OTDH.VPK", "PTDH.VPK",
                               "T1TDH.VPK", "T2TDH.VPK", "TTDH.VPK", "E2TDH.VPK", "HHTDH.VPK"};
    for (int i = 0; i < 10; i++) ck(std::strcmp(music::kTdhTracks[i], kElfTdh[i]) == 0, "pista TDH " + std::to_string(i));
    const char* kElfMx[10] = {"T2MX.VPK", "OMX.VPK", "CMX.VPK", "T1MX.VPK", "PMX.VPK",
                              "GMX.VPK", "TMX.VPK", "EMX.VPK", "E2MX.VPK", "HHMX.VPK"};
    for (int i = 0; i < 10; i++) ck(std::strcmp(music::kMxTracks[i], kElfMx[i]) == 0, "pista MX " + std::to_string(i));
    const char* kElfLayers[4] = {"HIGH", "MED", "LOW", "DRUM"};
    for (int i = 0; i < 4; i++) ck(std::strcmp(music::kLayers[i], kElfLayers[i]) == 0, "capa " + std::to_string(i));
    // trackName: los de circuito no tienen pista completa; los demás usan la permutación que les toca.
    ck(music::trackName("ALPINE").empty(), "ALPINE no debería tener pista completa");
    ck(music::trackName("CITY").empty(), "CITY no debería tener pista completa");
    ck(music::trackName("PERUTDH", 0) == "CTDH.VPK", "índice 0 en PERUTDH (orden TDH)");
    ck(music::trackName("PERUTDH", 4) == "PTDH.VPK", "índice 4 en PERUTDH (orden TDH: C,E,G,O,P)");
    ck(music::trackName("DUAL", 0) == "T2MX.VPK", "índice 0 en DUAL (orden MX: T2,O,C,T1,P)");
    ck(music::trackName("DUAL", 4) == "PMX.VPK", "índice 4 en DUAL (orden MX)");
    ck(music::trackName("GLACIERT", 9) == "HHTDH.VPK", "índice 9 en un nivel TDH");
    ck(music::trackName("ALPINEMX", 9) == "HHMX.VPK", "índice 9 en un nivel MX");
    for (const char* f : {"MOABMX", "GLACIERX", "DUAL", "PERUTDH", "GLACIERT", "BCTDX2"}) {
        const std::string t = music::trackName(f);
        const char* suf = music::levelKind(f) == Kind::Tdh ? "TDH.VPK" : "MX.VPK";
        ck(t.ends_with(suf), std::string("sufijo de ") + f + " -> " + t);
    }
    // Índice de tema: 0..9, estable, insensible a mayúsculas y repartido entre niveles.
    for (const char* f : kNames) ck(music::themeIndex(f) <= 9, std::string("índice fuera de rango en ") + f);
    ck(music::themeIndex("ALPINE") == music::themeIndex("alpine"), "el índice debe ser insensible a mayúsculas");
    ck(music::themeIndex("ALPINE") == music::themeIndex("ALPINE"), "el índice debe ser estable");
    std::set<unsigned> seen; for (const char* f : kNames) seen.insert(music::themeIndex(f));
    ck(seen.size() >= 5, "los 32 niveles deberían repartirse entre varios temas");
    // Capas: nombre exacto de FUN_001e1d10 y una capa desconocida cae en HIGH.
    ck(music::layerName("E2", "high", 'l') == "E2HIGH-L.VAG", "capa E2 high L");
    ck(music::layerName("HH", "DRUM", 'R') == "HHDRUM-R.VAG", "capa HH drum R");
    ck(music::layerName("T1", "zzz", 'L') == "T1HIGH-L.VAG", "capa desconocida -> HIGH");
    // Las diez letras tienen sus cuatro capas y sus dos lados.
    for (const char* th : kElfThemes) for (const char* l : kElfLayers) for (char s : {'L', 'R'}) {
        const std::string n = music::layerName(th, l, s);
        ck(n == std::string(th) + l + "-" + s + ".VAG", "capa " + n);
    }
    std::printf("%s music_test: 32 ids + 54 ficheros, %d fallos\n", fails ? "MAL " : "OK  ", fails);
    return fails != 0;
}
