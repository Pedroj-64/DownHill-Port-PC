<!-- SPDX-FileCopyrightText: 2026 Pedro Soto -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Flujo de investigación

🇬🇧 [English version](../en/research.md)

Copyright (C) 2026 Pedro Soto y las personas que contribuyen a DownHill-Port-PC. Licenciado bajo GPL-3.0-or-later.

Este documento explica cómo se obtuvieron los hallazgos de `formats.md`, para que cualquiera pueda reproducirlos y ampliarlos.
**Nunca subas datos del juego, el ejecutable, assets extraídos, proyectos de Ghidra ni salida del decompilador.**

## 1. Preparar los datos (tu propia imagen de disco)

```sh
7z x "Downhill Domination.iso" -oiso_extract        # contenido del disco (ignorado por git)
python3 tools/unpack_ie.py iso_extract unpacked     # descomprime los contenedores IE (ignorado por git)
```

El disco es la versión PAL (`SLES_522.02`). El ejecutable es un ELF MIPS R5900 sin símbolos (entrada `0x10a008`)
con microcódigo VU en `.vutext`.

## 2. Configurar Ghidra

1. Instala Ghidra (`pacman -S ghidra` en Arch).
2. Instala la versión de [ghidra-emotionengine-reloaded](https://github.com/chaoticgd/ghidra-emotionengine-reloaded) que corresponda a tu Ghidra
   en `~/.config/ghidra/<version_de_ghidra>_DEV/Extensions/`. El MIPS genérico produce una salida rota para el R5900 (MMI, operaciones de 128 bits).
3. Importa con el lenguaje `r5900:LE:32:default`:

```sh
mkdir ghidra_projects
ghidra-analyzeHeadless ghidra_projects dh -import iso_extract/SLES_522.02 -processor "r5900:LE:32:default" -overwrite
```

## 3. Scripts (`tools/ghidra/`)

| Script | Uso |
|--------|-----|
| `run.sh Script.java args...` | Ejecuta un script sobre el programa importado sin reanalizar |
| `StrRefs.java <subcadena> <salida> [n]` | Busca cadenas definidas que contengan `<subcadena>` y decompila las funciones que las referencian |
| `DecompFn.java <salida> <dir>...` | Decompila las funciones en las direcciones hexadecimales dadas y las escribe en `<salida>` |
| `RefsTo.java <salida> <dir>...` | Lista las referencias a direcciones (p. ej. cadenas de extensiones) |
| `RefsSym.java <salida> <subcadena>...` | Lista las referencias a símbolos por nombre |

Escribe las salidas en `decomp/` (ignorado por git).

## 4. Método que funcionó

El juego trae muchas cadenas de depuración (`"Texture Setup w %d h %d ..."`, `"CLUTs Loaded = ..."`). Buscar cadenas → seguir las xrefs →
decompilar la función → confirmar la estructura con datos reales mediante un script pequeño de Python → documentarla en `formats.md`.
Ejemplos: la cadena de registros de texturas, la tabla de relocalización `.PTR` y el orden de carga NGP/RTX/TEX/PTR salieron así.

## 5. Comprobaciones visuales

`dhview` puede capturar un fotograma para revisarlo:

```sh
DH_CAM="x y z yaw pitch" DH_SHOT=salida.bmp ./build/dhview malla.msh
```

Con archivos `.mdl` añade `DH_WALK=1` para empezar en modo caminar (colisión con el terreno, gravedad); `F` alterna volar/caminar en vivo.

## Controles del visor y variables de depuración (`dhview`)
- `.mdl`: WASD + ratón, Shift = más rápido, `F` volar/caminar (gravedad, colisión de terreno), `R` **modo bici** (prototipo), Espacio = saltar, Esc = salir. Un `<nombre>.sky.mdl` hermano se carga solo.
- **Modo bici (prototipo):** un punto material que baja por el terreno: gravedad a lo largo de la pendiente, rodadura y resistencia del aire, W = pedalear, S = frenar, A/D = girar (menos a más velocidad), Espacio = saltito, cámara en tercera persona y marcador de pirámide roja. Empieza en el primer punto de la línea `DH_PTS` con suelo debajo (empujón de 2 m/s) y reaparece en el último punto bueno si sale del mapa. Supone **100 unidades = 1 m** (sin confirmar; `DH_UNIT=n` lo cambia).
- **Límite conocido:** la colisión usa la malla *visual*, cuyas láminas se solapan en XZ (salientes, paredes, el casco lejano grueso), así que el ciclista puede subirse a la lámina equivocada o quedarse atascado; la línea completa de ALP2 aún no se puede recorrer. Hacen falta los datos de colisión propios del juego (candidatos: los archivos hermanos de `PTS/`, p. ej. `.HDT` = tabla de 38 grupos `(inicio:u16, cuenta:u16)`). Limitar la búsqueda de suelo a la altura del punto `.PTS` más cercano se probó y empeoró (la línea se solapa consigo misma donde el recorrido se cruza).
- Variables de depuración: `DH_CAM="x y z yaw pitch"`, `DH_SHOT=archivo.bmp` (guarda un fotograma y sale), `DH_FRAMES=n` (fotograma a guardar), `DH_DT=segundos` (paso de tiempo fijo), `DH_WALK=1`, `DH_RIDE=1`, `DH_RIDE_AT=i` (empezar en el punto i de la línea), `DH_AUTOSTEER=1` (piloto automático por la línea), `DH_AUTOPEDAL=1`, `DH_PTS=archivo` (dibuja `f32 x,y,z` crudos sobre el modelo, p. ej. de `tools/pts_path.py`), `DH_NOSKY=1`, `DH_DEBUG=1` (traza de la bici).
- `extract_mesh.py` puede volcar la hoja dueña de cada triángulo con `DH_OWNERS=archivo` (u32 por triángulo) para saber a qué hoja del grafo pertenece una superficie.
- Observado en ALP2: en una misma posición XZ la malla visual tiene hasta 8 láminas apiladas (p. ej. -4154, -2691, -1197, -24, 276, 3167): la hoja grande y gruesa `0x860da0` aporta las cáscaras superior/inferior; la hoja `0x368bf0` aporta paredes de roca.

## Directorios de assets, decodificados y vistos (2026-10-02; `tools/contact_sheet.py`)
- **`LOADBAR/`** (78 grupos): las **pantallas de carga**, `L<NIVEL><IDIOMA>` (`AR FR FS MX SC SE TD` = idiomas/regiones, p. ej. `LALPSE`). Una malla de cuadriláteros planos (plano XZ, Y arriba) con un fondo T8 lineal de 512x512 (`fmt 19`) más texturas pequeñas para marcos/barras; algunos cuadriláteros no tienen material (solo color de vértice). Se ven con el arte original (Alpes, cañón, ciudad, glaciar, jungla, Moab, Perú...); `LJUMPSE`/`LTRAINSE` son las pantallas de ayuda del mando y `LSHELL` el título. `LBIKESHP`, `LPODIUM`, `LOADBAR1` y `MCSTART` salen vacíos y aún no se explican.
- **`SHELL/`** (10 grupos): `UI*` (5 idiomas) es el anillo circular del menú; `BIKESHOP*` es la sala de la tienda de bicis (variantes de idioma `F G I S`). Ambos son escenas 3D, no imágenes planas.
- **`BIKE/`** (70 grupos): **piezas de bici**, no bicis completas: cuadros (`CANFIELD`, `GIAAC16`...), horquillas (`FOX125R`, `MARZ...`), manillares (`MAN100`...), ruedas (`WH...`), además de `BIKESKEL` (un modelo de esqueleto/ensamblaje, por verificar) y los archivos de animación `BANIM.NGA` (+ `CARTER.RRS`). La bici se ensambla con piezas en tiempo de ejecución.
- **`R/`** (194 grupos): assets de ciclista. Nombre = letra de piloto + código de vestimenta. Cada grupo es un conjunto de segmentos de cuerpo (brazos con guantes, piernas) en espacio local que un esqueleto coloca, así que mostrar un ciclista ensamblado exige el esqueleto y su animación (`NGA`).
- `SKAT/` (`.SKX`, `.CTL`, 97 MB), `REP/`, `RST/`, `SND/`, `VAG/` y `MOV/` (38 vídeos `.PSS`, 1,5 GB) siguen sin tocar.
- Los modelos que no son de nivel salen oscuros: sus colores de vértice promedian 0,4-0,7 y el motor original presumiblemente los aclara (sobrebrillo de la modulación PS2); falta un control de ganancia en el visor.

## Assets de bici y ciclista (2026-10-02)
- **Las piezas de bici comparten un espacio.** Largo = Y, arriba = Z, **1 unidad ~ 0,28 m** (un cuadro mide ~6,5 unidades = 1,8 m; las ruedas 2,4 unidades = 0,7 m). Los cuadros (payload 903, p. ej. `CANFIELD`, `GIAAC16`), las piezas manillar+horquilla (payload 904, `MAN100`, `FOX125R`, `FKPAT`...) y las ruedas (payload 905, `WH*`, `ENTRWH`) **no tienen transformaciones en su grafo** (`leaf_info` sale vacío). Las ruedas están centradas en el origen; la pieza manillar+horquilla está creada **vertical** en el centro de la bici.
- **Impostor de baja calidad dentro de cada cuadro:** un cuadrilátero plano de 6 vértices (textura 0, 64x64, que abarca toda la caja del cuadro) con una imagen lateral de la bici. Es el LOD de lejos; se descarta al ensamblar la bici detallada. Su máscara alfa da la disposición real (u = de atrás a delante por Y, v por Z): bujes en **Y = -1,60 (trasero) y +2,43 (delantero), Z = -1,84** (distancia entre ejes 4,03 u ~ 1,13 m) y una horquilla inclinada ~25,7° respecto a la vertical, que coincide con la constante 0,436 rad (25°) de la animación de la bici del taller `SHBANIM.NGA`.
- **Ensamblado (verificado a ojo, `tools/assemble_bike.py`):** cuadro + ruedas en los bujes + la pieza de horquilla girada alrededor de X 0,436 rad (parte alta hacia atrás) con su extremo inferior (centro de los vértices con Z < -1,9, en (Y 0,13, Z -2,28)) llevado al buje delantero. Resultado: una bici de descenso completa (horquilla inclinada, sillín, basculante). Los datos exactos de unión vendrán presumiblemente de `BIKESKEL` + `BANIM.NGA`, aún sin decodificar; las cifras anteriores están medidas, no leídas del juego.
- **`BIKE/BIKESKEL`:** payload 900; su nodo (tipo 25, payload 900) apunta a un bloque con `u32 17` (¿nº de articulaciones?), luego matrices 4x4 identidad (pose base = identidad) y la cadena `Banim` (el conjunto de animación). Las posiciones salen, pues, de la animación y no del esqueleto.
- **`.NGA` (animación) hasta ahora:** `u32 id/hash`, luego pares `(id de animación, offset)` (`SHBANIM`: 4 animaciones `0xC6..0xC9` con paso `0x120`; `RANIM`: cientos). Un bloque común las precede: entradas `u32 0xCCCC0003, f32 valor` = **canal constante CCCC** (27 canales: 9 grupos de 3, probablemente x/y/z). Los bloques de animación empiezan con `0x03e0000c, f32 0, f32 2.0 (¿duración?), u32 0x1b (nº de canales = 27)` y siguen códigos por canal (`0x0002_0002` x13 y luego constantes `0xCCCC0003`...). Los canales no constantes (`R/RANIM.NGA` tiene curvas cuantizadas a byte) no están decodificados.
- **`R/` son brazos en primera persona, no ciclistas completos:** cada grupo son dos antebrazos con guantes (colores/skins de equipo), una pieza pequeña tipo palo entre ellos y un plano grande y borroso (texturas 1 y 2: 12 y 21 vértices que abarcan toda la vista, probablemente una sombra/desenfoque). Los 16 grupos `*DI`/`*IO` venían envueltos en capas `IE` anidadas (variante `0x0a`); `tools/unpack_ie.py` (soporte de contenedores anidados de Brandon Gil) los desenvuelve. Cada uno es un cuadrilátero plano grande con arte de ciclista (camiseta/retrato) más la misma pieza tipo palo; 441 vértices, 147 triángulos.
- **`R/KCLOPS.MB`** es un **binario de Maya 4.0** (IFF `FOR4`) olvidado en los datos: escena `C:/downhillDB_local/DH_RIDERS/XRider_skel/skel_medium.mb` con el rig del ciclista: articulaciones `xx_hip_RH/LH`, `xx_knee_RH/LH`, `xx_lumbar`, `xx_neck`, `xx_shoulder_RH/LH`, `xx_elbow_RH/LH` (10 articulaciones), polos de IK y localizadores de ragdoll (`xx_rag_loc_*`). `R/KCLOPS.NGO` (900 KB) lista por nombre los canales de traslación/rotación de ese rig. Sirve como documentación del esqueleto del ciclista; la malla del cuerpo completo aún no se ha localizado (no está en `BIKE/` ni en `R/`).
- **`SKAT/DHSKAT.SKX` (101 MB) es audio:** magia `SKEX`, luego cabeceras `VAGp` (ADPCM de PS2) y la marca `KAudioDLL`; `DHSKAT.CTL` es su índice (tabla de offsets con paso de 0x44 bytes). No son datos de esqueleto ni de malla.
- Visor: `DH_BIKE=ensamblada.mdl` dibuja la bici ensamblada en el modo bici; `DH_GAIN=n` (multiplicador de brillo; por defecto 2 en modelos sin chunks), `DH_ORBIT="yaw pitch"` (cámara orbital alrededor de un modelo), `DH_BG="r g b"` (color de fondo).

## 6. Dónde ayudar ahora

- Decodificar el resto de `PTS/` (salida/meta/puntos de control en `.PTS`, los archivos hermanos `.APT/.HDT/...`) y construir la lógica de vueltas/progreso sobre la línea del recorrido.
- LOD en ejecución por distancia (los nodos tipo 2 llevan la distancia² máxima de dibujo); es una optimización, no un problema de corrección.
- Objetos de juego: nodos tipo 25/11 y payloads > `0x3E8` (props, disparadores, salida/meta).
- **Primero los datos de colisión:** encontrar los datos propios de colisión/superficie del juego (ver el límite de arriba) antes de afinar la física de conducción.
- Ciclista: la malla del cuerpo completo YA está localizada (NGP de cada nivel, nodos tipo 25 kinds 4030-4130, `docs/formats/rider.md`); falta esqueleto/pesos y asignar canales `.NGA` a huesos; bici: leer las transformaciones de unión reales de `BIKESKEL` + `BANIM.NGA` en vez de las medidas. Empezar leyendo cómo mueve el juego la cámara/el ciclista por el recorrido.
- Texturas PSMT8H (`0x1b`); qué tanda usa qué material (`--variant`).
- Intérprete VU1 / VIF / GIF en tiempo de ejecución, sólo si la geometría animada lo exige.
- Formatos aún sin tocar: `.PTS`, `.RST`, `.REP`, `.BNK`, `.SKX`, `.CTL`, `.APT`, `.BHS`.
- Preguntas de arquitectura y lenguaje: ver [`architecture.md`](architecture.md).
