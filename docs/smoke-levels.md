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

**Session 2026-10-17 — three real port bugs found with the smoke test (HYPOTHESES until each has its own oracle):**
1. `Race::load` settled the bike for 300 ticks with no brake and zeroed the velocity only at the end: on steep starts (PERUTDH, PERU, BC) it rolled ~190 u down the hill before the countdown, far from the racing line. Now the translation is zeroed every tick (rotation stays free); PERUTDH 14 → 108 u/s, MOSH 15 → 39.
2. `Run::start` placed the bike with a horizontal heading. On a 46° start (BC) the first rear-wheel contact flipped it onto its tail (`fwd·n = 0.99`). Now `fwd` is projected onto the ground plane under the start (`DH_START_FLAT=1` restores the old rule). CITY with the old rule stalls at 10 u/s, with the new one reaches 120.
3. Autopilot (test only): in the air it levels the pitch to the velocity direction (`DH_AUTOLEAN0=1` off; `DH_AUTOLEAN_SLOPE=1` aims at the slope of the racing line ahead instead: CITY 0 → 3 gates but AUBERMX 13 → 1, FUJI 6 → 1, GLACIER 10 → 4, PERUMX 13 → 5, so it is not the default), otherwise it lands almost vertical (`fwd.y ≈ 1`) and stalls (BC).
Diagnostics: `DH_DEBUG=1` prints one `auto ...` line per second (position, speed, line point, `fwd`, ground normal, wheel contacts); `DH_DEBUG_GATES=1` crosses every gate plane with the PTS line; `DH_DEBUG_LINE="a n"` dumps line points. Gate data was checked in BC/PERUTDH/ALP2: planes are crossed in order along the line, so the failures are not a gate-reading bug. "1/N gates" is only the start gate.
**Final run (51 levels, 7200 ticks, default profile, 2026-10-17):** 14 of 51 fail the smoke test (16 before), all of them `SIN_PUERTAS` (0 gates): ALPINET, CITY, CITYTDH, DUAL, GLACIERT, JUNGLE, JUNGLEMX, MOABTDH and their 2-player twins (8 distinct levels; 9 before). Fixed: PERUTDH and MOSH (no longer stalled), BC 0 → 4 gates, AUBERST 0 → 1. Only the start gate crossed ("1/N"): AUBERST, AUBERSTD, BCTDX, CITYMX, PERUTDH. Reaching every gate: ALPINEMX 14/14, GLACIERX 12/12, PERUMX 13/13, MOABMX 2/2. CITY reaches 3/6 with `DH_AUTOLEAN_SLOPE=1` but that option lowers AUBERMX, FUJI, GLACIER and PERUMX, so it is not the default. With `DH_REAL=1` (engine ground forces, sprint, `DH_AUTOVMAX=170`): MOAB 16/29, FUJI 13/24, GLACIER 11/26, BC 5/9 but AUBERMX 4/24 and PERUMX 9/13.
Still open (autopilot, not port): ALPINET wedges against a rock at line point 11 at 70 u/s, JUNGLE lands sideways on a gully wall after a 113 u/s jump — both need a better AI (milestone "AI and rivals"), not physics changes.

## ES

`sh tools/smoke_levels.sh "<imagen>.iso" [ticks=3600] [nivel...]` carga cada nivel de la imagen **sin ventana** (`SDL_VIDEODRIVER=offscreen`), lo conduce con el piloto automático del modo juego (`DH_AUTOPLAY=1`: sigue la línea de carrera PTS a todo gas, `src/main.cpp`) y muestra puertas / reinicios / velocidad máxima por nivel. Falla si un nivel no da resumen, trae NaN, nunca llega a 20 u/s (`PARADA`) o no cruza ninguna puerta en un nivel que las tiene (`SIN_PUERTAS`). Un proceso a la vez (`JOBS=1`): cada `dhview` con render por software llega a varios GB y con 4–8 en paralelo el OOM-killer del kernel los mata (confirmado en `journalctl -k`). Se saltan los `POD*`: escenas de podio sin parrilla de salida ni línea (hipótesis). 7200 ticks en 51 niveles tardan ~20 min; 3600 ticks ~10 min.

**Arreglado hoy:** el rumbo de salida salía de la normal de la puerta 0, que en CITY y ALPINEMX no apunta en el sentido de la carrera: la moto arrancaba pegada a una valla (1 y 4 u/s). Ahora sale de la línea PTS junto a la parrilla (`DH_HEADING_GATE=1` restituye la regla antigua). CITY 1 → 105 u/s, ALPINEMX 4 → 89, MOABMX 1 → 82.

