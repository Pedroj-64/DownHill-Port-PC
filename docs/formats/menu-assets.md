# Arte y vídeo del menú original / Original menu art and video

## ES

**Qué hay en la ISO** (inventario medido 2026-10-23; todo se lee de la imagen del jugador, nada de esto va al repo):

| Dónde | Contenido | Estado en el port |
|---|---|---|
| `SHELL/UI.{NGP,PTR,TEX,RTX}` (+ `UIF/G/I/S`: 5 idiomas) | **Escena 3D del menú** (montaña, cielo, cascada) + 245 materiales 2D: logo, ilustraciones de circuito, atlas de nombres, retratos, nombres de piloto con bandera, patrocinadores, texto del menú de pausa/tienda, bicis, ruedas, horquillas, trajes | escena y arte del selector **hechos** (`src/menuart.hpp`, `dhview "<iso>"`); el resto sin usar |
| `SHELL/BIKESHOP*.{NGP,PTR,TEX,RTX}` (5 idiomas) | tienda de bicis (escena 3D) | sin mirar |
| `SHELL/SHBANIM.NGA`, `SHRANIM.NGA` | animaciones del menú (corredores) | sin mirar |
| `MOV/*.PSS` (38) | vídeos MPEG-2 640x368 25 fps: `ATTRACT` (intro, 125 s), `BIO*` (biografía por piloto, 15), `TRICKS`, `CRASHES`, `CREDITS`, `TDH`, `MX`, `REVIEW`, `LOADING`, avisos legales por idioma | **ffmpeg los decodifica** (`ffmpeg -i X.PSS`); sin integrar en el port |
| `SND/SHELL_S.BNK`, `SHELL_CH.BNK` | efectos y música del menú | bancos legibles (`bnk.hpp`); sin usar en el menú |
| `VAG/NN_LIC.VPK`... | pistas licenciadas (solo el reproductor del menú) | sin usar |

**Mapa de materiales de `SHELL/UI`** (numeración de `build/tex_dump imagen.iso SHELL/UI <dir>`, orden de materiales (id, CBP) distintos; las texturas salen con las filas invertidas: hay que voltearlas):
- 136, 137, 138: logo «Downhill DOMINATION» (tres mosaicos de 128x128 de izquierda a derecha).
- 125, 224, 233: atlas de nombres de nivel, 12 + 6 + 12 líneas (de arriba abajo): *Italian Alps FR/MX, Utah FR/MX/TD, Hawaii FR/MX/TD, Peru FR/MX/TD, Russia FR* | *Italian Alps TD, British Columbia MX, Utah2 FR/MX/TD, Monkeyball* | *Russia MX, Scotland FR/MX/TD, Japan FR, USA FR/MX/TD, British Columbia FR, Japan FR, Russia TD, Moshpit 1*.
- 222-241 (menos 224 y 233): ilustraciones pintadas de los 9 circuitos, **dos mitades de 256x256** (izquierda, derecha) = imagen 2:1: 222/223 Alpes, 225/226 Utah, 227/228 Hawaii, 229/230 Perú, 231/232 Rusia, 234/235 Escocia, 236/237 Japón, 238/239 EE. UU., 240/241 Columbia Británica.
- 22-34 (128x128): retratos de piloto; 18-21: nombres de piloto con bandera (4 por hoja); 53: textos del menú de tienda (*Bike Upgrades, Bike/Jersey, Spare Parts, Bonus Purchases, Resume, Sponsorship*); 36-41, 165: patrocinadores; 57-98: bicis, cuadros, ruedas; 99-105: horquillas; 171-220: texturas de traje de cada piloto.

**Correspondencia circuito → archivos** (HIPÓTESIS por nombre, sin verificar contra el código): Alps = ALPINE/ALPINEMX/ALPINET; Utah = MOAB/MOABMX/MOABTDH; Hawaii = JUNGLE/JUNGLEMX/JUNGLETD; Peru = PERU/PERUMX/PERUTDH; Russia = GLACIER/GLACIERX/GLACIERT; Scotland = AUBERST/AUBERMX/AUBERSTD; Japan = FUJI; USA = CITY/CITYMX/CITYTDH; British Columbia = BC (FR) y BCTDX (MX). El orden de los atlas NO es el de la tabla de ids del ELF (0x2760D0). Faltan en el selector: Utah2 (MOAB2…), Monkeyball, Moshpit, DUAL, TRAINER.

