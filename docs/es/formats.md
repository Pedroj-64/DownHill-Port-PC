<!-- SPDX-FileCopyrightText: 2026 Pedro Soto -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Formatos de datos (notas de ingeniería inversa, sin datos del juego)

🇬🇧 [English version](../en/formats.md)

Estas notas describen formatos de archivo y estructuras descubiertas mediante investigación de interoperabilidad sobre la versión PAL (`SLES_522.02`).
No contienen datos del juego ni código copiado.

## Contenedor `IE\x03\x04` (casi todos los .NGP/.PTR/.RTX/.TEX)
Cabecera de 20 B, luego u32 con la longitud del nombre, nombre ASCII y deflate crudo (`zlib -15`). Ver `tools/unpack_ie.py`.
La variante con `0x0a` en el byte 4 es una envoltura sin compresión que puede contener otra capa IE; `tools/unpack_ie.py` la desenvuelve hasta obtener los datos finales (verificado en R/TDI.NGP y R/XDI.*).

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
- Pendiente: topología (tiras/índices), UVs, colores, materiales (enlace textura<->CLUT) y el microcódigo VU1 que los consume.

## Fragmentos de malla VIF (verificado en LVL/ALP2)
Cada fragmento es: `UNPACK V3-32 xN @0xB5` (posiciones) → `STCYCL cl=3 wl=1` → `UNPACK V4-32 x1 @0x238` (tag GIF: NLOOP = nº de vértices, EOP, NREG=3 con regs ST, RGBAQ, XYZ2)
→ `UNPACK S-8 xM @0x239+3i` (**índices** a las posiciones, en orden de tira; M ≤ N: sólo se usa parte de los vértices) → `UNPACK V4-8/V4-5 xM` (color)
→ `UNPACK V2-16 xM` (ST con signo, punto fijo) → `MSCNT/MSCAL` (ejecuta el microcódigo VU1).
- Las tiras de triángulos tienen reinicios (ver la regla ADC más abajo), que `tools/extract_mesh.py` aplica. Los anillos/cintas (sombras) salen bien; el terreno aún sale mezclado (varios LOD apilados, sin transformaciones de instancia).
- Hay fragmentos en espacio local (coords ~±5: props, sombras) y en espacio mundo (terreno, coords ±10⁴).
- Herramientas: `tools/vif.py` (decodificador), `tools/extract_mesh.py` (→ .msh), `tools/preview_msh.py` y el visor `dhview` (`DH_CAM="x y z yaw pitch" DH_SHOT=out.bmp` para capturas).

## Grafo de escena (primeras pistas)
- Cabecera `.NGP`: `u32 4` + 4 punteros a tablas: `0x1080` (cadenas/modelos), la tabla de nodos (`0xB66F00`: `u32 7, u32 175` + 175 punteros a nodos de 0xC0 B) y otras dos.
  Nodo: `u32 0x7f0c0019`, 5 punteros, puntero a malla, **matriz 4x4 de transformación** (floats, identidad en el ejemplo), flags.
- Entradas de directorio de 0x30 B: `[1, 0, 0x00010001, f32 ?, 4 f32 (probable esfera: centro xyz + radio), puntero a la cadena de dibujo]`.
- Pendiente: recorrer el grafo (instancias + LOD), UV/colores/materiales y el enlace textura<->CLUT. Requiere leer el recorrido de dibujo en Ghidra.

