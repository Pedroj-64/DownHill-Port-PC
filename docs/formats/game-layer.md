# Capa de juego: inventario de la fase 2 / Game layer: phase 2 inventory
Estado 2026-10-06. Solo nombres de archivo, tamaños, direcciones del ELF y conteos; ningún dato del juego. Las librerías de Sony (libpad, libsd, libmc, libcdvd, `MOD/*.IRX`) se sustituyen por SDL3 y no se revierten. / Status 2026-10-06. File names, sizes, ELF addresses and counts only; no game data. Sony libraries (libpad, libsd, libmc, libcdvd, `MOD/*.IRX`) are replaced by SDL3 and are not reverse-engineered.

## ES — Tabla por área
Esfuerzo: S ≈ horas, M ≈ 1-3 días, L ≈ semana o más. Todo lo no marcado «verificado» es **hipótesis**.

| Área | Archivos (ISO) | Qué está decodificado | Funciones del ELF | Esfuerzo | Orden |
|---|---|---|---|---|---|
| Cronómetro, meta, resultados | `PTS/*` (28 puertas en ALP2), `RST/*.RST` (60 × 16 384 B) | puertas y gestor de salida (`gates.hpp`); meta = última puerta (hipótesis). Resultados: 7 modos por cadena: carrera, freestyle, slalom dual, freeride, contrarreloj, combate, super salto | tabla de textos (datos, no función) en `0x277160..0x27769c` (`RACE RESULTS` `0x2771CC`, `TIME TRIAL RESULTS` `0x2774DC`, `GREAT FINISH!` `0x277614`, `End of Recorded Replay` `0x27769C`); código que la usa: sin localizar | S cronómetro y meta / M resultados | 1 |
| HUD | texturas por nivel/`SHELL/UI*` | nada | sin localizar | M | 2 |
| Menús y pausa | `SHELL/` (42 archivos: `UI*` anillo circular en 5 idiomas, `BIKESHOP*`), `LOADBAR/` (78, ya a PNG) | LOADBAR renderiza; SHELL carga como NGP | `PLAYER n PAUSED` en la tabla `0x277160`; **candidatas** (referencian la zona de la tabla de textos, sin leer): `FUN_00191E48` (ref. `0x277110`), `FUN_0018F698` (`0x2770F0`), `FUN_0018FB78` (`0x277060`) — pausa, resultados o HUD, hipótesis | S assets / L lógica | 3 y 5 |
| Audio (efectos) | `SND/*.BNK` (50: `<nivel>_S`, `RIDE_xx`, `SHELL_S`, `PAUSE_S`, `PODIUM_S`, `PCKP_S`, `SMUSIC01-04`, ≈ 1,4 MB c/u) | nada | `FUN_001CAD90` (carga `Pause_S.bnk`, 716 B): familia de cargadores de bancos | M | 4 |
| Audio (música y voces) | `VAG/` (128: `NN_LIC.VPK` ×20 ≈ 10 MB, `CMX/CTDH.VPK`, pares `*-L/R.VAG`), `SKAT/DHSKAT.SKX` (101 MB `SKEX`+VAGp) con índice `DHSKAT.CTL` (0x44 B por entrada) | cabeceras VAGp (research.md); PS2 ADPCM estándar | `FUN_001C0110` (abre `\SKAT\DHSKAT.SKX;1`, 496 B) | M | 4 |
| Vídeo | `MOV/` (38 `.PSS`: copyright, bios de pilotos, attract, créditos, tricks) | nada | lista de nombres `.pss` en el ELF (datos) | L (PSS = MPEG-2 de PS2) | diferido |
| Entrada | sin archivo; pantalla `SELECT CONTROL OPTIONS` en el ELF | nada | cadena en `0x2B5EA8`, en la tabla `0x2c7710` que lee `FUN_001FC7B8` (pantalla de opciones de mando; hecho: referencia de datos; su función exacta, hipótesis) | S (acciones por defecto propias) | 5 |
| Guardado | `MEMCRD/` solo iconos (`LISTICON`, `POSTER`, `REPLAY`) | nada | sin localizar | S (formato propio) | 5 |
| Repeticiones | `REP/`: 6 `.REP` (2-25 KB) y 28 `.ORB` de 11 256 B | cabeceras (abajo) | cadenas de repetición en el ELF (`Replay Data Not Found`, `Controller Removed, Replay Stopped`) | M | tras 1 |
| Rivales/IA y modos | línea `PTS/`, `BIKE/` | línea PTS | sin localizar | L | diferido |

