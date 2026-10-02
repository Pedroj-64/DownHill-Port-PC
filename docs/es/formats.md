<!-- SPDX-FileCopyrightText: 2026 Pedro Soto -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Formatos de datos (notas de ingeniería inversa, sin datos del juego)

🇬🇧 [English version](../en/formats.md)

Estas notas describen formatos de archivo y estructuras descubiertas mediante investigación de interoperabilidad sobre la versión PAL (`SLES_522.02`).
No contienen datos del juego ni código copiado.

## Contenedor `IE\x03\x04` (casi todos los .NGP/.PTR/.RTX/.TEX)
Cabecera de 20 B, luego u32 con la longitud del nombre, nombre ASCII y deflate crudo (`zlib -15`). Ver `tools/unpack_ie.py`.
La variante con `0x0a` en el byte 4 es una envoltura sin compresión que puede contener otra capa IE; `tools/unpack_ie.py` la desenvuelve hasta obtener los datos finales (verificado en R/TDI.NGP y R/XDI.*).

## GIFtag (cabecera de transferencia al GS)
- Es una cabecera de 128 bits: `NLOOP` ocupa bits 0-14, `EOP` el bit 15, `PRE` el bit 46, `PRIM` los bits 47-57, `FLG` los bits 58-59 y `NREG` los bits 60-63.
- `NREG = 0` representa 16 registros; los descriptores de 4 bits se leen en `REGS`, desde el bit 64.
- En `PACKED`, el payload ocupa `NLOOP * NREG` qwords; en `REGLIST`, `NLOOP * ceil(NREG / 2)` qwords; en `IMAGE`, `NLOOP` qwords.
- `tools/gif.py` valida la cabecera, los modos y el tamaño declarado sin interpretar aún los registros GS ni el bit ADC.

## `.TEX` / `.RTX` (texturas) — descomprimido
- `u32 nbins, u32 first` (`first*16` = offset del primer registro), luego `nbins` pares `(cantidad, bytes_por_textura)`.
- Registros encadenados: `u32 next` (desplazamiento en **palabras de 32 bits**, `(next & ~3)*4` bytes; 0 = fin), `u32 hash`, `u64 info` con `id = lo & 0xffff` y, en `hi`:
  ancho `1<<(hi>>8&15)`, alto `1<<(hi>>12&15)`, formato `hi&0x3f`, mips `hi>>19&15`, bin `hi>>27`.
- Verificado: nº de registros = suma de las cantidades de los bins (170 en SHELL/BIKESHOP, 290 en LVL/ALP2).
- Cada registro: 0x80 B de cabecera (plantilla de paquetes GIF/BITBLTBUF que el juego rellena en ejecución) + `w*h*4` B de datos.
- **Píxeles (resuelto):** `w`,`h` son las dimensiones de la *subida* PSMCT32. Los datos son una textura **PSMT8 (8 bpp indexado) de `2w x 2h`** ya reordenada (swizzle):
  se suben linealmente como CT32 (`gs.upload32`) y se lee la memoria como T8 (`gs.read8`). La imagen sale invertida verticalmente.
  Las tablas de swizzle están en `tools/gs.py`; el exportador es `tools/tex_export.py` (gris, sin paleta).
- **Paletas:** están en el `.RTX` (cadena de registros; 16x16 = CLUT de 256 colores, 8x2 = CLUT de 16; entradas RGBA8 con alfa 0x80 = opaco).
  El enlace textura<->CLUT NO está en el `.TEX`; se asigna al cargar (probablemente mediante un material en el `.NGP`).

## `.NGP` + `.PTR` (geometría) — imagen de memoria + tabla de relocalización
El juego carga el `.NGP` descomprimido como una imagen de memoria enlazada en la dirección base `0xA00000` y usa el `.PTR` para relocalizarla.
- `.PTR`: `u32 n`, luego `n` offsets u32 dentro del NGP donde hay un puntero absoluto (`valor - 0xA00000` = offset de archivo);
  después `u32 nt` + `nt` offsets de ids de textura (u16) a remapear; después `u32 nc` + `nc` offsets de palabras GS TEX0 a parchear (CBP/TBP).
  Verificado en `LVL/ALP2`: todos los punteros caen en [0xA00020, base+tamaño].