## Microcódigo VU1 (`tools/vudis.py`)
Volcado: sección ELF `.vutext` en `0x00269d90`, `0xA140` B (5160 instrucciones de 64 bit = varios programas), offset de archivo `0x160d90`. Los programas se suben con paquetes `MPG`.
Se desensambla con `python3 tools/vudis.py vutext.bin > vu1.asm`. Referencia de codificación: las tablas públicas de opcodes VU (PCSX2 `DisVUmicro.h` / `DisVUops.h`, GPL).
- Las primeras 0x400 instrucciones son datos; los programas reales están en ~0x400-0xB00 y 0xC00-0x1280. Hay 20 sitios `XGKICK` = 20 caminos de salida (tipos de vértice).
- El programa de vértices cerca de `0xC24` lee registros de 3 qwords por vértice (paso 3, igual que `STCYCL cl=3 wl=1`), obtiene la posición con `LQ vf20, 0xB5(vi9)` (`vi9` = índice del vértice, del array S-8), la transforma con la matriz de `vf1..vf4` (`MADDAx/y/z`), hace la división de perspectiva (`DIV Q, vf0w, vf25w`), la convierte a punto fijo del GS (`FTOI4`) y escribe los qwords de salida con `SQ`.
- **Los reinicios de tira (ADC) se calculan en tiempo de ejecución** a partir de flags de recorte (`CLIP`, `FSAND/FCAND`) y de palabras de flags por registro (`ILWR.w vi7`, `IAND`), y se escriben en el lane `w` del qword XYZ2 (`ISW.w`). Los flags por registro salen de pequeños paquetes de parche `STMASK` + `S-8 x2` que siguen al array de índices: un parche en la dirección `A` marca los vértices `v = (A - (hdr+3))/3` y `v+1` (`hdr` = dirección de la cabecera `V4-32`), es decir, los dos primeros vértices de cada tira. **Verificado** en la muestra de 42 vértices (parches en los vértices 6-7, 10-11, 14-15, ... = inicio de cada grupo de quad). `tools/extract_mesh.py` ya aplica esta regla en lugar de una heurística.
- Consecuencia: la geometría fiel requiere (a) un intérprete del VU1 + desempaquetado VIF + parser GIF (emulación de alto nivel de la ruta vectorial de la PS2) o (b) reimplementar a mano cada uno de los ~20 programas. Plan: prototipar (a) en Python para validar y luego pasarlo a C++.
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
- Pendiente: aplicar las transformaciones de los nodos (tipos 3/4), elegir un LOD por región según la distancia, enlazar materiales con texturas/CLUTs.

## Modelos con textura: materiales y paletas (verificado en `BIKE/BOARBIKE`, una montura con forma de jabalí)
- **Tabla de materiales en el `.PTR`:** la segunda lista (`nt` offsets u16) apunta a **ids de textura** dentro del `.NGP`, y la tercera (`nc` offsets) a las **palabras `TEX0` del GS**; ambas listas tienen la misma longitud y orden, así que la entrada *k* de una se empareja con la *k* de la otra. Cada bloque de malla `V3-32` va precedido de uno o más de estos pares (materiales alternativos).
- Campos de `TEX0` (disposición estándar del GS): `PSM` (`0x13` = PSMT8, `0x14` = PSMT4), `TW/TH` (log2 del tamaño, p. ej. 6 = 64), **`CBP` = id de paleta** (coincide con el campo `id = lo>>16` de un registro del `.RTX`).
- **Paletas:** cada registro del `.RTX` es `{next, hash, lo, hi}` + datos RGBA8; los de 256 colores son 16x16 y los de 16 colores 8x2. Las CLUT de 256 entradas usan el orden CSM1 del GS: se intercambian los bits 3 y 4 del índice del píxel (`idx' = (i & ~0x18) | ((i & 8) << 1) | ((i & 16) >> 1)`). Alfa `0x80` = opaco.
- **Texturas:** la textura con id *t* es el registro del `.TEX` con `id = t`. Las PSMT8 miden `2w x 2h` respecto a las dimensiones de la subida CT32 del registro (se decodifican como se describió arriba); las PSMT4 (detalles pequeños) **aún no están implementadas** (esos triángulos se dibujan sin textura).
- **UVs:** los valores ST de `V2-16` son punto fijo de 12 bits con signo (`/4096`) con repetición (`mod 1`); `v` se invierte (`1 - v`).
- Herramienta: `python3 tools/extract_model.py unpacked/BIKE/BOARBIKE salida.mdl [--variant N]` escribe un `.mdl` que `dhview` muestra con texturas. `--variant` elige qué material de un grupo se usa (aún se desconoce qué tanda usa cada material).