Hallazgos de cadenas (verificables con `strings -a -t x`): hasta 4 jugadores (`PLAYER 1..4 PAUSED`), 5 idiomas (EN/FR/DE/IT/ES), editor de pósters en la pantalla de pausa, vídeo visible desde Opciones.

### .REP y .ORB como oráculo de ruta y tiempo: DESCARTADO por ahora
Evidencia (solo 256 B de `ALPINE.ORB` y `ALPINE.REP`):
- `.ORB`: cabecera `54 30 00 00 | 20 00 | 0e 00 | 06 06 03 00 | <nombre ASCII de piloto>` y 11 256 = 60 + 2 799 × 4. El flujo posterior son grupos de 4 bytes; el cuarto byte solo toma los valores 0, 3 y 7 (máscara de 3 bits) y el tercero es un paseo irregular de 8 bits (`fe fc f5 ef f1 ee e7 e1 …`), sin la monotonía de una coordenada a lo largo de una bajada. Es la firma de un registro de **pulsaciones de mando** más que de posiciones. Medido en 3 niveles (`ALPINE`, `AUBERMX`, `FUJI`): el u32 de 0x18 vale 1586 / 381 / 1688 (el de 0x14 es 0); el flujo útil acaba en la muestra 2799 / 1399 / 2798 de 2799 (el resto, ceros); canales 1-3 suaves (autocorrelación 0,88-0,98, valores 0-255 con saltos) y 4.º byte en {0, 1…7, 64…70, 127, 128, 190…208}. Uso previsto: demos o fantasmas, no oráculo.
- `.REP`: cabecera dispersa (`u32 0x4039`, `0xF615`, `0x11`, `0x04`, resto ceros).
- El ELF habla de «Controller Removed, Replay Stopped» y «End of Recorded Replay»: las repeticiones son grabaciones de entrada (hipótesis).
- Consecuencia: serviría de oráculo solo con una física fiel al motor, que no tenemos (hito 3b). No hay posiciones ni marcas de tiempo evidentes. Para cerrarlo hace falta decodificar el archivo completo (`tools/peek.sh` aún no autorizado) y comparar con la línea de ALP2.

### Cómo reproducir
`strings -a -t x SLES_522.02 | grep -iE 'results|pause|replay|options|\.skx|\.bnk'`; `tools/ghidra/run.sh StrXrefs.java '<regex>' salida.txt` (una apertura del proyecto, solo xrefs); `ls -l` de `REP SND VAG SKAT MOV MEMCRD RST SHELL` sobre la ISO extraída.

## ES — Rebanada vertical en ALP2 (APROBADA 2026-10-06)
Alcance mínimo: contrarreloj en ALP2, una moto, sin rivales, teclado con SDL3 y asignación por defecto, sin pantalla de opciones. **Diferido** (documentado, no se hará ahora): los otros 6 modos, multijugador, idiomas y editor de pósters.
**Fase 1 hecha** (`src/race.hpp`, `tests/race_test.cpp`): estados carga → cuenta atrás (3 s, **hipótesis**: no sale del ELF) → conducción → meta (aviso de 2 s, hipótesis) → resultados; reloj a cero al «¡Ya!»; meta = última puerta del recorrido (hipótesis); resultados con tiempo, parciales, reinicios y velocidad máxima. **La puerta 0 es la línea de salida, no un parcial:** se arma tras el «¡Ya!» y exige cruce de detrás hacia delante; si la moto ya está delante del plano al arrancar se da por cruzada en t = 0. ALP2: 28 puertas = salida + 27 parciales. Evidencia: `ctest` 9/9 con terreno y puertas sintéticos; `bike_demo` en ALP2: **07:10.98, 28/28 puertas, 0 reinicios, 122 km/h máx**, línea de salida a 00:03.21 y 27 parciales (el primero a 00:16.00); el visor (`dhview`, modo jugable) muestra estado, cuenta atrás y reloj en el título y la captura de la parrilla en la cuenta atrás. Fragilidad del arnés (no se añaden ayudas al piloto de pruebas): el salto del barranco del punto 125 de la línea sale con ~8 u de margen y depende del estado inicial; `bike_demo` usa `Race::settle = false` SOLO como prueba de regresión (repite la trayectoria validada); con la moto asentada el piloto se atasca ahí. La prueba real es jugar a mano.