- `.NGP`: `u32 4` + 4 punteros a listas de nivel superior; registros de 0x30 B con (cuenta, flags, hash/bbox, puntero a malla).
- La geometría está en **paquetes VIF**: `UNPACK V3-32` (`0x68nnXXXX`, `n` vectores, luego `n*12` B de floats) contiene vértices;
  `UNPACK V4-32` (`0x6c..`) etc. contienen el resto. `tools/extract_points.py` extrae los bloques V3-32: ALP2 da 243 951 vértices que dibujan la pista real.
- Topología, UV, colores y materiales están resueltos en las secciones siguientes. Sigue abierta la reproducción exacta de la ruta VU1 (ver *Microcódigo VU1*).

## Fragmentos de malla VIF (verificado en LVL/ALP2)
Cada fragmento es: `UNPACK V3-32 xN @0xB5` (posiciones) → `STCYCL cl=3 wl=1` → `UNPACK V4-32 x1 @0x238` (tag GIF: NLOOP = nº de vértices, EOP, NREG=3 con regs ST, RGBAQ, XYZ2)
→ `UNPACK S-8 xM @0x239+3i` (**índices** a las posiciones, en orden de tira; M ≤ N: sólo se usa parte de los vértices) → `UNPACK V4-8/V4-5 xM` (color)
→ `UNPACK V2-16 xM` (ST con signo, punto fijo) → `MSCNT/MSCAL` (ejecuta el microcódigo VU1).
- Las tiras de triángulos tienen reinicios (ver la regla ADC más abajo), que `tools/extract_mesh.py` aplica. Los anillos/cintas (sombras) salen bien, y con el filtro del grafo de escena (más abajo) también todo el terreno.
- Hay fragmentos en espacio local (coords ~±5: props, sombras) y en espacio mundo (terreno, coords ±10⁴).
- Herramientas: `tools/vif.py` (decodificador), `tools/extract_mesh.py` (→ .msh), `tools/preview_msh.py` y el visor `dhview` (`DH_CAM="x y z yaw pitch" DH_SHOT=out.bmp` para capturas).

## Grafo de escena (primeras pistas)
- Cabecera `.NGP`: `u32 4` + 4 punteros a tablas: `0x1080` (cadenas/modelos), la tabla de nodos (`0xB66F00`: `u32 7, u32 175` + 175 punteros a nodos de 0xC0 B) y otras dos.
  Nodo: `u32 0x7f0c0019`, 5 punteros, puntero a malla, **matriz 4x4 de transformación** (floats, identidad en el ejemplo), flags.
- Entradas de directorio de 0x30 B: `[1, 0, 0x00010001, f32 ?, 4 f32 (probable esfera: centro xyz + radio), puntero a la cadena de dibujo]`.
- El recorrido del grafo, las instancias, los materiales y el enlace textura<->CLUT están hechos: ver *Recorrido del grafo de escena* y *Modelos con textura* más abajo.

