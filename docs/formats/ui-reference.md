# Referencia de interfaz del original / Original UI reference
Estado 2026-10-06. Solo descripciones, medidas y números de textura; las capturas y los PNG viven fuera del repo (`~/dh-captures/`). Medidas en píxeles de una captura de 640×480. / Status 2026-10-06. Descriptions, measurements and texture numbers only; screenshots and PNGs live outside the repo (`~/dh-captures/`). Measurements in pixels of a 640×480 capture.

## ES — Cómo se obtuvo
- **Capturas reales:** cada savestate de PCSX2 (`.p2s`, un zip) lleva un `Screenshot.png` del fotograma; se extraen sin abrir el emulador (10 estados: 6 de ALP2 en carrera, 2 de otro nivel en modo con puntuación).
- **Texturas:** `python3 tools/ui_textures_export.py unpacked/SHELL/UI <salida>` (242 texturas de la escena de menús) y lo mismo sobre `unpacked/LVL/ALP2` (186). Los números `texNNN` de abajo son los de esa exportación (orden de la escena; no se asume estable entre niveles).

## ES — HUD en carrera (del juego real)
| Elemento | Posición aprox. (640×480) | Descripción | Textura (ALP2) |
|---|---|---|---|
| Ordenador de la bici | x 500-628, y 376-458 | carcasa azul grisácea con pantalla LCD verde claro: velocidad «NN KPH», reloj mm:ss y una tercera cifra (55-56 en carrera, puntuación de 5 cifras en el otro modo); 5 leds verdes a la izquierda y, en el modo con puntuación, una barra de leds naranja/rojos a la derecha | carcasa tex022; tipografía LCD de siete segmentos tex020; rótulos MPH / KPH / K.P.O + «PTS» tex017-019 (corregido: antes se citaban otros números) |
| Panel de progreso | x 8-100, y 243-472 | panel oscuro con borde verde; el trazado del recorrido en vertical con puntos de color (rojo = líder, verde = jugador y rivales) y abajo la posición «n / total» en cifras grandes (6/6, 1/10) | por localizar |
| Lista de tiempos | x 0-172, y 97-190 | 4 filas granates translúcidas con borde derecho oblicuo: tiempo a la izquierda («0:15:39» y «+01:17» de diferencia) y el logotipo de cada piloto; solo en el modo de carrera | logotipos de pilotos tex023 |
| Etiqueta lateral | x 538-640, y 330-356 | cinta oscura inclinada con texto cursivo blanco («Golpe» en español, «Bote.3» en el otro modo) | texto del ELF |
| Rótulo superior | centrado, y 14-28 | nombre del trazado y número en blanco con sombra («Black Diamond 79») | texto del ELF |
| Marcador en el mundo | sobre el trazado | rombo negro de dificultad, plano en el mundo | por localizar |
| Cartel de meta | pantalla/mundo | «FINISH» en varios idiomas (TERM/META/ENDE/FINE), cursiva amarilla sobre fondo claro | tex239, tex248-250 |
Estilo común: cursiva gruesa sin remates, blanco con sombra oscura, paneles oscuros translúcidos con bordes oblicuos, verde/ámbar para estados, esquinas redondeadas en los medidores.

## ES — Pantallas de menú y carga (SHELL, LOADBAR)
- **Logotipo** «Downhill Domination» (montaña naranja con borde, texto blanco cursivo): tex016 (ALP2, 256×128), tex174-175 (menús, partido en dos mitades). **Iconos ovalados** de menú de pausa (jugador, tabla, cámara, foto, botones, bici, indicador, música): tex099. **Numerales cursivos negros** 1-9: tex100. Carteles «BIKE SHOP» sobre paneles granates y dorados: tex126-127. Retratos de pilotos (tex064-077) y miniaturas de niveles (tex195-210) para selección. Listas de nombres de nivel por idioma: tex211-213.
- **Pantallas de carga** (`LOADBAR`, 78): un cuadro pintado a pantalla casi completa con una banda negra inferior para la barra de progreso.
## ES — Consecuencias para el HUD nativo (propuesta, sin implementar)
1. Dibujo de rectángulos con textura en `gfx::Renderer` (hoy solo color plano) y carga nativa de las texturas anteriores desde los datos del usuario (como el resto de assets, nunca en el repo).
2. Reproducir primero el ordenador de la bici (carcasa + LCD de siete segmentos) y el panel de progreso con las posiciones de la tabla (fracciones de 640×480).
3. Tipografía de texto: cursiva gruesa; la fuente real aún no se ha localizado (el 5x7 actual es provisional). Candidata: texturas de letras en el NGP del nivel o de SHELL (no encontradas en la exportación de ALP2 ni de UI).
4. Contrastar cada pantalla con las capturas de referencia con `tools/compare_view.py`.