### Orden propuesto (aprobado)
1. Cronómetro, meta y pantalla de resultados (tiempo en pantalla con la puerta final ya existente).
2. HUD mínimo (velocidad, tiempo, puertas).
3. Menú mínimo con assets reales de SHELL (anillo UI, LOADBAR).
4. Audio: decodificador VAG/VPK a PCM y música + efectos clave (carga, rodadura, impacto).
5. Pausa y opciones (mapeo de entrada propio).
Riesgo principal: la lógica de menús y resultados del ELF no está localizada; se parte de un formato propio y se usan las cadenas solo como textos.

## EN — Per-area table
Effort: S ≈ hours, M ≈ 1-3 days, L ≈ a week or more. Anything not marked "verified" is a **hypothesis**.

| Area | Files (ISO) | Decoded | ELF functions | Effort | Order |
|---|---|---|---|---|---|
| Timer, finish, results | `PTS/*` (28 gates in ALP2), `RST/*.RST` (60 × 16,384 B) | gates and start manager (`gates.hpp`); finish = last gate (hypothesis). Results: 7 modes from strings (race, freestyle, dual slalom, freeride, time trial, combat, super jump) | text table (data, not code) at `0x277160..0x27769c` (`RACE RESULTS` `0x2771CC`, `TIME TRIAL RESULTS` `0x2774DC`, `GREAT FINISH!` `0x277614`, `End of Recorded Replay` `0x27769C`); the code that uses it: not located | S timer+finish / M results | 1 |
| HUD | per-level textures / `SHELL/UI*` | nothing | not located | M | 2 |
| Menus and pause | `SHELL/` (42 files: `UI*` circular ring in 5 languages, `BIKESHOP*`), `LOADBAR/` (78, already exported to PNG) | LOADBAR renders; SHELL loads as NGP | `PLAYER n PAUSED` in the `0x277160` table; **candidates** (they reference the text-table area, unread): `FUN_00191E48` (ref `0x277110`), `FUN_0018F698` (`0x2770F0`), `FUN_0018FB78` (`0x277060`) — pause, results or HUD, hypothesis | S assets / L logic | 3 and 5 |
| Audio (effects) | `SND/*.BNK` (50: `<level>_S`, `RIDE_xx`, `SHELL_S`, `PAUSE_S`, `PODIUM_S`, `PCKP_S`, `SMUSIC01-04`, ≈ 1.4 MB each) | nothing | `FUN_001CAD90` (loads `Pause_S.bnk`, 716 B): bank-loader family | M | 4 |
| Audio (music, voices) | `VAG/` (128: `NN_LIC.VPK` ×20 ≈ 10 MB, `CMX/CTDH.VPK`, `*-L/R.VAG` pairs), `SKAT/DHSKAT.SKX` (101 MB `SKEX`+VAGp) with index `DHSKAT.CTL` (0x44 B per entry) | VAGp headers (research.md); standard PS2 ADPCM | `FUN_001C0110` (opens `\SKAT\DHSKAT.SKX;1`, 496 B) | M | 4 |
| Video | `MOV/` (38 `.PSS`: copyright, rider bios, attract, credits, tricks) | nothing | `.pss` name list in the ELF (data) | L (PSS = PS2 MPEG-2) | deferred |
| Input | no file; `SELECT CONTROL OPTIONS` screen in the ELF | nothing | string at `0x2B5EA8`, in the `0x2c7710` table read by `FUN_001FC7B8` (control-options screen; fact: a data reference; its exact role, hypothesis) | S (own default actions) | 5 |
| Save data | `MEMCRD/` icons only (`LISTICON`, `POSTER`, `REPLAY`) | nothing | not located | S (own format) | 5 |
| Replays | `REP/`: 6 `.REP` (2-25 KB) and 28 `.ORB` of 11,256 B | headers (below) | replay strings in the ELF (`Replay Data Not Found`, `Controller Removed, Replay Stopped`) | M | after 1 |
| Rivals/AI and modes | `PTS/` line, `BIKE/` | PTS line | not located | L | deferred |

