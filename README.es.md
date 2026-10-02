# DownHill-Port-PC

🇬🇧 [English](README.md) · 🇪🇸 Español

Un esfuerzo comunitario para llevar **Downhill Domination** (Incognito Entertainment, PS2, 2003) a los PC modernos, de forma nativa en **Linux y Windows**.

Este es un proyecto **de la comunidad y para la comunidad**. No está afiliado, respaldado ni conectado con los desarrolladores o distribuidores originales.

> **Estado: ingeniería inversa temprana.** Todavía no hay un juego jugable. Hoy existen notas de formatos, herramientas de extracción y un pequeño visor 3D. Ver la [Hoja de ruta](#hoja-de-ruta).

## Objetivo

Una reimplementación nativa del motor (no un envoltorio de emulador) que carga los datos originales del juego desde **tu propia imagen de disco obtenida legalmente** y funciona en sistemas actuales con resoluciones, controles y fluidez modernos.

## Aviso legal

- **Este repositorio no contiene datos del juego, ni código del juego, ni resultados de desensamblado o decompilación.** No se incluye ni se incluirá ninguna ISO, ejecutable, textura, modelo, audio ni video.
- Debes aportar **tu propia copia** del juego. Las herramientas leen tu imagen de forma local y no se sube nada a ningún sitio.
- El `.gitignore` está configurado para mantener fuera del control de versiones las imágenes de disco, los assets extraídos, el ejecutable del juego, los proyectos de Ghidra y la salida del decompilador. **Por favor, respétalo en tus pull requests.**
- La documentación describe *formatos de archivo y estructuras* descubiertos mediante investigación de interoperabilidad. Si eres titular de derechos y tienes dudas, abre un issue.

## Qué funciona hasta ahora

| Área | Estado |
|------|--------|
| Extracción de la ISO | Hecho (cualquier herramienta de ISO; sirve `7z x`) |
| Contenedor `IE` de assets (`.NGP/.PTR/.RTX/.TEX`) | Descompresor funcionando (`tools/unpack_ie.py`); faltan algunas variantes anidadas |
| Texturas `.TEX` | Decodificadas a imágenes indexadas de 8 bits (swizzle del GS deshecho), exportadas en gris |
| Paletas `.RTX` | Decodificadas; el enlace textura–paleta está resuelto mediante la tabla de materiales del `.PTR` |
| Tabla de relocalización `.PTR` | Entendida (base `0xA00000`) |
| Geometría `.NGP` y grafo de escena | Resuelto para la geometría estática del nivel: recorrido de nodos, transformaciones, subárbol de detalle fino, tiras con la regla ADC, UV, color de vértice y alfa |
| Microcódigo VU1 | Localizado y desensamblado (`tools/vudis.py`); por ahora no hace falta intérprete para la geometría estática |
| Modelos y niveles con textura | Los 54 niveles se extraen; ALP2 se exporta como **todo el grafo de escena** con rangos de visibilidad por hoja (`DHM3`). Todos los formatos de subida de textura decodificados (CT32 con swizzle, T8/T4 lineal, T8H). 78 pantallas de carga se ven con su arte original. Las piezas de bici (`BIKE/`) y los segmentos de ciclista (`R/`) se ven; ensamblarlos exige el esqueleto y las animaciones |
| Visor (`dhview`) | Visor SDL3 + OpenGL de puntos, triángulos y modelos con textura (`.mdl`): cámara libre, capas con alfa, modo caminar con colisión de terreno |
| Línea del recorrido | `.PTS` decodificado como grafo de puntos (línea de carrera de ~55 000 u en ALP2); faltan salida/meta/puntos de control y los archivos hermanos |
| Bici / física | Prototipo de punto material (`R` en el visor); aún no se han hallado los datos de colisión reales, así que no recorre un circuito entero |
| IA, audio, menús | Sin empezar |

Las notas de formatos están en [`docs/es/formats.md`](docs/es/formats.md); la configuración de Ghidra y el flujo de investigación, en [`docs/es/research.md`](docs/es/research.md). La arquitectura y la decisión de lenguaje están en [`docs/es/architecture.md`](docs/es/architecture.md). Todos también existen en inglés en [`docs/en/`](docs/en/).

## Hoja de ruta

1. ✅ Recorrer el grafo de escena y renderizar la geometría del nivel limpia y con textura (terreno estático hecho).
2. ✅ Telón de fondo/cielo separado. Siguiente: decodificar `PTS/` (línea del recorrido ya hallada, faltan salida/meta), props y objetos de juego, LOD en ejecución opcional.
3. Decidir el lenguaje y la arquitectura del motor y escribir los parsers nativos ([`docs/es/architecture.md`](docs/es/architecture.md)); las herramientas Python pasan a ser el oráculo de referencia.
4. Datos de colisión, modelo de la bici, animación, cámara y física de conducción (existe un prototipo de punto material).
   (Un intérprete VU1 sólo se hará si la geometría animada lo necesita.)
5. Audio (VAG/BNK), video (PSS), menús y modos de juego, datos de guardado.
6. Empaquetado: compilaciones `.deb` y de Windows. El paquete **no** incluirá datos del juego; la aplicación extraerá los assets de tu imagen en el primer arranque.

## Compilación

Requisitos: compilador C++20, CMake ≥ 3.20, Ninja y los archivos de desarrollo de SDL3 y OpenGL.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/dhview ruta/a/malla.msh      # WASD + ratón, Shift = rápido, Esc = salir
./build/dhview nivel.mdl             # igual, y además F = volar/caminar (gravedad + colisión de terreno), R = prototipo de bici, Espacio = saltar
```

Empaquetado (sin datos del juego): `cd build && cpack -G DEB`.

## Herramientas

Python 3 con Pillow basta para todo lo que hay en `tools/`. Python se usa sólo para investigación y conversión offline; el motor de ejecución es C++ (la discusión de lenguaje, incluido un posible paso a Rust, está en [`docs/es/architecture.md`](docs/es/architecture.md)).

| Herramienta | Para qué sirve |
|-------------|----------------|
| `tools/unpack_ie.py` | Descomprime los contenedores `IE` de una carpeta de disco extraída |
| `tools/tex_dump.py`, `tools/tex_export.py` | Inspeccionan y exportan texturas (`gs.py` tiene las tablas de swizzle del GS) |
| `tools/vif.py` | Decodificador mínimo de paquetes VIF |
| `tools/scene.py` | Recorredor del grafo de escena de archivos `.NGP`; `instances()` / `Owners` dan las transformaciones por hoja y el filtro de detalle fino |
| `tools/extract_model.py` | Genera un `.mdl` con textura (malla + materiales + paletas + color/alfa de vértice) de un grupo de modelo o de nivel |
| `tools/batch_models.py` | Ejecuta `extract_model.py` sobre todos los grupos completos de una carpeta (p. ej. todos los niveles) |
| `tools/pts_path.py` | Lee un `.PTS` de nivel (grafo de puntos del recorrido) y exporta sus puntos / la línea de carrera principal |
| `tools/contact_sheet.py` | Renderiza con `dhview` todos los `.mdl` de una carpeta en una única hoja de contacto etiquetada |
| `tools/assemble_bike.py` | Ensambla una bici a partir de piezas `.mdl` de cuadro + manillar/horquilla + rueda (bujes medidos, horquilla inclinada) |
| `tools/vudis.py` | Desensamblador del microcódigo VU1 |
| `tools/extract_mesh.py`, `tools/extract_points.py` | Extraen vértices/triángulos de archivos `.NGP` |
| `tools/preview_msh.py` | Vista previa 2D rápida de una malla extraída |
| `tools/ghidra/` | Scripts de Ghidra en modo headless (xrefs de cadenas, decompilación de funciones) |

La ingeniería inversa usa [Ghidra](https://ghidra-sre.org/) con la extensión comunitaria [ghidra-emotionengine-reloaded](https://github.com/chaoticgd/ghidra-emotionengine-reloaded) (lenguaje `r5900:LE:32:default`).

Flujo típico:

```sh
7z x "Downhill Domination.iso" -oiso_extract
python3 tools/unpack_ie.py iso_extract unpacked
python3 tools/tex_export.py unpacked/SHELL/BIKESHOP.TEX out_textures
```

## Contribuir

Lee primero [`CONTRIBUTING.md`](CONTRIBUTING.md) y [`SECURITY.md`](SECURITY.md) (ambos bilingües).

Las contribuciones son muy bienvenidas, en especial:

- Ingeniería inversa: formatos (`.PTS`, `.RST`, `.REP`, `.BNK`, `.SKX`, ...), microcódigo VU1, lógica del juego.
- Renderizado y trabajo de motor en C++.
- Pruebas con otras versiones PAL/NTSC del juego (hasta ahora sólo se ha examinado la compilación PAL `SLES_522.02`).
- Documentación de todo lo que descubras.

Reglas básicas:

1. Nunca subas datos del juego, ejecutables, assets extraídos ni salida del decompilador.
2. Documenta los hallazgos en `docs/en/formats.md` y `docs/es/formats.md` (en ambos idiomas, si puedes) con tus propias palabras (estructuras y significado de los campos, no código copiado).
3. Mantén las herramientas pequeñas y ejecutables; una comprobación corta vale más que una explicación larga.

## Agradecimientos

Gracias a las comunidades de PCSX2 y Ghidra, y a todas las personas que han documentado el hardware de la PS2 (GS, VIF, VU) a lo largo de los años.

## Autoría

Creado y mantenido por **Pedro Soto**, con contribuciones de la comunidad (ver [`AUTHORS`](AUTHORS)).

## Licencia

Copyright (C) 2026 Pedro Soto y las personas que contribuyen a DownHill-Port-PC.

El código de este repositorio se publica bajo la **Licencia Pública General de GNU v3.0 o posterior** (ver [`LICENSE`](LICENSE)). Las contribuciones se aceptan bajo la misma licencia.

Esta licencia cubre únicamente el código y la documentación de este repositorio. Los assets, el código y las marcas del juego pertenecen a sus respectivos titulares y no están incluidos.
