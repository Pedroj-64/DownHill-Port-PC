# Telemetría y fotos del juego / Game telemetry and snapshots

**ES · Para colaboradores (no hace falta saber programar).** Mientras juegas a *Downhill Domination* en PCSX2, este programa graba lo que hace el juego por dentro para que el proyecto pueda reconstruirlo. **Tú solo juegas**: él detecta el nivel y el piloto, te dice qué te falta jugar, hace las fotos de menús y pantallas de carga, comprueba que la grabación sirve y empaqueta todo.
**EN · For contributors (no programming needed).** While you play *Downhill Domination* in PCSX2 this program records what the game does internally so the project can rebuild it. **You only play**: it detects the level and rider, tells you what is still missing, snapshots menus and loading screens, checks the recording and packs everything.

## Pasos / Steps
1. **Una sola vez / Once:** *PCSX2 → Ajustes → Avanzado → Activar PINE* (y reinicia PCSX2). Necesitas tu copia del juego **PAL (SLES-52202)** y **Python 3.8+** (sin instalar paquetes). / *Settings → Advanced → Enable PINE* (restart PCSX2). You need your own **PAL (SLES-52202)** copy and **Python 3.8+** (no packages).
2. **Descarga el repo y haz doble clic en / Get the repo and double-click:** `tools/telemetry/GRABAR.bat` (Windows) o / or `tools/telemetry/grabar.sh` (Linux/macOS). Sin doble clic / without double-click: `python3 tools/telemetry/record.py`.
3. **Juega** (abre PCSX2 y el juego antes o después: el programa espera). Navega por los menús, elige un nivel, corre. Verás líneas `[OK] ...` según cubres cosas del guion y `Te falta: ...` con lo siguiente.
   / **Play** (open PCSX2 and the game before or after: it waits). Browse the menus, pick a level, race. You will see `[OK] ...` lines as you cover the script and `Still missing: ...`.
4. **Cuando termines pulsa Ctrl+C.** El programa guarda, comprueba y dice al final **`>>> ENVÍA ESTE ARCHIVO: ...`**. Mándalo solo a quien te lo pidió, por un canal privado. / **When done press Ctrl+C.** It saves, checks and ends with **`>>> SEND THIS FILE: ...`**. Send it only to whoever asked, over a private channel.

> **ES:** **no** lee tu teclado/mando del sistema, **no** captura tu pantalla y **no** mira nada de tu equipo: solo memoria del juego por PINE. Todo queda en `~/dh-telemetry/` (Windows: `C:\Users\TU_USUARIO\dh-telemetry\`), nunca dentro del repo.
> **EN:** it does **not** read your keyboard/gamepad, **not** capture your screen, nothing on your computer: only game memory through PINE. Everything stays in `~/dh-telemetry/`, never inside the repo.
> ⚠ **NO lo subas a GitHub ni a sitios públicos / do NOT upload it to GitHub or anywhere public**: contiene memoria del juego (material con copyright). / it contains game memory (copyrighted).

## Qué hace solo / What it does by itself
| ES | EN |
|---|---|
| Espera a PCSX2 y al juego si aún no están abiertos y te dice qué activar | Waits for PCSX2 and the game and tells you what to enable |
| Detecta **nivel y piloto** leyendo el juego (no escribes nada; `--level/--rider` son opcionales) | Detects **level and rider** from the game (you type nothing; `--level/--rider` optional) |
| Mide tu PC y elige cuántos pilotos grabar (modo ligero si va justo) | Measures your PC and picks how many riders to record (light mode if tight) |
| **Menús → carrera → menús…** en continuo: cada carrera y cada tramo de menús es una grabación; fotos en cada cambio de nivel/carga y cada 15 s en menús | **Menus → race → menus…** continuously: each race and menu stretch is a recording; snapshots at each level change/loading and every 15 s in menus |
| Guion en vivo: marca `[OK]` giros, frenadas, saltos cortos y cargados, vuelo largo, terrenos, choque, 3 min | Live script: ticks turns, braking, short/charged jumps, long flight, terrains, crash, 3 min |
| Al final comprueba la grabación, te sugiere el nivel/piloto que falta y empaqueta (un solo `.zip` si hay varias) | At the end checks the recording, suggests the missing level/rider and packs (a single `.zip` if several) |
| Respalda y devuelve tu savestate de la ranura 10; avisa si queda poco disco | Backs up and restores your slot-10 savestate; warns on low disk |

**Opcional / Optional:** escribe `snap nombre` + Enter para una foto completa en ese instante (se queda unos segundos parado); o una nota (`derrape fuerte`) + Enter. / type `snap name` + Enter for a full snapshot now (freezes a few seconds); or a note + Enter.
**Variedad que más ayuda / Variety that helps most:** otros niveles y pilotos, otros modos (contrarreloj, freeride, slalom, combate…), pausar en carrera y ver resultados, la pantalla de carga de cada nivel. El programa te sugiere el siguiente al acabar. / other levels and riders, other modes, pausing in a race and the results, each level's loading screen; the program suggests the next one.

## Opciones (casi nunca hacen falta) / Options (rarely needed)
`--once` una carrera y salir · `--tour` solo menús · `--light` PC lento · `--riders 1` solo tú · `--no-states` sin savestates · `--no-coach` sin guion · `--out RUTA` otra carpeta · `--sstates RUTA` carpeta de savestates de PCSX2 · `--tcp 127.0.0.1:28011` / `--socket RUTA` si PINE no se encuentra · `--label texto`. `python3 tools/telemetry/check.py "<carpeta o .zip>"` revisa una grabación; `bundle.py` junta los `.zip`.

## Problemas frecuentes / Troubleshooting
| Síntoma / Symptom | ES | EN |
|---|---|---|
| «Esperando a PCSX2» | abre PCSX2, activa PINE y reinicia PCSX2; en Windows prueba `--tcp 127.0.0.1:28011` | open PCSX2, enable PINE and restart it; on Windows try `--tcp 127.0.0.1:28011` |
| «El juego cargado es …» | solo el PAL SLES-52202 | only the PAL SLES-52202 |
| No encuentra savestates | `--sstates RUTA` (carpeta `sstates` de PCSX2) o `--no-states` | `--sstates PATH` or `--no-states` |
| `REVISAR` al final | graba otra vez, más larga y con más movimiento | record again, longer and with more movement |
| Se cerró PCSX2 / se cortó | se guarda lo grabado; envía igualmente el archivo | what was recorded is saved; send it anyway |

## Para el proyecto / For the project
- Formato: `schema.py` (DHTEL1, direcciones de SLES-52202). Detección y guion: `detect.py` (direcciones del nivel, lista de pilotos y tabla de niveles: hipótesis, ver `docs/p2s-savestates.md`). Pruebas: `python3 -m unittest tests.test_telemetry tests.test_telemetry_auto`.
- `meta.json` guarda `detected` (nivel, piloto, lista completa, coherencia), `coverage` (guion) y `riders_in_race` (separa modos: 10 pilotos vs 6). La variante (ALP2 vs ALPINE) **no** está en RAM del nivel: se resuelve al analizar con las puertas de `race_mgr`.
- Lo recibido se guarda **fuera del repo** (p. ej. `~/dh-states/telemetry/`); solo se versionan resúmenes derivados, nunca volcados, savestates ni `.dhtel`. / Received data stays **outside the repo**; only derived summaries are versioned.