**Resultado con el piloto (7200 ticks, 2026-10-16):** pasan 35 de 51 y fallan 16 (9 niveles distintos, cada uno con su gemelo de 2 jugadores). Pendientes — aún sin separar «piloto débil» de «bug del port»: la tabla de arriba (ALPINET, AUBERST, BC, CITYTDH, DUAL, GLACIERT, MOABTDH, PERUTDH y MOSH).

Siguiente: en cada uno, trazar dónde se para o se sale de la línea (`DH_DEBUG`, capturas con `DH_SHOT`), decidir piloto frente a bug de colisión/salida y arreglar solo los bugs del port. Pasan hoy como referencia: `ctest`, `tools/replay_check.py --assert`, `tools/native_view_check.py --assert --iso`.

**Sesión 2026-10-17 — tres fallos reales del port hallados con la prueba de humo (HIPÓTESIS hasta que cada uno tenga su propio oráculo):**
1. `Race::load` asentaba la moto 300 ticks sin freno y ponía la velocidad a cero solo al final: en salidas con pendiente (PERUTDH, PERU, BC) rodaba ~190 u cuesta abajo antes de la cuenta atrás, lejos de la línea de carrera. Ahora se anula la traslación en cada tick (el giro queda libre); PERUTDH 14 → 108 u/s, MOSH 15 → 39.
2. `Run::start` colocaba la moto con rumbo horizontal. En una salida de 46° (BC) el primer contacto de la rueda trasera la volcaba sobre la cola (`fwd·n = 0,99`). Ahora `fwd` se proyecta sobre el plano del suelo bajo la salida (`DH_START_FLAT=1` restituye la regla antigua). CITY con la regla antigua se para a 10 u/s y con la nueva llega a 120.
3. Piloto automático (solo pruebas): en el aire nivela el cabeceo a la dirección de la velocidad (`DH_AUTOLEAN0=1` lo apaga; `DH_AUTOLEAN_SLOPE=1` apunta en cambio a la pendiente de la línea por delante: CITY 0 → 3 puertas pero AUBERMX 13 → 1, FUJI 6 → 1, GLACIER 10 → 4, PERUMX 13 → 5, por eso no es el valor por defecto); sin eso aterriza casi vertical (`fwd.y ≈ 1`) y se atasca (BC).
Diagnóstico: `DH_DEBUG=1` imprime una línea `auto ...` por segundo (posición, velocidad, punto de la línea, `fwd`, normal del suelo, contactos de rueda); `DH_DEBUG_GATES=1` cruza cada plano de puerta con la línea PTS; `DH_DEBUG_LINE="a n"` vuelca puntos de la línea. Los datos de puertas se revisaron en BC/PERUTDH/ALP2: los planos se cruzan en orden a lo largo de la línea, así que los fallos no son un error de lectura de puertas. «1/N puertas» es solo la puerta de salida.
**Pasada final (51 niveles, 7200 ticks, perfil por defecto, 2026-10-17):** fallan 14 de 51 (antes 16), todos `SIN_PUERTAS` (0 puertas): ALPINET, CITY, CITYTDH, DUAL, GLACIERT, JUNGLE, JUNGLEMX, MOABTDH y sus gemelos de 2 jugadores (8 niveles distintos; antes 9). Arreglados: PERUTDH y MOSH (ya no se paran), BC de 0 a 4 puertas, AUBERST de 0 a 1. Solo cruzan la de salida («1/N»): AUBERST, AUBERSTD, BCTDX, CITYMX, PERUTDH. Llegan a todas las puertas: ALPINEMX 14/14, GLACIERX 12/12, PERUMX 13/13, MOABMX 2/2. CITY llega a 3/6 con `DH_AUTOLEAN_SLOPE=1`, pero esa opción baja AUBERMX, FUJI, GLACIER y PERUMX, así que no es el valor por defecto. Con `DH_REAL=1` (fuerzas de suelo del motor, sprint, `DH_AUTOVMAX=170`): MOAB 16/29, FUJI 13/24, GLACIER 11/26, BC 5/9 pero AUBERMX 4/24 y PERUMX 9/13.
Sigue abierto (piloto, no port): ALPINET se encaja contra una roca en el punto 11 a 70 u/s, JUNGLE aterriza de lado en la pared de una vaguada tras un salto a 113 u/s — ambos piden una IA mejor (hito «IA y rivales»), no cambios de física.
