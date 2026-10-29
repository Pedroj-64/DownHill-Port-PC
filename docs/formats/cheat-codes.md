<!-- SPDX-FileCopyrightText: 2026 Pedro Soto -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Cheat codes: table in the executable / Códigos de trucos: tabla en el ejecutable

Evidence / Evidencia: byte search in the retail executable of the PAL disc (`SLES_522.02`, read from the player's own image; nothing copied to the repo). Cross-checked against the community wiki's cheat list, which is only a pointer.

## EN
**Where.** The executable holds a table of **26 codes** at file offset `0x173808` (RAM `0x27C808`; RAM = file offset + `0x109000` for the single loadable segment). Each entry is 36 bytes: a `u32` code id (1..26) followed by up to 8 `u32` button masks, ended by a zero. The masks are the raw pad bits: Up `0x10`, Right `0x20`, Down `0x40`, Left `0x80`, Triangle `0x1000`, Circle `0x2000`, X `0x4000`, Square `0x8000`.

**Names.** After the table come pointer tables of display names, one per language (English, French and Italian were checked; other languages were not looked at), 25 names each in code order starting at id 2 (id 1 is the unlock code itself, labelled "Codes"/"Unlock Code" elsewhere). Names are read from the image, not copied here beyond what is needed to label the codes.

| id | Sequence | Name (EN) | In wiki |
|---|---|---|---|
| 1 | U T D X L C R S | Unlock Code | yes |
| 2 | U X L L C C | Infinite Bottles | yes |
| 3 | U D L L R | Combat Upgrade | yes |
| 4 | U X L S U | Super Bunny Hop | yes |
| 5 | D T R R S | Speed Freak | yes |
| 6 | D T S S U | Anti Gravity | yes |
| 7 | L R T U | Super Smack | **no** |
| 8 | L R D D | Extra Smack Time | yes |
| 9 | L S X U T | Super Bounce | yes |
| 10 | R L L U T | Pro Rider Bios | **no** |
| 11 | R L L U X | Fantasy Rider Bios | **no** |
| 12 | R L L D | DH Video Clips | **no** |
| 13 | R L L D D S C | All Videos | **no** |
| 14 | R T T L | Cash ($2,000 per the wiki) | yes |
| 15 | R U U C C S | More Cash | yes |
| 16 | R U U R R S | Mega Flip | yes |
| 17 | D L L R | Adrenaline Boost | yes |
| 18 | D L L R R | Stoke Trick Meter | yes |
| 19 | D S S L C | Always Stoked | yes |
| 20 | D R R L L | Energy Restore | yes |
| 21 | R L R L R L | Freeze Trick | **no** |
| 22 | U D L L R R | Upgrade To Bottle | yes |
| 23 | U D U D U D | You Win | **no** |
| 24 | D U U D D U U | Unlock Everything | yes |
| 25 | U D X T U X D | Buy Everything | **no** |
| 26 | L S C S L | Combat Free | yes |

Letters: U/D/L/R = d-pad, T = Triangle, C = Circle, X = Cross, S = Square. All 18 codes in the wiki match the table exactly; **8 codes are in the executable but not in the wiki** (ids 7, 10, 11, 12, 13, 21, 23, 25). What those eight do is read from their names only; their effect was not exercised in the emulator.

**Not done.** The input handler that compares presses to this table and the effect of each code were not traced. The port has no cheats.

## ES
**Dónde.** El ejecutable guarda una tabla de **26 códigos** en el desplazamiento de archivo `0x173808` (RAM `0x27C808`; RAM = desplazamiento + `0x109000` para el único segmento cargable). Cada entrada ocupa 36 bytes: un `u32` con el id (1..26) y hasta 8 `u32` con máscaras de botón, terminadas en cero. Las máscaras son los bits crudos del mando: Arriba `0x10`, Derecha `0x20`, Abajo `0x40`, Izquierda `0x80`, Triángulo `0x1000`, Círculo `0x2000`, X `0x4000`, Cuadrado `0x8000`.

**Nombres.** Tras la tabla vienen tablas de punteros con los nombres que se muestran, una por idioma (se comprobaron inglés, francés e italiano; no se miraron otros idiomas), 25 nombres cada una en el orden de los códigos desde el id 2 (el id 1 es el propio código de desbloqueo). Los nombres se leen de la imagen; aquí solo se usan para etiquetar los códigos.

La tabla de arriba lista las secuencias. Las letras son: U/D/L/R = cruceta, T = Triángulo, C = Círculo, X = Cruz, S = Cuadrado. Los 18 códigos de la wiki coinciden exactamente con la tabla; **8 códigos están en el ejecutable y no en la wiki** (ids 7, 10, 11, 12, 13, 21, 23, 25). Lo que hacen esos ocho se deduce solo de su nombre; no se probaron en el emulador.

**Sin hacer.** No se rastreó el manejador de entrada que compara las pulsaciones con esta tabla, ni el efecto de cada código. El port no tiene trucos.
