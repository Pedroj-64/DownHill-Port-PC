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
| Paletas `.RTX` | Localizadas; el enlace textura–paleta aún se desconoce |
| Tabla de relocalización `.PTR` | Entendida (base `0xA00000`) |
| Geometría `.NGP` | Datos de vértices e índices de tira extraídos; topología/LOD/instancias sin resolver |
| Microcódigo VU1 | Localizado y desensamblado (`tools/vudis.py`); intérprete aún sin escribir |
| Modelos con textura | Primer modelo texturizado renderizado de forma nativa (`BOARBIKE`): materiales, paletas y UVs decodificados; faltan las texturas PSMT4 |
| Visor (`dhview`) | Visor SDL3 + OpenGL de puntos, triángulos y modelos con textura (`.mdl`) con cámara libre |
| Física, bici, IA, audio, menús | Sin empezar |

Las notas de formatos están en [`docs/es/formats.md`](docs/es/formats.md); la configuración de Ghidra y el flujo de investigación, en [`docs/es/research.md`](docs/es/research.md). Ambos también existen en inglés en [`docs/en/`](docs/en/).

## Hoja de ruta

1. Emular la ruta de vértices del VU1 (desempaquetado VIF + intérprete VU1 + parser GIF) para recuperar con exactitud los reinicios de tira, los UV y las transformaciones.
2. Recorrer el grafo de escena (nodos, LOD, instancias) y renderizar una pista limpia.
3. Texturas con paletas y materiales.
4. Modelo de la bici, animación, cámara y física de conducción.
5. Audio (VAG/BNK), video (PSS), menús y modos de juego, datos de guardado.
6. Empaquetado: compilaciones `.deb` y de Windows. El paquete **no** incluirá datos del juego; la aplicación extraerá los assets de tu imagen en el primer arranque.

## Compilación

Requisitos: compilador C++20, CMake ≥ 3.20, Ninja y los archivos de desarrollo de SDL3 y OpenGL.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/dhview ruta/a/malla.msh      # WASD + ratón, Shift = rápido, Esc = salir
```

Empaquetado (sin datos del juego): `cd build && cpack -G DEB`.

## Herramientas

Python 3 con Pillow basta para todo lo que hay en `tools/`.

| Herramienta | Para qué sirve |
|-------------|----------------|
| `tools/unpack_ie.py` | Descomprime los contenedores `IE` de una carpeta de disco extraída |
| `tools/tex_dump.py`, `tools/tex_export.py` | Inspeccionan y exportan texturas (`gs.py` tiene las tablas de swizzle del GS) |
| `tools/vif.py` | Decodificador mínimo de paquetes VIF |
| `tools/scene.py` | Recorredor del grafo de escena de archivos `.NGP` |
| `tools/extract_model.py` | Genera un `.mdl` con textura (malla + materiales + paletas) de un grupo de modelo |
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