**Pendiente** (en este orden): selector de piloto con retratos 22-34 y nombres 18-21 (el orden retrato↔nombre↔modelo 3D es hipótesis); menú de pausa/tienda con 53; música y efectos de `SHELL_*`; decompilar el código del menú (la maquetación real, animaciones y transiciones no se conocen: la actual es provisional); vídeos `.PSS` (intro, créditos) con ffmpeg o un decodificador propio; capturas del menú real en PCSX2 para comparar (las 10 `.p2s` de `~/dh-captures/ref` son todas de carrera).

## EN

**What the ISO holds** (measured 2026-10-23; everything is read from the player's disc image, none of it goes into the repo):

| Where | Content | Port status |
|---|---|---|
| `SHELL/UI.{NGP,PTR,TEX,RTX}` (+ `UIF/G/I/S`: 5 languages) | **3D menu scene** (mountain, sky, waterfall) + 245 2D materials: logo, track illustrations, name atlases, portraits, rider names with flags, sponsors, pause/shop menu text, bikes, wheels, forks, riding suits | scene and selector art **done** (`src/menuart.hpp`, `dhview "<iso>"`); the rest unused |
| `SHELL/BIKESHOP*.{NGP,PTR,TEX,RTX}` (5 languages) | bike shop (3D scene) | not looked at |
| `SHELL/SHBANIM.NGA`, `SHRANIM.NGA` | menu animations (riders) | not looked at |
| `MOV/*.PSS` (38) | MPEG-2 videos 640x368 25 fps: `ATTRACT` (intro, 125 s), `BIO*` (per-rider biography, 15), `TRICKS`, `CRASHES`, `CREDITS`, `TDH`, `MX`, `REVIEW`, `LOADING`, legal notices per language | **ffmpeg decodes them** (`ffmpeg -i X.PSS`); not integrated |
| `SND/SHELL_S.BNK`, `SHELL_CH.BNK` | menu sound effects and music | banks readable (`bnk.hpp`); unused in the menu |
| `VAG/NN_LIC.VPK`... | licensed tracks (menu player only) | unused |

**Material map of `SHELL/UI`** (numbering of `build/tex_dump image.iso SHELL/UI <dir>`: order of distinct (id, CBP) materials; textures come out with inverted rows and must be flipped):
- 136, 137, 138: «Downhill DOMINATION» logo (three 128x128 tiles, left to right).
- 125, 224, 233: level-name atlases, 12 + 6 + 12 lines (top to bottom): see the Spanish list above.
- 222-241 (except 224 and 233): painted illustrations of the 9 tracks, **two 256x256 halves** (left, right) = a 2:1 image: 222/223 Alps, 225/226 Utah, 227/228 Hawaii, 229/230 Peru, 231/232 Russia, 234/235 Scotland, 236/237 Japan, 238/239 USA, 240/241 British Columbia.
- 22-34 (128x128): rider portraits; 18-21: rider names with flags (4 per sheet); 53: shop-menu texts (*Bike Upgrades, Bike/Jersey, Spare Parts, Bonus Purchases, Resume, Sponsorship*); 36-41, 165: sponsors; 57-98: bikes, frames, wheels; 99-105: forks; 171-220: per-rider suit textures.

**Track → files mapping** (HYPOTHESIS by name, unverified against the code): Alps = ALPINE/ALPINEMX/ALPINET; Utah = MOAB/MOABMX/MOABTDH; Hawaii = JUNGLE/JUNGLEMX/JUNGLETD; Peru = PERU/PERUMX/PERUTDH; Russia = GLACIER/GLACIERX/GLACIERT; Scotland = AUBERST/AUBERMX/AUBERSTD; Japan = FUJI; USA = CITY/CITYMX/CITYTDH; British Columbia = BC (FR) and BCTDX (MX). The atlas order is NOT the ELF id table order (0x2760D0). Missing from the picker: Utah2 (MOAB2…), Monkeyball, Moshpit, DUAL, TRAINER.

**Pending** (in this order): rider selector with portraits 22-34 and names 18-21 (the portrait↔name↔3D-model order is a hypothesis); pause/shop menu with 53; `SHELL_*` music and effects; decompile the menu code (real layout, animations and transitions are unknown: the current one is provisional); `.PSS` videos (intro, credits) via ffmpeg or an own decoder; real-menu captures from PCSX2 to compare (the 10 `.p2s` in `~/dh-captures/ref` are all race frames).