## EN — How it was obtained
- **Real screenshots:** every PCSX2 savestate (`.p2s`, a zip) carries a `Screenshot.png` of the frame; extracted without opening the emulator (10 states: 6 of ALP2 racing, 2 of another level in the scoring mode).
- **Textures:** `python3 tools/ui_textures_export.py unpacked/SHELL/UI <out>` (242 textures of the menu scene) and the same on `unpacked/LVL/ALP2` (186). The `texNNN` numbers below are from that export (scene order; not assumed stable across levels).

## EN — In-race HUD (from the real game)
| Element | Approx. position (640×480) | Description | Texture (ALP2) |
|---|---|---|---|
| Bike computer | x 500-628, y 376-458 | blue-grey housing with a light-green LCD: speed "NN KPH", mm:ss clock and a third figure (55-56 in race, a 5-digit score in the other mode); 5 green LEDs on the left and, in the scoring mode, an orange/red LED bar on the right | housing tex022; seven-segment LCD font tex020; MPH / KPH / K.P.O + "PTS" labels tex017-019 (corrected: other numbers were cited before) |
| Progress panel | x 8-100, y 243-472 | dark panel with a green border; the course drawn vertically with coloured dots (red = leader, green = player and rivals) and the position "n / total" in large digits below (6/6, 1/10) | to locate |
| Times list | x 0-172, y 97-190 | 4 translucent maroon rows with a slanted right edge: time on the left ("0:15:39" and "+01:17" gaps) and each rider's logo; race mode only | rider logos tex023 |
| Side tag | x 538-640, y 330-356 | dark slanted ribbon with white italic text ("Golpe" in Spanish, "Bote.3" in the other mode) | ELF text |
| Top caption | centred, y 14-28 | trail name and number in white with a shadow ("Black Diamond 79") | ELF text |
| World marker | on the trail | black difficulty diamond, flat in the world | to locate |
| Finish banner | screen/world | "FINISH" in several languages (TERM/META/ENDE/FINE), yellow italic on a light background | tex239, tex248-250 |
Common style: heavy italic sans-serif, white with a dark shadow, translucent dark panels with slanted edges, green/amber for states, rounded corners on the gauges.

## EN — Menu and loading screens (SHELL, LOADBAR)
- **Logo** "Downhill Domination" (orange outlined mountain, white italic text): tex016 (ALP2, 256×128), tex174-175 (menus, split in two halves). **Oval pause-menu icons** (rider, board, camera, photo, buttons, bike, gauge, music): tex099. **Black italic numerals** 1-9: tex100. "BIKE SHOP" signs on maroon and gold panels: tex126-127. Rider portraits (tex064-077) and level thumbnails (tex195-210) for selection. Level-name lists per language: tex211-213.
- **Loading screens** (`LOADBAR`, 78): an almost full-screen painting with a black bottom band for the progress bar.
## EN — Consequences for the native HUD (proposal, not implemented)
1. Textured rectangle drawing in `gfx::Renderer` (flat colour only today) and native loading of the textures above from the user's data (like every asset, never in the repo).
2. Reproduce first the bike computer (housing + seven-segment LCD) and the progress panel at the table's positions (fractions of 640×480).
3. Text typography: heavy italic; the real font has not been located (the current 5x7 is provisional). Candidate: letter textures in the level or SHELL NGP (not found in the ALP2 or UI exports).
4. Compare each screen with the reference screenshots using `tools/compare_view.py`.

## ES — HUD implementado con las texturas reales (2026-10-06)
- **Bloque de texturas del HUD:** 8 texturas consecutivas con la secuencia de tamaños 256×128 (logotipo), 64×64 ×3 (MPH, KPH, K.P.O con «PTS»), 128×128 (fuente LCD), 128×32 (tira), 128×128 (carcasa), 256×128 (logotipos de pilotos). El contenido es idéntico en todos los niveles; solo cambia el número de partida (16 en ALP2, 4 en ALPINEMX; verificado byte a byte). `tools/hud_export.py unpacked/LVL/ALP2` lo localiza por esa secuencia y escribe `out/hud/hud.dat` (gitignorado); `dhview` lo carga al arrancar el modo jugable (`DH_HUD=ruta` para otra). Si falta, usa el texto provisional 5x7.
- **Decisión:** carga en tiempo de ejecución desde un archivo derivado de los datos del usuario, no decodificación nativa del `.TEX`/`.RTX` (esa es el paso 4 del plan del loader, tamaño L); se sustituirá cuando exista.
- **Medidas** (sobre 640×480 de referencia, comparadas con el fotograma real): carcasa x 500-628, y 368-460 (128×92); velocidad altura 21 y borde derecho x 566; reloj altura 16, borde 585; tercera cifra altura 13, borde 590; rótulo KPH x 570-592, y 395-407; línea separadora y 435,5. Fuente LCD (128×128): celdas de 24×42 en filas y 3, 49 y 100; el «1» ocupa solo 7 px a la derecha de su celda y no existe el «6» (es el «9» girado 180°). Ranuras de leds de la carcasa (x, y, ancho, alto): izquierda 5 en (18,22), (14,38), (11,54), (9,70), (8,86) de 10×11; derecha 10 barras de 8×7 a 17×9.
- **Pendiente / hipótesis:** la carcasa no trae alfa (el juego la recorta con un polígono): se usa un rectángulo redondeado aproximado; la tercera cifra del juego (55-56) se muestra como el contador de puertas (hipótesis); la barra derecha de leds (solo en modos con puntuación) y el brillo de los leds verdes no están; panel de progreso, lista de tiempos y etiquetas laterales sin hacer. La pantalla de resultados del juego no aparece en ninguna captura disponible: sigue con la maquetación provisional.

