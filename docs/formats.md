<!-- SPDX-FileCopyrightText: 2026 Pedro Soto -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Formatos de datos (ingeniería inversa, sin datos del juego)

## Contenedor `IE\x03\x04` (casi todos los .NGP/.PTR/.RTX/.TEX)
Cabecera de 20 B, luego u32 longitud de nombre, nombre ASCII y deflate crudo (`zlib -15`). Ver `tools/unpack_ie.py`.
Pendiente: variante con `0x0a` en el byte 4 y contenedores anidados (R/TDI.NGP, R/XDI.*).

## `.TEX` / `.RTX` (texturas) — descomprimido
- `u32 nbins, u32 first` (`first*16` = offset del primer registro), luego `nbins` pares `(cantidad, bytes_por_textura)`.
- Registros encadenados: `u32 next` (desplazamiento en **palabras de 32 bits**, `(next & ~3)*4` bytes; 0 = fin),
  `u32 hash`, `u64 info` con `id = lo & 0xffff` y en `hi`: ancho `1<<(hi>>8&15)`, alto `1<<(hi>>12&15)`, formato `hi&0x3f`,
  mips `hi>>19&15`, bin `hi>>27`.
- Verificado: nº de registros = suma de cantidades de bins (170 en SHELL/BIKESHOP, 290 en LVL/ALP2).
- Cada registro: 0x80 B de cabecera (plantilla de paquetes GIF/BITBLTBUF que el juego rellena en runtime) + `w*h*4` B de datos.
- **Píxeles (resuelto):** `w`,`h` son las dims de la *subida* PSMCT32. Los datos son una textura **PSMT8 (8 bpp indexado) de `2w x 2h`**
  ya reordenada: hay que subirlos linealmente como CT32 (`gs.upload32`) y leer la memoria como T8 (`gs.read8`). La imagen sale vertical-invertida.
  Tablas de swizzle en `tools/gs.py`; exportador en `tools/tex_export.py` (gris, sin paleta).
- **Paletas:** viven en el `.RTX` (cadena de registros 16x16 = CLUT de 256 colores, 8x2 = CLUT de 16; entradas RGBA8 con alfa 0x80 = opaco).
  El enlace textura<->CLUT NO está en el `.TEX`; se asigna en tiempo de carga (probablemente vía material en el `.NGP`).

## `.NGP` + `.PTR` (geometría) — imagen de memoria + tabla de relocalización
El juego carga el `.NGP` descomprimido como una imagen de memoria enlazada en la dirección base `0xA00000` y usa el `.PTR` para relocalizarla.
- `.PTR`: `u32 n`, `n` offsets (u32) dentro del NGP donde hay un puntero absoluto (`valor - 0xA00000` = offset de archivo);
  luego `u32 nt` + `nt` offsets de ids de textura (u16) a remapear; luego `u32 nc` + `nc` offsets de palabras GS TEX0 a parchear (CBP/TBP).
  Verificado en `LVL/ALP2`: todos los punteros caen en [0xA00020, base+tamaño].
- `.NGP`: `u32 4` + 4 punteros a listas de nivel superior; registros de 0x30 B con (cuenta, flags, hash/bbox, puntero a malla).
- La geometría está en **paquetes VIF**: `UNPACK V3-32` (`0x68nnXXXX`, `n` vectores, luego `n*12` B de floats) contiene vértices;
  `UNPACK V4-32` (`0x6c..`) etc. para el resto. `tools/extract_points.py` extrae los V3-32 → ALP2 da 243 951 vértices que dibujan la pista real.
- Pendiente: topología (tiras/índices), UVs, colores, materiales (enlace textura<->CLUT) y el microcódigo VU1 que los consume.

## Fragmentos de malla VIF (verificado en LVL/ALP2)
Cada fragmento es: `UNPACK V3-32 xN @0xB5` (posiciones) → `STCYCL cl=3 wl=1` → `UNPACK V4-32 x1 @0x238` (tag GIF: NLOOP=nº vértices, EOP, NREG=3 con regs ST,RGBAQ,XYZ2)
→ `UNPACK S-8 xM @0x239+3i` (**índices** a las posiciones, en orden de tira; M ≤ N: sólo se usa parte de los vértices) → `UNPACK V4-8/V4-5 xM` (color RGBA)
→ `UNPACK V2-16 xM` (ST con signo, punto fijo) → `MSCNT/MSCAL` (ejecuta microcódigo VU1).
- Las tiras son de triángulos con reinicios (el ADC está en el microcódigo): `tools/extract_mesh.py` los corta por arista anómala (heurística local). Anillos/cintas (sombras) salen bien; el terreno aún sale mezclado (varios LOD apilados, sin transformaciones de instancia).
- Hay fragmentos en espacio local (coords ~±5: props, sombras) y en espacio mundo (terreno, coords ±10⁴).
- Herramientas: `tools/vif.py` (decodificador), `tools/extract_mesh.py` (→ .msh), `tools/preview_msh.py`, y el visor `dhview` (`DH_CAM="x y z yaw pitch" DH_SHOT=out.bmp` para capturas).

## Grafo de escena (primeras pistas)
- Cabecera `.NGP`: `u32 4` + 4 punteros a tablas: `0x1080` (cadenas/modelos), tabla de nodos (`0xB66F00`: `u32 7, u32 175` + 175 punteros a nodos de 0xC0 B),
  y otras dos. Nodo: `u32 0x7f0c0019`, 5 punteros, puntero a malla, **matriz 4x4 de transformación** (floats, identidad en el ejemplo), flags.
- Entradas de directorio de 0x30 B: `[1, 0, 0x00010001, f32 ?, 4 f32 (probable esfera: centro xyz + radio), puntero a cadena de dibujo]`.
- Pendiente: recorrer el grafo (instancias + LOD), UV/colores/materiales y el enlace textura<->CLUT. Requiere leer el recorrido de dibujo en Ghidra.

## Microcódigo VU1 (pendiente de desensamblar)
- Está en el ELF: sección `.vutext` @ `0x00269d90`, `0xA140` B (5160 instrucciones de 64 bit = varios programas). Lo suben paquetes `MPG`.
- Referencia de codificación: tablas de `pcsx2/DebugTools/DisVUmicro.h` / `DisVUops.h` (PCSX2, GPL) — sirven para escribir un desensamblador Python propio (`tools/vudis.py`).
- Lo que hay que sacar de ahí: regla de reinicio de tira (ADC), cómo se transforman posiciones (matriz de nodo), cómo se generan UV (ST) y color.
- Nota: en el ELF no hay referencias directas a los registros DMA/VIF1; el envío se hace con rutinas de biblioteca, así que el recorrido de dibujo debe buscarse por estructuras (nodo `0x7f0c0019...`) y no por registros.
