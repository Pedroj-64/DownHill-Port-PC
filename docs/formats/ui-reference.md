# Referencia de interfaz del original / Original UI reference
Estado 2026-10-06. Solo descripciones, medidas y números de textura; las capturas y los PNG viven fuera del repo (`~/dh-captures/`). Medidas en píxeles de una captura de 640×480. / Status 2026-10-06. Descriptions, measurements and texture numbers only; screenshots and PNGs live outside the repo (`~/dh-captures/`). Measurements in pixels of a 640×480 capture.

## ES — Cómo se obtuvo
- **Capturas reales:** cada savestate de PCSX2 (`.p2s`, un zip) lleva un `Screenshot.png` del fotograma; se extraen sin abrir el emulador (10 estados: 6 de ALP2 en carrera, 2 de otro nivel en modo con puntuación).
- **Texturas:** `python3 tools/ui_textures_export.py unpacked/SHELL/UI <salida>` (242 texturas de la escena de menús) y lo mismo sobre `unpacked/LVL/ALP2` (186). Los números `texNNN` de abajo son los de esa exportación (orden de la escena; no se asume estable entre niveles).

## ES — HUD en carrera (del juego real)
| Elemento | Posición aprox. (640×480) | Descripción | Textura (ALP2) |
|---|---|---|---|
| Ordenador de la bici | x 500-628, y 376-458 | carcasa azul grisácea con pantalla LCD verde claro: velocidad «NN KPH», reloj mm:ss y una tercera cifra (55-56 en carrera, puntuación de 5 cifras en el otro modo); 5 leds verdes a la izquierda y, en el modo con puntuación, una barra de leds naranja/rojos a la derecha | carcasa tex024; tipografía LCD de siete segmentos tex022 (0-9); rótulos KPH/MPH/PTS tex020-021 |
| Panel de progreso | x 8-100, y 243-472 | panel oscuro con borde verde; el trazado del recorrido en vertical con puntos de color (rojo = líder, verde = jugador y rivales) y abajo la posición «n / total» en cifras grandes (6/6, 1/10) | por localizar |
| Lista de tiempos | x 0-172, y 97-190 | 4 filas granates translúcidas con borde derecho oblicuo: tiempo a la izquierda («0:15:39» y «+01:17» de diferencia) y el logotipo de cada piloto; solo en el modo de carrera | logotipos de pilotos tex025 |
| Etiqueta lateral | x 538-640, y 330-356 | cinta oscura inclinada con texto cursivo blanco («Golpe» en español, «Bote.3» en el otro modo) | texto del ELF |
| Rótulo superior | centrado, y 14-28 | nombre del trazado y número en blanco con sombra («Black Diamond 79») | texto del ELF |
| Marcador en el mundo | sobre el trazado | rombo negro de dificultad, plano en el mundo | por localizar |
| Cartel de meta | pantalla/mundo | «FINISH» en varios idiomas (TERM/META/ENDE/FINE), cursiva amarilla sobre fondo claro | tex239, tex248-250 |
Estilo común: cursiva gruesa sin remates, blanco con sombra oscura, paneles oscuros translúcidos con bordes oblicuos, verde/ámbar para estados, esquinas redondeadas en los medidores.

## ES — Pantallas de menú y carga (SHELL, LOADBAR)
- **Logotipo** «Downhill Domination» (montaña naranja con borde, texto blanco cursivo): tex019 (ALP2), tex174-175 (menús, partido en dos mitades). **Iconos ovalados** de menú de pausa (jugador, tabla, cámara, foto, botones, bici, indicador, música): tex099. **Numerales cursivos negros** 1-9: tex100. Carteles «BIKE SHOP» sobre paneles granates y dorados: tex126-127. Retratos de pilotos (tex064-077) y miniaturas de niveles (tex195-210) para selección. Listas de nombres de nivel por idioma: tex211-213.
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
| Bike computer | x 500-628, y 376-458 | blue-grey housing with a light-green LCD: speed "NN KPH", mm:ss clock and a third figure (55-56 in race, a 5-digit score in the other mode); 5 green LEDs on the left and, in the scoring mode, an orange/red LED bar on the right | housing tex024; seven-segment LCD font tex022 (0-9); KPH/MPH/PTS labels tex020-021 |
| Progress panel | x 8-100, y 243-472 | dark panel with a green border; the course drawn vertically with coloured dots (red = leader, green = player and rivals) and the position "n / total" in large digits below (6/6, 1/10) | to locate |
| Times list | x 0-172, y 97-190 | 4 translucent maroon rows with a slanted right edge: time on the left ("0:15:39" and "+01:17" gaps) and each rider's logo; race mode only | rider logos tex025 |
| Side tag | x 538-640, y 330-356 | dark slanted ribbon with white italic text ("Golpe" in Spanish, "Bote.3" in the other mode) | ELF text |
| Top caption | centred, y 14-28 | trail name and number in white with a shadow ("Black Diamond 79") | ELF text |
| World marker | on the trail | black difficulty diamond, flat in the world | to locate |
| Finish banner | screen/world | "FINISH" in several languages (TERM/META/ENDE/FINE), yellow italic on a light background | tex239, tex248-250 |
Common style: heavy italic sans-serif, white with a dark shadow, translucent dark panels with slanted edges, green/amber for states, rounded corners on the gauges.

## EN — Menu and loading screens (SHELL, LOADBAR)
- **Logo** "Downhill Domination" (orange outlined mountain, white italic text): tex019 (ALP2), tex174-175 (menus, split in two halves). **Oval pause-menu icons** (rider, board, camera, photo, buttons, bike, gauge, music): tex099. **Black italic numerals** 1-9: tex100. "BIKE SHOP" signs on maroon and gold panels: tex126-127. Rider portraits (tex064-077) and level thumbnails (tex195-210) for selection. Level-name lists per language: tex211-213.
- **Loading screens** (`LOADBAR`, 78): an almost full-screen painting with a black bottom band for the progress bar.
## EN — Consequences for the native HUD (proposal, not implemented)
1. Textured rectangle drawing in `gfx::Renderer` (flat colour only today) and native loading of the textures above from the user's data (like every asset, never in the repo).
2. Reproduce first the bike computer (housing + seven-segment LCD) and the progress panel at the table's positions (fractions of 640×480).
3. Text typography: heavy italic; the real font has not been located (the current 5x7 is provisional). Candidate: letter textures in the level or SHELL NGP (not found in the ALP2 or UI exports).
4. Compare each screen with the reference screenshots using `tools/compare_view.py`.
