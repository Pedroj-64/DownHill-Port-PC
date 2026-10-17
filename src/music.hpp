// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Qué música toca cada nivel (leído del ejecutable el 2026-10-16). El gestor de música es FUN_001e2cc0 y decide entre
// tres comportamientos según el identificador de nivel `DAT_004dcb40` (la tabla de nombres está en VA 0x2760D0):
//   * FUN_001e2c80() != 0 -> los nueve niveles del circuito, ids {0,1,2,3,5,0xb,0xc,0x13,0x14} = ALPINE, MOAB, JUNGLE,
//     PERU, GLACIER, AUBERST, FUJI, BC, CITY: música **por capas** `<tema>{HIGH,MED,LOW,DRUM}-{L,R}.VAG`, que FUN_001e1d10
//     abre con un tema 0..9 **al azar** y FUN_001e27b8 va cambiando de capa (una a la vez, con 10 cuadros de fundido).
//   * FUN_001e2c40() != 0 -> los ocho niveles «tour de downhill», ids {4,0xa,0xd,0x10,0x12,0x16,0x18,0x19} = PERUTDH,
//     JUNGLETD, MOABTDH, AUBERSTD, CITYTDH, GLACIERT, ALPINET, BCTDX: una pista **<tema>TDH.VPK** al azar.
//   * el resto (los MX: ALPINEMX, MOABMX, JUNGLEMX, PERUMX, AUBERMX, CITYMX, GLACIERX, y además DUAL, MOSH, MONKEY,
//     TRAINER, PODSUP/PODSPE/PODARC, FUJIPRAC) -> una pista **<tema>MX.VPK** al azar.
// La lista de 40 pistas la rellena FUN_001e2280 (10 TDH en +0xcc, 10 MX en +0xf4 y 20 licenciadas `NN_LIC.VPK` en +0x11c);
// ojo: los tres órdenes son **distintos** entre sí (el de TDH es C,E,G,O,P,T1,T2,T,E2,HH y el de MX es T2,O,C,T1,P,G,T,E,E2,HH),
// así que se copian los tres tal cual. Las letras de tema (sólo para la música por capas) salen de la tabla del ELF en
// VA 0x2AEB60 (paso de 3 bytes). El motor sortea con FUN_00228870 evitando repetir el tema anterior; aquí se usa un índice
// estable por nivel (FNV-1a) para que el resultado sea reproducible.
#pragma once
#include <cctype>
#include <cstdint>
#include <string>

