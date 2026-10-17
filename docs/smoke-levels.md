# Movement smoke test over all levels / Prueba de humo de movimiento en todos los niveles

## EN

`sh tools/smoke_levels.sh "<image>.iso" [ticks=3600] [level...]` loads every level from the disc **without a window** (`SDL_VIDEODRIVER=offscreen`), drives it with the play-mode autopilot (`DH_AUTOPLAY=1`: follows the PTS racing line at full throttle, `src/main.cpp`) and prints gates / respawns / top speed per level. It fails if a level gives no summary, has NaN, never reaches 20 u/s (`PARADA`) or crosses no gate in a level that has gates (`SIN_PUERTAS`). One process at a time (`JOBS=1`): each `dhview` with software rendering reaches several GB and with 4–8 in parallel the kernel OOM-killer ends them (confirmed in `journalctl -k`). `POD*` are skipped: podium scenes without a start grid or racing line (hypothesis). 7200 ticks over 51 levels take ~20 min; 3600 ticks ~10 min.

**Fixed today:** the start heading came from the normal of gate 0, which in CITY and ALPINEMX does not point along the race: the bike started against a fence (1 and 4 u/s). It now comes from the PTS line next to the grid (`DH_HEADING_GATE=1` restores the old rule). CITY 1 → 105 u/s, ALPINEMX 4 → 89, MOABMX 1 → 82.

**Result with the autopilot (7200 ticks, 2026-10-16):** 35 of 51 pass, 16 fail (9 distinct levels, each with its 2-player twin). Open items — not yet separated into "weak autopilot" vs "port bug":

| Level | Gates | Top speed | Note |
|---|---|---|---|
| ALPINET | 0/3 | 82 | circuit finishes early or the line is short |
| AUBERST | 0/9 | 137 | fast but crosses no gate |
| BC | 0/9 | 42 | slow: check start/collision |
| CITYTDH | 0/4 | 65 | |
| DUAL | 0/6 | 123 | 4 respawns |
| GLACIERT | 0/4 | 123 | 1 respawn |
| MOABTDH | 0/4 | 123 | 3 respawns |
| PERUTDH | 1/6 | 14 | stuck (`PARADA`) |
| MOSH | 0/0 | 15 | trick arena, bike starts boxed in between panels (not a race) |

Next: for each, trace where the bike stops or leaves the line (`DH_DEBUG`, screenshots with `DH_SHOT`), decide autopilot vs. collision/start bug, fix only port bugs. Reference passes: `ctest`, `tools/replay_check.py --assert`, `tools/native_view_check.py --assert --iso`.

## ES

`sh tools/smoke_levels.sh "<imagen>.iso" [ticks=3600] [nivel...]` carga cada nivel de la imagen **sin ventana** (`SDL_VIDEODRIVER=offscreen`), lo conduce con el piloto automático del modo juego (`DH_AUTOPLAY=1`: sigue la línea de carrera PTS a todo gas, `src/main.cpp`) y muestra puertas / reinicios / velocidad máxima por nivel. Falla si un nivel no da resumen, trae NaN, nunca llega a 20 u/s (`PARADA`) o no cruza ninguna puerta en un nivel que las tiene (`SIN_PUERTAS`). Un proceso a la vez (`JOBS=1`): cada `dhview` con render por software llega a varios GB y con 4–8 en paralelo el OOM-killer del kernel los mata (confirmado en `journalctl -k`). Se saltan los `POD*`: escenas de podio sin parrilla de salida ni línea (hipótesis). 7200 ticks en 51 niveles tardan ~20 min; 3600 ticks ~10 min.

**Arreglado hoy:** el rumbo de salida salía de la normal de la puerta 0, que en CITY y ALPINEMX no apunta en el sentido de la carrera: la moto arrancaba pegada a una valla (1 y 4 u/s). Ahora sale de la línea PTS junto a la parrilla (`DH_HEADING_GATE=1` restituye la regla antigua). CITY 1 → 105 u/s, ALPINEMX 4 → 89, MOABMX 1 → 82.

**Resultado con el piloto (7200 ticks, 2026-10-16):** pasan 35 de 51 y fallan 16 (9 niveles distintos, cada uno con su gemelo de 2 jugadores). Pendientes — aún sin separar «piloto débil» de «bug del port»: la tabla de arriba (ALPINET, AUBERST, BC, CITYTDH, DUAL, GLACIERT, MOABTDH, PERUTDH y MOSH).

Siguiente: en cada uno, trazar dónde se para o se sale de la línea (`DH_DEBUG`, capturas con `DH_SHOT`), decidir piloto frente a bug de colisión/salida y arreglar solo los bugs del port. Pasan hoy como referencia: `ctest`, `tools/replay_check.py --assert`, `tools/native_view_check.py --assert --iso`.