String findings (reproducible with `strings -a -t x`): up to 4 players (`PLAYER 1..4 PAUSED`), 5 languages (EN/FR/DE/IT/ES), poster maker in the pause screen, video viewable from Options.

### .REP and .ORB as route and time oracle: DISCARDED for now
Evidence (only 256 B of `ALPINE.ORB` and `ALPINE.REP`):
- `.ORB`: header `54 30 00 00 | 20 00 | 0e 00 | 06 06 03 00 | <ASCII rider name>` and 11,256 = 60 + 2,799 × 4. The stream is 4-byte groups; the 4th byte only takes 0, 3 and 7 (a 3-bit mask) and the 3rd is an irregular 8-bit walk (`fe fc f5 ef f1 ee e7 e1 …`), without the monotonicity of a coordinate along a descent. This is the signature of recorded **controller presses** rather than positions. Measured on 3 levels (`ALPINE`, `AUBERMX`, `FUJI`): the u32 at 0x18 is 1586 / 381 / 1688 (0x14 is 0); the useful stream ends at sample 2799 / 1399 / 2798 of 2799 (rest zeros); channels 1-3 smooth (autocorrelation 0.88-0.98, values 0-255 with jumps), 4th byte in {0, 1…7, 64…70, 127, 128, 190…208}. Intended use: demos or ghosts, not an oracle.
- `.REP`: sparse header (`u32 0x4039`, `0xF615`, `0x11`, `0x04`, rest zeros).
- The ELF says "Controller Removed, Replay Stopped" and "End of Recorded Replay": replays are input recordings (hypothesis).
- Consequence: usable as an oracle only with a physics faithful to the engine, which we do not have (milestone 3b). No evident positions or timestamps. Closing this needs the full file decoded (`tools/peek.sh` not yet authorised) and a comparison with the ALP2 line.

### How to reproduce
`strings -a -t x SLES_522.02 | grep -iE 'results|pause|replay|options|\.skx|\.bnk'`; `tools/ghidra/run.sh StrXrefs.java '<regex>' out.txt` (one project open, xrefs only); `ls -l` of `REP SND VAG SKAT MOV MEMCRD RST SHELL` on the extracted ISO.

## EN — Vertical slice in ALP2 (APPROVED 2026-10-06)
Minimum scope: time trial in ALP2, one bike, no rivals, SDL3 keyboard with default mapping, no options screen. **Deferred** (documented, not done now): the other 6 modes, multiplayer, languages and poster editor.
**Phase 1 done** (`src/race.hpp`, `tests/race_test.cpp`): states loading → countdown (3 s, **hypothesis**: not from the ELF) → riding → finish (2 s banner, hypothesis) → results; clock at zero on GO; finish = last course gate (hypothesis); results with time, splits, resets and top speed. **Gate 0 is the start line, not a split:** it is armed after GO and needs a behind-to-front crossing; if the bike is already in front of the plane at the start it counts as crossed at t = 0. ALP2: 28 gates = start + 27 splits. Evidence: `ctest` 9/9 with synthetic ground and gates; `bike_demo` on ALP2: **07:10.98, 28/28 gates, 0 resets, 122 km/h max**, start line at 00:03.21 and 27 splits (first at 00:16.00); the viewer (`dhview` play mode) shows state, countdown and clock in the title, plus a grid capture during the countdown. Harness fragility (no aids are added to the test pilot): the canyon jump at line point 125 clears by ~8 u and depends on the initial state; `bike_demo` uses `Race::settle = false` ONLY as a regression test (it repeats the validated trajectory); with the bike settled the pilot gets stuck there. The real test is playing by hand.

### Proposed order (approved)
1. Timer, finish and results screen (on-screen time with the existing final gate).
2. Minimal HUD (speed, time, gates).
3. Minimal menu with real SHELL assets (UI ring, LOADBAR).
4. Audio: VAG/VPK-to-PCM decoder, music + key effects (loading, rolling, impact).
5. Pause and options (own input mapping).
Main risk: the ELF's menu and results logic is not located; start from an own format and use the strings only as texts.