## Microcódigo VU1 (`tools/vudis.py`)
Volcado: sección ELF `.vutext` en `0x00269d90`, `0xA140` B (5160 instrucciones de 64 bit = varios programas), offset de archivo `0x160d90`. Los programas se suben con paquetes `MPG`.
Se desensambla con `python3 tools/vudis.py vutext.bin > vu1.asm`. Referencia de codificación: las tablas públicas de opcodes VU (PCSX2 `DisVUmicro.h` / `DisVUops.h`, GPL).
- Las primeras 0x400 instrucciones son datos; los programas reales están en ~0x400-0xB00 y 0xC00-0x1280. Hay 20 sitios `XGKICK` = 20 caminos de salida (tipos de vértice).
- El programa de vértices cerca de `0xC24` lee registros de 3 qwords por vértice (paso 3, igual que `STCYCL cl=3 wl=1`), obtiene la posición con `LQ vf20, 0xB5(vi9)` (`vi9` = índice del vértice, del array S-8), la transforma con la matriz de `vf1..vf4` (`MADDAx/y/z`), hace la división de perspectiva (`DIV Q, vf0w, vf25w`), la convierte a punto fijo del GS (`FTOI4`) y escribe los qwords de salida con `SQ`.
- **Los reinicios de tira (ADC) se calculan en tiempo de ejecución** a partir de flags de recorte (`CLIP`, `FSAND/FCAND`) y de palabras de flags por registro (`ILWR.w vi7`, `IAND`), y se escriben en el lane `w` del qword XYZ2 (`ISW.w`). Los flags por registro salen de pequeños paquetes de parche `STMASK` + `S-8 x2` que siguen al array de índices: un parche en la dirección `A` marca los vértices `v = (A - (hdr+3))/3` y `v+1` (`hdr` = dirección de la cabecera `V4-32`), (`hdr` = dirección de la cabecera `V4-32`). **Corrección (verificada renderizando ALP2):** un vértice marcado sólo *suprime el disparo* (ningún triángulo termina en él); la tira continúa y la paridad del orden de vértices cuenta desde el inicio del lote. Ambas herramientas aplican `si j en flag: saltar`, paridad `j & 1`.
- Consecuencia: la geometría fiel requiere (a) un intérprete del VU1 + desempaquetado VIF + parser GIF (emulación de alto nivel de la ruta vectorial de la PS2) o (b) reimplementar a mano cada uno de los ~20 programas. Estado actual: **no hace falta por ahora**: las reglas de geometría estática anteriores reproducen el terreno sin intérprete VU1. Puede seguir siendo necesario para objetos animados y la bici. Ver [`architecture.md`](architecture.md) para saber dónde irá la implementación de ejecución.
- Las matrices (`vf1..vf4`), la cámara y las constantes de cada dibujo las escribe el código de CPU del juego antes de ejecutar la cadena; no están en el `.NGP`.
- El ELF no tiene referencias directas a los registros DMA/VIF1 (el envío pasa por rutinas de biblioteca), así que el recorrido de dibujo debe buscarse por estructuras de datos (nodo `0x7f0c0019...`), no por registros.

## Recorrido del grafo de escena (verificado con el propio recorrido del juego; `tools/scene.py`)
La cabecera del `.NGP` es `u32 n` seguido de `n` punteros a raíces (4 en ALP2). La primera palabra de cada nodo: bits 0-5 = **tipo de nodo**, bits 18+ = **tipo de payload** (se despacha a un callback).
- Las raíces de tipo 7 son tablas de registro (la tabla de 175 entradas en `0xB66F00`); las demás se recorren.
- Tipos de nodo y disposición de los hijos: **1** grupo (cuenta `u16` de hijos en `+8`, array de punteros en `+0x20`; una máscara `u16` en `+10` oculta el nodo); **2** selector/LOD (cuenta `u32` en `+4`, entradas de 8 B en `+0x28`, primer campo = puntero); **3** traslación (`f32 x,y,z` en `+0x10`, cuenta `u32` en `+8`, hijos en `+0x1c`); **4** matriz (matriz en `+0x10`, traslación en `+0x40`, cuenta en `+8`, hijos en `+0x50`); **6** lista (byte de cuenta en `+0xb`, hijos en `+0xc`); **8** enlace (puntero en `+4`); el resto son hojas. El recorrido mantiene una pila de transformaciones (hasta 150 entradas).
- Los payloads `1` y `2` son mallas estáticas; los mayores de `0x3E8` son objetos de juego (disparadores, props, etc.).
- Los nodos de grupo de malla tienen hojas de 0x30 B: `[hdr=1, 0, flags, hash, f32 centro xyz, f32 radio, ..., puntero a una cadena VIF en +0x20]`.
  En `LVL/ALP2`, nueve hojas de esfera grande (radio 3000-13000) apuntan a cadenas de LOD lejano y grueso desde `0x1A7F90`; un décimo subárbol contiene el detalle fino cerca del inicio del archivo. Por eso, dibujar todo a la vez apila varios LOD.
