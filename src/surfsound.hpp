// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// GENERADO por tools/surf_sound_table.py (no editar a mano) / GENERATED, do not edit.
// Superficie del suelo -> material de sonido. VALIDADO contra el ejecutable PAL (docs/formats/audio.md,
// «superficie → material, resuelto»): `kind = grupo(nivel) * 32 + bits 11-15 del u16 de superficie`
// (USA func_001C32D0 suma 0x20 por grupo de niveles; PAL idéntico), y de `kind` salen los nombres de los
// bancos B_<MAT>_{PEDAL,COAST,LAND_SM,...}. Material 0 = sin sonido. Referencia cruzada: decompilación USA
// de github.com/skuza-oss/downhill-revived (GPL-3.0-or-later), solo para el material de LAND (ver el script).
#pragma once
#include <cstdint>
#include <string>
namespace surfsound {
inline constexpr const char* kMat[] = {"", "BRDG", "CONC", "DIRT", "GRAS", "H2O", "ICE", "LAVA", "MUD", "ROCK", "ROOF", "SAND", "SNOW", "TRAK", "FENCE"};
inline constexpr uint8_t kRide[316] = {
    9, 9, 4, 9, 0, 12, 5, 0, 0, 0, 9, 0, 9, 9, 9, 9, 0, 0, 0, 0, 9, 0, 0, 9, 0, 0, 9, 9, 0, 0, 0, 0,
    9, 3, 0, 9, 0, 0, 5, 8, 1, 0, 9, 0, 0, 9, 9, 9, 0, 0, 0, 9, 9, 0, 0, 9, 9, 0, 0, 9, 0, 0, 0, 0,
    9, 3, 4, 9, 0, 0, 5, 8, 1, 9, 9, 0, 9, 9, 9, 9, 0, 2, 7, 0, 0, 0, 0, 0, 11, 0, 0, 9, 0, 0, 0, 0,
    9, 3, 4, 9, 0, 0, 5, 0, 1, 9, 1, 0, 0, 9, 9, 9, 0, 2, 0, 0, 0, 9, 0, 0, 0, 13, 10, 9, 0, 0, 0, 0,
    9, 0, 0, 9, 0, 12, 5, 0, 1, 0, 9, 0, 9, 9, 9, 9, 6, 2, 0, 9, 0, 0, 0, 0, 0, 13, 10, 9, 9, 0, 9, 0,
    9, 3, 4, 9, 0, 0, 5, 0, 1, 9, 1, 9, 9, 9, 9, 9, 0, 2, 9, 9, 0, 0, 0, 0, 0, 0, 10, 9, 0, 0, 0, 0,
    9, 3, 4, 9, 0, 12, 5, 3, 10, 0, 9, 0, 9, 9, 9, 0, 0, 2, 0, 0, 9, 0, 0, 0, 0, 0, 10, 9, 0, 0, 0, 0,
    9, 3, 4, 9, 0, 0, 5, 0, 1, 9, 9, 9, 9, 9, 9, 9, 0, 9, 0, 9, 9, 0, 0, 0, 0, 0, 9, 9, 9, 9, 0, 0,
    9, 3, 4, 9, 0, 12, 5, 8, 13, 9, 9, 9, 13, 9, 9, 9, 0, 2, 0, 0, 9, 0, 0, 0, 11, 13, 13, 9, 0, 0, 0, 0,
    3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 14
};
inline constexpr uint8_t kLand[316] = {
    9, 9, 9, 9, 0, 12, 5, 0, 0, 0, 9, 0, 9, 9, 9, 9, 0, 0, 0, 0, 9, 0, 0, 9, 0, 0, 9, 9, 0, 0, 0, 0,
    9, 3, 0, 9, 0, 0, 5, 8, 1, 0, 9, 0, 0, 9, 9, 9, 0, 0, 0, 9, 9, 0, 0, 9, 9, 0, 0, 9, 0, 0, 0, 0,
    9, 3, 9, 9, 0, 0, 5, 8, 1, 9, 9, 0, 1, 9, 9, 9, 0, 9, 7, 0, 0, 0, 0, 0, 9, 0, 0, 9, 0, 0, 0, 0,
    9, 3, 4, 9, 0, 0, 5, 0, 1, 9, 9, 0, 0, 9, 9, 9, 0, 9, 0, 0, 0, 9, 0, 0, 0, 9, 9, 9, 0, 0, 0, 0,
    9, 0, 0, 9, 0, 12, 5, 0, 1, 0, 9, 0, 9, 9, 9, 9, 12, 9, 0, 9, 0, 0, 0, 0, 0, 13, 9, 9, 9, 0, 9, 0,
    9, 3, 4, 9, 0, 0, 5, 0, 9, 9, 9, 9, 9, 9, 9, 9, 0, 2, 9, 9, 0, 0, 0, 0, 0, 0, 10, 9, 0, 0, 0, 0,
    9, 3, 4, 9, 0, 12, 5, 3, 10, 0, 9, 0, 9, 9, 9, 0, 0, 2, 0, 0, 9, 0, 0, 0, 0, 0, 10, 9, 0, 0, 0, 0,
    9, 3, 4, 9, 0, 0, 5, 0, 1, 9, 9, 9, 9, 9, 9, 9, 0, 9, 0, 9, 9, 0, 0, 0, 0, 0, 9, 9, 9, 9, 0, 0,
    9, 3, 4, 9, 0, 12, 5, 8, 13, 9, 9, 9, 13, 9, 9, 9, 0, 2, 0, 0, 9, 0, 0, 0, 11, 13, 13, 9, 0, 0, 0, 0,
    3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3
};

// Grupo de niveles (mismos grupos que el juego usa para sumar 0x20 al kind). Desconocido -> 0 (como el `default` del juego).
inline int levelGroup(std::string n) {
    for (char& c : n) c = (char)std::toupper((unsigned char)c);
    struct P { const char* pre; int g; };
    static const P ps[] = {{"ALP", 0}, {"MOA", 1}, {"JUN", 2}, {"PER", 3}, {"GLA", 4}, {"AUB", 5}, {"FUJ", 6}, {"CIT", 7}, {"BC", 8}, {"MOS", 9}};
    for (const P& p : ps) if (n.rfind(p.pre, 0) == 0) return p.g;
    return 0;
}
inline int kind(int group, uint16_t surface) { return group * 32 + ((surface >> 11) & 31); }
// Nombre completo del sonido de rodadura ("B_ROCK_PEDAL" / "B_ROCK_COAST"); vacío si la superficie no suena.
inline std::string rideName(int kind, bool pedalling) {
    const uint8_t m = kRide[kind % 316]; if (!m) return {};
    if (kMat[m][0] == 'F') return "B_SCR_FENCE_L";
    return std::string("B_") + kMat[m] + (pedalling ? "_PEDAL" : "_COAST");
}
// Material de aterrizaje/choque ("ROCK", "SNOW", ...); nulo si el juego usa el caso por defecto.
inline const char* landMat(int kind) { const uint8_t m = kLand[kind % 316]; return m ? kMat[m] : nullptr; }
}  // namespace surfsound