## EN — HUD implemented with the real textures (2026-10-06)
- **HUD texture block:** 8 consecutive textures with the size sequence 256×128 (logo), 64×64 ×3 (MPH, KPH, K.P.O with "PTS"), 128×128 (LCD font), 128×32 (strip), 128×128 (housing), 256×128 (rider logos). Content is identical in every level; only the starting number changes (16 in ALP2, 4 in ALPINEMX; verified byte for byte). `tools/hud_export.py unpacked/LVL/ALP2` finds it by that sequence and writes `out/hud/hud.dat` (git-ignored); `dhview` loads it when play mode starts (`DH_HUD=path` for another). If missing, it falls back to the provisional 5x7 text.
- **Decision:** runtime loading from a file derived from the user's data, not native decoding of `.TEX`/`.RTX` (that is step 4 of the loader plan, size L); it will be replaced when that exists.
- **Measurements** (on the 640×480 reference, compared with the real frame): housing x 500-628, y 368-460 (128×92); speed height 21 with right edge x 566; clock height 16, edge 585; third figure height 13, edge 590; KPH label x 570-592, y 395-407; separator line y 435.5. LCD font (128×128): 24×42 cells in rows y 3, 49 and 100; the "1" occupies only 7 px at the right of its cell and there is no "6" (it is the "9" rotated 180°). Housing LED slots (x, y, w, h): left 5 at (18,22), (14,38), (11,54), (9,70), (8,86) of 10×11; right 10 bars from 8×7 to 17×9.
- **Pending / hypothesis:** the housing has no alpha (the game clips it with a polygon): an approximate rounded rectangle is used; the game's third figure (55-56) is shown as the gate counter (hypothesis); the right LED bar (scoring modes only) and the green LED glow are missing; progress panel, times list and side tags not done. The game's results screen is in none of the available captures: it keeps the provisional layout.

## ES — Implementado el 2026-10-07 (pantalla de carga y panel de progreso)
- **Pantalla de carga** (`src/loadscreen.hpp`, `tests/loadscreen_test.cpp`, datos con `tools/loadbar_export.py` → `out/loadbar/*.lbr`): imagen real del juego a su proporción en el 88 % superior y una barra en la banda inferior. **Medido** sobre las 78 imágenes: el cuadro ocupa todo el ancho o un trapecio desde x ≈ 112-120 y acaba en la fila ≈ 400 de 512; debajo la textura es negra (se detecta por datos, `pictureBottom`). **Hipótesis:** posición/escala en pantalla, barra (sin textura localizada, colores propios), duración 1,5 s (`Race::loadLen`, parámetro; no sale del ELF), elección del archivo por prefijo del nivel (`ALP`→`LALP…`, sufijo `MX`/`TD`; el significado de los sufijos sale de los nombres).
- **Panel de progreso** (`hud::progressPanel`, `hud::fitCourse`): caja medida (x 8-100, y 243-472), trazado = línea `.PTS` girada para que salida→meta vaya hacia abajo, punto verde del jugador según puertas cruzadas / total, «1/1» sin rivales. **Hipótesis:** orientación y contenido del trazado, colores planos (la textura del panel no está localizada), punto rojo del líder ausente al no haber rivales.
## EN — Implemented on 2026-10-07 (loading screen and progress panel)
- **Loading screen** (`src/loadscreen.hpp`, `tests/loadscreen_test.cpp`, data via `tools/loadbar_export.py` → `out/loadbar/*.lbr`): the game's real image at its aspect ratio in the top 88 % and a bar in the bottom band. **Measured** on all 78 images: the picture spans the full width or a trapezoid from x ≈ 112-120 and ends at row ≈ 400 of 512; below it the texture is black (found from the data, `pictureBottom`). **Hypothesis:** on-screen position/scale, the bar (no texture located, own colours), 1.5 s duration (`Race::loadLen`, a parameter; not from the ELF), file choice by level prefix (`ALP`→`LALP…`, `MX`/`TD` suffix; suffix meaning comes from the names).
- **Progress panel** (`hud::progressPanel`, `hud::fitCourse`): measured box (x 8-100, y 243-472), course = `.PTS` line rotated so start→finish goes down, green player dot from gates crossed / total, "1/1" without rivals. **Hypothesis:** the trace's orientation and content, flat colours (panel texture not located), no red leader dot since there are no rivals.