- **Tandas (batches):** un bloque de posiciones `V3-32` se reutiliza en varias tandas de dibujo (cada una = cabecera `V4-32` + índices `S-8` + color + ST + `MSCNT`). Leer sólo la primera tanda pierde la mayoría de los triángulos; `tools/extract_mesh.py` ahora extrae todas (13 069 tandas / 274 k triángulos en ALP2) y la superficie del terreno sale coherente.
- **Aplicado y verificado (`tools/scene.py`: `instances`, `Owners`):** las matrices de los nodos 3/4 son rotación ortonormal + traslación (fila 3), pero sólo 70 de 184 hojas las tienen y afectan al 7 % de la geometría. La causa real de la "sopa" era dibujar todo a la vez: el subárbol de la raíz con payload **1** (`0xEE0` en ALP2) es el **detalle fino** del nivel (cada celda es un nodo tipo 2 con `d2` = distancia² máxima de dibujo, p. ej. 25e6 = 5000 u); el de payload **2** (`0x50`) es un LOD lejano diminuto (928 vértices); el resto son objetos de juego. Cada bloque `V3-32` pertenece a la hoja con mayor inicio de cadena <= su offset. Con sólo el detalle fino (208 k vértices, 227 k triángulos) sale un circuito reconocible.
- **ADC corregido:** el bit ADC (parche S-8 x2 en el slot UV) marca vértices que **no disparan** triángulo, pero la tira continúa y la paridad del orden de vértices cuenta desde el inicio del lote; tratarlo como reinicio perdía el primer triángulo de cada tira (227 k -> 273 k triángulos en ALP2) y dejaba huecos.
- **Materiales de nivel (ver también los formatos de registro TEX más abajo):** un bloque sin `TEX0` propio hereda el material del bloque anterior (estado GS persistente); con eso el 100 % de los vértices lleva textura (antes 13 % sin ella). Los PSMT4 ya se decodifican (`tools/gs.py: read4`). `extract_model.py` ahora usa el mismo filtro/transformación en `LVL/*`.
- **Color y alfa por vértice (verificado en ALP2):** `V4-8` = RGBA8 y `V4-5` = RGBA 5551 (canal <<3); 0x80 = 1.0 (modulación PS2). Contienen la **iluminación horneada** y el **alfa** de las capas superpuestas (sombras: textura negra con alfa de vértice ~36-45/128). El `.mdl` pasa a `DHM2` (10 f32 por vértice: `x y z u v r g b a tex`). Las UV ya no se reducen con `mod 1` por vértice (rompía la interpolación; la repetición la hace `GL_REPEAT`).
- **Visor (`dhview`):** mezcla alfa + `GL_LEQUAL` (las capas comparten posición con el suelo), plano cercano proporcional a la escala. Con `.mdl`: `F` alterna volar/caminar; caminar usa una rejilla XZ de triángulos (Y = arriba), gravedad, salto (Espacio) y escalón máximo 0.6*eye. `DH_WALK=1` arranca caminando.
- Pendiente: LOD en ejecución por distancia (es una optimización: el subárbol de detalle fino no apila LOD), objetos/props (nodos 25/11), PSMT8H, física de la bici.

## `.PTS` (grafo de puntos del recorrido) — `unpacked/PTS/<NIVEL>.PTS`
`8` bytes de cabecera + `2048` registros de `32` bytes: `f32 x, y, z` (mismo espacio de mundo que la malla del nivel, verificado superponiéndolos en ALP2), luego ocho campos `i16` `a0..a7`, luego 4 bytes de relleno.
- **`a6` = índice del registro siguiente** (mediana de distancia a él: 38 u; las cadenas confluyen, 64 cabezas de cadena en ALP2). Seguir `a6` desde el registro 0 da una única línea de carrera limpia de 611 puntos / ~55 000 u que recorre el sendero de principio a fin (`tools/pts_path.py --chain 0`).
- `a0`, `a1`: contadores crecientes que siguen aproximadamente el progreso por el recorrido (no estrictamente monótonos en el orden del archivo; unidad sin confirmar). `a2`: categoría pequeña (5 la más común, luego 6, 10, 2, 1, 7). `a5`: 1 o 2. `a4`: tipo máscara (0, 2, 8, 512, 16384, ...). `a7`: otro índice (949 valores distintos, registros lejanos): significado desconocido.
- Aún sin decodificar: qué registros son salida/meta/puntos de control, anchos de carril, los archivos hermanos de `PTS/` (`.APT .BHS .BLM .BPT .BRD .DPT .FPT .HDT .HLI .HWK .JPT .PED .PKP .RPL .SPT`; casi todos empiezan con la misma cabecera `01 20 18 10` y contienen coordenadas f32).
- Uso: línea de conducción/IA, progreso y vueltas, posición de salida, raíles de cámara. Herramienta: `python3 tools/pts_path.py NIVEL.PTS salida.pts [--chain N] [--stats]`; verla con `DH_PTS=salida.pts dhview nivel.mdl` (puntos dibujados encima del modelo).

