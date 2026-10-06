# Telemetría y fotos del juego / Game telemetry and snapshots

**ES · Para colaboradores.** Este script graba, mientras juegas a *Downhill Domination* en PCSX2, lo que hace el juego por dentro (entradas, física, pilotos, objetos, menús) para que el proyecto pueda reconstruirlo. No necesitas saber programar: son 4 pasos.
**EN · For contributors.** While you play *Downhill Domination* in PCSX2, this script records what the game does internally (inputs, physics, riders, objects, menus) so the project can rebuild it. No programming needed: 4 steps.

---

## 1. Qué necesitas / What you need
- **ES:** tu propia copia del juego **PAL (SLES-52202)** funcionando en **PCSX2**, y **Python 3.8+** (no hay que instalar paquetes). Unos 2 GB libres en disco por sesión larga.
- **EN:** your own copy of the **PAL (SLES-52202)** game running in **PCSX2**, and **Python 3.8+** (no packages to install). About 2 GB free disk per long session.
- **ES:** descarga el repo (`git clone https://github.com/Pedroj-64/DownHill-Port-PC.git` o el .zip del repo) y abre una terminal en esa carpeta. **EN:** get the repo (clone or zip) and open a terminal in that folder.

## 2. Una sola vez: activar PINE en PCSX2 / Once: enable PINE in PCSX2
- **ES:** en PCSX2: *Ajustes → Avanzado → Activar PINE* (puerto/ranura por defecto 28011). Reinicia PCSX2.
- **EN:** in PCSX2: *Settings → Advanced → Enable PINE* (default slot/port 28011). Restart PCSX2.
- Linux (nativo o Flatpak) y macOS: el script busca solo el socket. **Windows:** usa TCP automáticamente (127.0.0.1:28011). Si falla: `--socket RUTA` o `--tcp 127.0.0.1:28011`.

## 3. Cada sesión / Each session
1. Abre PCSX2 con el juego (PAL). / Open PCSX2 with the game (PAL).
2. En la terminal, en la carpeta del repo / In the terminal, in the repo folder:
   - **Carreras / Races:** `python3 tools/telemetry/record.py --level ALP2 --rider Cosmo`
     (cambia `ALP2` por el nivel y `Cosmo` por tu piloto; si no sabes el nombre, escribe lo que veas). El script espera a que empieces la carrera.
   - **Menús y pantallas (sin carrera) / Menus and screens (no race):** `python3 tools/telemetry/record.py --tour --label menus`
3. **Juega con normalidad.** Mientras grabas puedes **escribir una nota y pulsar Enter** (p. ej. `derrape fuerte`, `salto cargado`, `estoy sobre hierba`): queda guardada con su hora, y ayuda mucho.
   Escribe `snap nombre` + Enter (p. ej. `snap menu_principal`) para sacar una **foto completa** del juego en ese instante (se queda unos segundos parado: es normal).
4. Termina con **Ctrl+C**. El script comprueba y empaqueta todo y te dice al final: **`>>> ENVÍA: <carpeta SEND>`**.

> **ES:** el script **no** lee tu teclado/mando del sistema, **no** captura tu pantalla y **no** mira nada de tu equipo: solo lee memoria del juego a través de PINE.
> **EN:** the script does **not** read your keyboard/gamepad, **does not** capture your screen and does not look at anything on your computer: it only reads game memory through PINE.

## 4. Qué jugar (guion) / What to play (script)
**ES — queremos variedad: distintos mapas, distintos pilotos/bicis, distintos modos.** Para cada combinación nivel + piloto, una sesión de 3–5 minutos con esto (en cualquier orden):
1. Salida: deja la cuenta atrás completa; luego **20 s en línea recta pedaleando** a tope.
2. **Giros:** izquierda y derecha a fondo, suaves y bruscos, a poca y a mucha velocidad; zigzag.
3. **Freno:** frenadas cortas y largas; parar del todo y arrancar.
4. **Saltos:** pulsación corta y **carga larga** del salto; rampas grandes y pequeñas; aterrizajes limpios y malos; cabeceo en el aire (inclinar adelante/atrás).
5. **Superficies:** tierra, hierba, roca, nieve, madera/puentes, barro (lo que haya).
6. **Caídas y choques:** estrellarse contra un árbol/roca, caer por un barranco, volcar; reaparecer.
7. Acabar la carrera hasta la meta y **ver la pantalla de resultados** (escribe `snap resultados` ahí).
8. Si hay otros modos (contrarreloj, freeride, truco, slalom, replays), **un rato en cada uno** con `--label` que lo diga.
**Recorrido (`--tour`)**, una sola sesión: menú principal, opciones, taller de bicis, selección de nivel, selección de piloto, pantallas de carga, cuenta atrás, pausa, resultados, repetición, créditos. En cada pantalla escribe `snap <nombre>` + Enter. Se gestiona bien con teclado y mando de PCSX2.
**EN — we want variety: different maps, riders/bikes and modes.** For each level + rider, a 3–5 minute session with the same list: straight-line pedalling, turns, braking, short and **long-charged** jumps, surfaces, crashes, finishing and the results screen (`snap results`), other modes with `--label`, and one `--tour` session through all menus and screens using `snap <name>`.

