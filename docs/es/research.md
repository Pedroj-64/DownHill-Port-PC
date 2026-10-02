<!-- SPDX-FileCopyrightText: 2026 Pedro Soto -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Flujo de investigación

🇬🇧 [English version](../en/research.md)

Copyright (C) 2026 Pedro Soto y las personas que contribuyen a DownHill-Port-PC. Licenciado bajo GPL-3.0-or-later.

Este documento explica cómo se obtuvieron los hallazgos de `formats.md`, para que cualquiera pueda reproducirlos y ampliarlos.
**Nunca subas datos del juego, el ejecutable, assets extraídos, proyectos de Ghidra ni salida del decompilador.**

## 1. Preparar los datos (tu propia imagen de disco)

```sh
7z x "Downhill Domination.iso" -oiso_extract        # contenido del disco (ignorado por git)
python3 tools/unpack_ie.py iso_extract unpacked     # descomprime los contenedores IE (ignorado por git)
```

El disco es la versión PAL (`SLES_522.02`). El ejecutable es un ELF MIPS R5900 sin símbolos (entrada `0x10a008`)
con microcódigo VU en `.vutext`.

## 2. Configurar Ghidra

1. Instala Ghidra (`pacman -S ghidra` en Arch).
2. Instala la versión de [ghidra-emotionengine-reloaded](https://github.com/chaoticgd/ghidra-emotionengine-reloaded) que corresponda a tu Ghidra
   en `~/.config/ghidra/<version_de_ghidra>_DEV/Extensions/`. El MIPS genérico produce una salida rota para el R5900 (MMI, operaciones de 128 bits).
3. Importa con el lenguaje `r5900:LE:32:default`:

```sh
mkdir ghidra_projects
ghidra-analyzeHeadless ghidra_projects dh -import iso_extract/SLES_522.02 -processor "r5900:LE:32:default" -overwrite
```

## 3. Scripts (`tools/ghidra/`)

| Script | Uso |
|--------|-----|
| `run.sh Script.java args...` | Ejecuta un script sobre el programa importado sin reanalizar |
| `StrRefs.java <subcadena> <salida> [n]` | Busca cadenas definidas que contengan `<subcadena>` y decompila las funciones que las referencian |
| `DecompFn.java <salida> <dir>...` | Decompila las funciones en las direcciones hexadecimales dadas y las escribe en `<salida>` |
| `RefsTo.java <salida> <dir>...` | Lista las referencias a direcciones (p. ej. cadenas de extensiones) |
| `RefsSym.java <salida> <subcadena>...` | Lista las referencias a símbolos por nombre |

Escribe las salidas en `decomp/` (ignorado por git).

## 4. Método que funcionó

El juego trae muchas cadenas de depuración (`"Texture Setup w %d h %d ..."`, `"CLUTs Loaded = ..."`). Buscar cadenas → seguir las xrefs →
decompilar la función → confirmar la estructura con datos reales mediante un script pequeño de Python → documentarla en `formats.md`.
Ejemplos: la cadena de registros de texturas, la tabla de relocalización `.PTR` y el orden de carga NGP/RTX/TEX/PTR salieron así.

## 5. Comprobaciones visuales

`dhview` puede capturar un fotograma para revisarlo:

```sh
DH_CAM="x y z yaw pitch" DH_SHOT=salida.bmp ./build/dhview malla.msh
```

## 6. Dónde ayudar ahora

- Escribir el intérprete del VU1 (el microcódigo de `.vutext` @ `0x00269d90`, `0xA140` B, ya se puede desensamblar con `tools/vudis.py`) junto con el desempaquetador VIF y el parser GIF para obtener geometría exacta.
- Recorrer el grafo de escena: tabla de nodos en el offset `0xB66F00` del NGP, nodos de 0xC0 B con una matriz 4x4.
- Encontrar el enlace entre texturas y paletas (CLUTs en `.RTX`).
- Formatos aún sin tocar: `.PTS`, `.RST`, `.REP`, `.BNK`, `.SKX`, `.CTL`, `.APT`, `.BHS`.