## Telón de fondo / cielo (verificado en ALP2, JUNGLE, MOAB, PERU, GLACIERT)
El subárbol de detalle fino de cada nivel tiene hojas cuya palabra de cabecera en `+8` guarda una *capa* en la mitad alta: `3` celdas de terreno, `5` otros y exactamente una hoja con `2` = **telón de fondo/cielo** (una cúpula con nubes en JUNGLE, un valle lejano en ALP2). `extract_model.py` lo escribe en `<nombre>.sky.mdl`; `dhview` lo carga solo y lo dibuja primero sin escribir profundidad (`DH_NOSKY=1` lo oculta). `extract_mesh.py` lo omite salvo con `DH_SKY=1`.

## Formatos de registro `.TEX` (corrige la nota anterior «todos los T8 son 2w x 2h») — verificado en LOADBAR, LVL
El campo `fmt` del registro (`hi & 0x3f`) es el **formato GS con el que se sube** el dato; la disposición de píxeles depende de él:
- `fmt 0` (subida PSMCT32, 20 321 registros): `w x h` palabras con el **swizzle del GS** aplicado. La textura real (según `TEX0.PSM` del material) es T8 de `2w x 2h`, T4 o T8H (`gs.read8`, `gs.read4`, `gs.read8h`: en PSMT8H el índice de paleta es el byte alto de cada píxel de 32 bits).
- `fmt 19` (`0x13`, subida PSMT8, 111 registros): `w x h` bytes **lineales**, un índice de paleta por píxel, sin swizzle. Las 78 pantallas de carga lo usan (fondo de 512x512).
- `fmt 20` (`0x14`, subida PSMT4, 441 registros, todos en `LVL`): `w x h` nibbles **lineales**, dos píxeles por byte, nibble bajo primero.
- Otros valores aparecen una vez cada uno en `R/` (16, 18, 10, 28, 47, 30, 40, 32, 8, 14, 7, 51, 42): sin decodificar.
- La longitud de datos es `w*h*4` (fmt 0), `w*h` (19) o `w*h/2` (20). Leer `w*h*4` en un registro fmt 19 entraba en el registro siguiente y daba las imágenes rayadas y desordenadas de los primeros renders de pantallas de carga.
- Error corregido en `gs.upload32`: reservaba solo `len(data)` bytes, pero el GS escribe páginas completas de 8 KB, así que las texturas menores que una página (p. ej. 64x16) direccionaban más allá del final y se decodificaban mal. Ahora reserva páginas completas.
- CLUT: las paletas de 256 entradas usan el intercambio de bits 3/4 de CSM1 para cualquier textura de 8 bits (T8, T8H, T8 lineal); las de 16 entradas se usan tal cual.

## Grafo de escena completo y rangos de visibilidad (ALP2: 184 hojas de malla, 106 con rangos) — `tools/scene.py: leaf_info`
- Exportar solo el subárbol de detalle fino deja grandes huecos (plataforma de salida, montañas lejanas). La escena fiel es **todo el grafo**: cada hoja con su matriz acumulada **y** los nodos selectores tipo 2 que tiene encima: `(centro xyz, radio, distancia² máxima)` (aparecen rangos de 400 u, 800 u, 1 000 u, 5 000 u y 6 500 u). Una hoja se dibuja solo mientras la cámara está a menos de `sqrt(d2) + radio` de cada selector superior.
- `extract_model.py` escribe ahora **`DHM3`**: `DHM2` más, tras los índices, `u32 nchunks` y por chunk `u32 primer_índice, u32 nº_índices, u32 nsel, nsel * (cx, cy, cz, radio, dist2_max)`. Un chunk por hoja de malla. `dhview` aplica los rangos en cada fotograma (`DH_NOCULL=1` lo dibuja todo). `DH_SUB=fine` recupera la exportación antigua solo de detalle fino.
- El descarte de caras traseras **no** funciona (probados los dos sentidos de giro, ambos pierden la mitad de los triángulos): las mallas mezclan el sentido, así que el juego las dibuja a doble cara. Hay que dejarlo desactivado.
- **Escala:** los 3 carriles de la línea `.PTS` están separados ~22 u, lo que apunta a unas **10 u = 1 m** (un recorrido de 55 000 u serían 5,5 km y un desnivel de 7 500 u, 750 m; plausible para un descenso alpino). Sin confirmar. A esa escala el visor muestra un sendero de tierra reconocible con hierba a los lados y abetos (texturas con recorte alfa).