## 5. Cómo enviarlo / How to send it
- **ES:** todo queda en `~/dh-telemetry/` (en Windows `C:\Users\TU_USUARIO\dh-telemetry\`). **Envía la carpeta `SEND`** (contiene un `.zip` por grabación) — o ejecuta `python3 tools/telemetry/bundle.py` para juntar todo en **un solo `.zip`** (`dhtel_bundle_…zip`) y envía ese. Mándalo **solo a quien te lo pidió**, por un canal privado.
- **EN:** everything is in `~/dh-telemetry/`. **Send the `SEND` folder** (one `.zip` per recording), or run `python3 tools/telemetry/bundle.py` to merge everything into **one `.zip`** and send that one — **only to whoever asked**, over a private channel.
- ⚠ **ES: NO lo subas a GitHub ni a ningún sitio público.** Contiene memoria del juego (material con copyright). El script se niega a escribir dentro de un repositorio git. **EN: do NOT upload it to GitHub or anywhere public.** It contains game memory (copyrighted material). The script refuses to write inside a git repository.
- Para comprobar que tu grabación sirve / To check your recording is usable: `python3 tools/telemetry/check.py "~/dh-telemetry/sessions/<carpeta>"` → debe acabar en **RESULTADO: OK**.

## 6. Qué contiene cada sesión / What each session contains
`sessions/dhtel_<fecha>_<nivel>_<piloto>/`
| Archivo | ES | EN |
|---|---|---|
| `ticks.dhtel.gz` | cada paso de física (~50/s): entradas, estado de tu moto y de los demás pilotos, nodos; cada ~0,5 s los pilotos y el gestor de carrera **enteros** | every physics step (~50/s): inputs, state of your bike and the other riders, nodes; every ~0.5 s the riders and race manager **in full** |
| `static.dhtel`, `static_end.dhtel` | tablas y constantes del juego y tu piloto entero, al empezar y al acabar | game tables/constants and your rider in full, at start and end |
| `snapshots/*.ram.gz` | **fotos completas de la RAM** (32 MB comprimidos): al empezar, al acabar, cada 60 s, en cada cambio carrera/menú y con `snap` | **full RAM snapshots**: at start, end, every 60 s, at each race/menu change and on `snap` |
| `snapshots/*.p2s` | **savestates de PCSX2** (RAM + vídeo + audio + IOP + captura), si se encontró la carpeta de savestates | **PCSX2 savestates** (RAM + video + audio + IOP + screenshot) when the savestate folder is found |
| `notes.jsonl` | tus notas con su hora | your notes with time |
| `meta.json` | versión, juego, PCSX2, nivel, piloto, recuentos, lista de fotos | version, game, PCSX2, level, rider, counts, snapshot list |
- **ES — savestates:** usan la **ranura 10** (`--state-slot N` para otra). Si ya tenías un savestate ahí, el script **lo copia aparte antes y lo restaura al terminar**. Si no quieres tocar savestates: `--no-states` (solo RAM). **EN:** they use **slot 10**; any savestate you already had there is copied aside and restored afterwards; `--no-states` to avoid it.
- **ES — carga:** si el juego va a tirones, usa `--riders 1` o `--light`. **EN:** if the game stutters, use `--riders 1` or `--light`.

## 7. Problemas frecuentes / Troubleshooting
| Síntoma / Symptom | ES | EN |
|---|---|---|
| «No puedo conectar con PCSX2» | PCSX2 abierto + PINE activado + juego en marcha; en Windows prueba `--tcp 127.0.0.1:28011`; en Linux `--socket RUTA` | PCSX2 open + PINE on + game running; Windows `--tcp 127.0.0.1:28011`; Linux `--socket PATH` |
| «El juego cargado es …» | solo el PAL SLES-52202 | only the PAL SLES-52202 |
| No encuentra savestates | `--sstates RUTA` (carpeta `sstates` de PCSX2) o `--no-states` | `--sstates PATH` or `--no-states` |
| «Esperando a que empieces una carrera» | llega a la salida de un nivel (o usa `--tour`) | get to a level's start (or use `--tour`) |
| `check.py` dice PROBLEMA | graba otra vez, más larga y con más movimiento | record again, longer and with more movement |
| Se cerró PCSX2 / se cortó | se guarda lo grabado; sube igualmente la carpeta `SEND` | what was recorded is saved; send `SEND` anyway |

## 8. Para el proyecto / For the project
- Formato y regiones: `schema.py` (DHTEL1, direcciones de SLES-52202, documentadas). Lector: `schema.read_file(ruta)`. Comprobador: `check.py`. Pruebas: `python3 -m unittest tests.test_telemetry`.
- **Lo recibido se guarda fuera del repo** (p. ej. `~/dh-states/telemetry/`). Lo que sí se versiona son los **resúmenes derivados** (curvas, tablas, parámetros ajustados, vectores de prueba), nunca los volcados, savestates ni `.dhtel`.
- Received data stays **outside the repo**; only **derived summaries** (curves, tables, fitted parameters, test vectors) are versioned.