namespace music {

enum class Kind { Tour, Tdh, Mx };

// Letras de tema para la música por capas, en el orden exacto de la tabla del ELF (VA 0x2AEB60, entradas de 3 bytes).
inline const char* const kThemes[10] = {"C", "E", "E2", "G", "HH", "O", "P", "T1", "T2", "T"};

// Las diez pistas TDH, en el orden de FUN_001e2280 (obj+0xcc). Es *otro* orden que el de las letras.
inline const char* const kTdhTracks[10] = {"CTDH.VPK", "ETDH.VPK", "GTDH.VPK", "OTDH.VPK", "PTDH.VPK",
                                           "T1TDH.VPK", "T2TDH.VPK", "TTDH.VPK", "E2TDH.VPK", "HHTDH.VPK"};

// Las diez pistas MX, en el orden de FUN_001e2280 (obj+0xf4): T2, O, C, T1, P, G, T, E, E2, HH.
inline const char* const kMxTracks[10] = {"T2MX.VPK", "OMX.VPK", "CMX.VPK", "T1MX.VPK", "PMX.VPK",
                                          "GMX.VPK", "TMX.VPK", "EMX.VPK", "E2MX.VPK", "HHMX.VPK"};

// Capas de la música por capas, en el orden de la tabla de flujos de FUN_001e1d10 (0x1a0..0x1bc): la 0 es HIGH, que es
// la capa con la que el motor arranca la carrera (obj+0x190 = 0).
inline const char* const kLayers[4] = {"HIGH", "MED", "LOW", "DRUM"};

inline std::string upper(std::string s) { for (char& c : s) c = (char)std::toupper((unsigned char)c); return s; }

// Nivel -> tipo de música. Los nombres son los de la tabla del ELF; los ficheros de LVL/ llevan variantes de dos jugadores
// y de la maqueta (ALP2, ALPT2, ALPX2, GLAC2P, GLACT2P, GLACX2P, JUNGLET2, JUNGLEX2, MOABMX2, MOABTDH2, PERUMX2,
// PERUTDH2, AUBERMX2, AUBERST2, AUBTDH2, BCTDX2, CITY2, CITYTDH, FUJI2, MOSH2, BC2...) que caen en su nivel: X = variante
// MX, T = variante TDH, 2/P/2P = variante de dos jugadores.
inline Kind levelKind(std::string n) {
    n = upper(std::move(n));
    auto pre = [&](const char* s) { return n.rfind(s, 0) == 0; };
    if (pre("PERUTD") || pre("JUNGLETD") || pre("JUNGLET2") || pre("MOABTDH") || pre("AUBERSTD") || pre("AUBTDH") ||
        pre("CITYTD") || pre("GLACIERT") || pre("GLACT") || pre("ALPINET") || pre("ALPT") || pre("BCTDX")) return Kind::Tdh;
    if (pre("ALPINEMX") || pre("ALPX") || pre("GLACIERX") || pre("GLACX") || pre("JUNGLEMX") || pre("JUNGLEX") ||
        pre("MOABMX") || pre("PERUMX") || pre("AUBERMX") || pre("CITYMX") || pre("FUJIPRAC")) return Kind::Mx;   // FUJIPRAC (id 0x1a) es la práctica de FUJI y el motor le da MX
    if (pre("ALPINE") || pre("ALP2") || pre("MOAB") || pre("JUNGLE") || pre("PERU") || pre("GLAC") || pre("AUBERST") ||
        pre("FUJI") || pre("BC") || pre("CITY")) return Kind::Tour;
    return Kind::Mx;   // DUAL, MOSH, MONKEY, TRAINER, POD*, FUJIPRAC
}

// Índice 0..9 estable por nivel (el juego sortea 0..9 en cada carrera con FUN_00228870 y evita el tema anterior; aquí
// FNV-1a del nombre del nivel, para que la elección sea reproducible y no la misma en todos los niveles).
inline unsigned themeIndex(const std::string& stem) {
    uint32_t h = 2166136261u;
    for (unsigned char c : upper(stem)) { h ^= c; h *= 16777619u; }
    return h % 10u;
}
inline const char* themeLetter(unsigned i) { return kThemes[i % 10u]; }

// Pista completa de un nivel TDH o MX (con la extensión); cadena vacía si el nivel es de circuito (música por capas).
// `idx` = 0..9 fuerza la entrada de la lista (10 = la del nivel). El nombre no lleva la carpeta: se busca en VAG/ de la imagen.
inline std::string trackName(std::string stem, unsigned idx = 10) {
    const Kind k = levelKind(stem);
    if (k == Kind::Tour) return {};
    const unsigned i = (idx < 10 ? idx : themeIndex(stem)) % 10u;
    return k == Kind::Tdh ? kTdhTracks[i] : kMxTracks[i];
}

// Una capa de la música de circuito: "<tema><CAPA>-<L|R>.VAG" (FUN_001e1d10 concatena la letra del tema y el nombre de la
// capa). Una capa desconocida cae en HIGH, que es la capa inicial del motor.
inline std::string layerName(const std::string& theme, std::string layer, char side) {
    layer = upper(std::move(layer));
    bool ok = false; for (const char* l : kLayers) ok = ok || layer == l;
    if (!ok) layer = "HIGH";
    return theme + layer + "-" + (side == 'R' || side == 'r' ? "R" : "L") + ".VAG";
}

}  // namespace music