## Modelos con textura: materiales y paletas (verificado en `BIKE/BOARBIKE`, una montura con forma de jabalí)
- **Tabla de materiales en el `.PTR`:** la segunda lista (`nt` offsets u16) apunta a **ids de textura** dentro del `.NGP`, y la tercera (`nc` offsets) a las **palabras `TEX0` del GS**; ambas listas tienen la misma longitud y orden, así que la entrada *k* de una se empareja con la *k* de la otra. Cada bloque de malla `V3-32` va precedido de uno o más de estos pares (materiales alternativos).
- Campos de `TEX0` (disposición estándar del GS): `PSM` (`0x13` = PSMT8, `0x14` = PSMT4), `TW/TH` (log2 del tamaño, p. ej. 6 = 64), **`CBP` = id de paleta** (coincide con el campo `id = lo>>16` de un registro del `.RTX`).
- **Paletas:** cada registro del `.RTX` es `{next, hash, lo, hi}` + datos RGBA8; los de 256 colores son 16x16 y los de 16 colores 8x2. Las CLUT de 256 entradas usan el orden CSM1 del GS: se intercambian los bits 3 y 4 del índice del píxel (`idx' = (i & ~0x18) | ((i & 8) << 1) | ((i & 16) >> 1)`). Alfa `0x80` = opaco.
- **Texturas:** la textura con id *t* es el registro del `.TEX` con `id = t`. Las PSMT8 miden `2w x 2h` respecto a las dimensiones de la subida CT32 del registro (se decodifican como se describió arriba); las PSMT4 (texturas de detalle pequeñas) se decodifican con `gs.read4` (direccionamiento por nibbles, bloques de 32x16). Las PSMT8H (`0x1b`, 3 materiales en ALP2) no se tratan.
- **UVs:** los valores ST de `V2-16` son punto fijo de 12 bits con signo (`/4096`) y se escriben **sin reducir** (sin `mod 1` por vértice: rompe la interpolación en triángulos que cruzan el borde de una repetición; la repetición la hace `GL_REPEAT` del visor); `v` se invierte (`1 - v`).
- Herramienta: `python3 tools/extract_model.py unpacked/BIKE/BOARBIKE salida.mdl [--variant N]` escribe un `.mdl` que `dhview` muestra con texturas. `--variant` elige qué material de un grupo se usa (aún se desconoce qué tanda usa cada material). En los grupos `LVL/*` la herramienta conserva automáticamente sólo el subárbol de detalle fino y aplica las transformaciones del grafo (se cambia con `DH_SUB=all` o `DH_SUB=0x50,0xee0`).

## Formatos de salida de las herramientas (propios, no del juego)
Todo en little-endian.
- **`.msh`** (`extract_mesh.py`): `u32 nv, u32 ni`, `nv * 3 f32` (x y z), `ni * u32` índices de triángulo. Y es arriba.
- **`.mdl`, magia `DHM2`** (`extract_model.py`): `u32 ntex, u32 nv, u32 ni`; por textura `u32 w, u32 h` + `w*h*4` bytes RGBA8; `nv * 10 f32` = `x y z u v r g b a tex` (`r g b a` en 0..~2, 1.0 = neutro; `tex` = índice de textura o -1); `ni * u32` índices. Las texturas se repiten; `v` ya está invertida. (`DHM1` tenía 9 floats y no llevaba alfa; `dhview` sólo lee `DHM2`.)
- Variables de entorno de depuración de las herramientas: `DH_SUB` (subárboles a conservar: vacío = detalle fino, `all`, o lista de nodos), `DH_NOXFORM=1` (omite las transformaciones del grafo, `extract_mesh.py`), `DH_MIN/DH_MAX/DH_OFF0/DH_OFF1/DH_RADIUS` (filtros). Visor: `DH_CAM`, `DH_SHOT`, `DH_WALK`.
