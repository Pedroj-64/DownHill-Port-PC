<!-- SPDX-FileCopyrightText: 2026 Pedro Soto -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Arquitectura y decisión de lenguaje

🇬🇧 [English version](../en/architecture.md)

## 1. Cómo encajan las piezas hoy

```
tu ISO ──7z──▶ iso_extract/ ──unpack_ie.py──▶ unpacked/ (.NGP .PTR .TEX .RTX ...)
                                                    │
                  herramientas offline (Python, tools/): decodifican formatos, recorren el
                  grafo de escena, generan .msh / .mdl
                                                    │
                                                    ▼
                                      dhview (C++20, SDL3 + OpenGL)
                           visor, cámara libre, modo caminar con colisión de terreno
```

- **Python es sólo herramienta de investigación y conversión.** Existe porque descubrir formatos exige iterar rápido (cambiar una regla, volver a renderizar, comparar). No es, ni será, el motor del juego.
- **El único código de ejecución es C++** (`src/main.cpp`, ~200 líneas hoy). Lee los `.mdl`/`.msh` que producen las herramientas.
- **Los formatos documentados (`docs/*/formats.md`) son el verdadero producto de la investigación.** No dependen del lenguaje: cualquier implementación, en cualquier lenguaje, puede escribirse a partir de ellos.

## 2. Arquitectura objetivo (qué significa "el port")

Un único programa nativo que, en el primer arranque, lee la imagen de disco del propio usuario, convierte lo que necesita a su caché (el equivalente de los `.mdl` de hoy) y juega desde ahí. Nunca se distribuyen datos del juego. Eso implica que el código de conversión (hoy en Python) debe **reescribirse en el lenguaje del motor** antes de que el port lo pueda usar alguien que no sea desarrollador. Capas previstas:

| Capa | Responsabilidad |
|------|-----------------|
| `platform` | ventana, entrada, dispositivo de audio, temporización (SDL3 o equivalente) |
| `render` | abstracción de GPU, dibujo de niveles/modelos, materiales, capas con alfa |
| `assets` | lector de imagen de disco, contenedor `IE`, parseo de `.NGP/.PTR/.TEX/.RTX`, grafo de escena, caché |
| `world` | grafo del nivel, LOD por distancia, rejilla de colisión, objetos de juego |
| `game` | bici, ciclista, física, IA, modos, menús, partidas guardadas |

## 3. Decisión de lenguaje (estado: **en evaluación, sin decidir**)

**DECIDIDO (2026-10-02): el motor es C++20; Rust queda descartado.** El texto de abajo se conserva como registro de la discusión.

**Idea registrada del mantenedor (2026-10-01):** quizá el motor convenga escribirlo en Rust (u otro lenguaje con buena gestión de memoria) en vez de C++, y debe comportarse con criterio en cualquier PC. Es pronto, así que se deja anotado para tenerlo en cuenta, no para actuar todavía.

**Qué aporta la elección y qué no**

| Objetivo | ¿Ayuda Rust? | Notas |
|----------|--------------|-------|
| Seguridad de memoria al parsear datos no confiables (archivos de una imagen de disco) | **Sí, mucho.** | Es el argumento más fuerte: el proyecto es sobre todo un parser de formatos binarios con offsets, punteros y tablas de relocalización. Las lecturas fuera de rango son la clase principal de errores. |
| Compilación multiplataforma (`.deb` en Linux, `.exe` en Windows) | Sí, algo | `cargo` es más simple que CMake + SDL3 del sistema + cabeceras OpenGL. C++ también puede ir bien con vcpkg/Conan. |
| "Sabe lo que hace en cualquier PC" (GPU, drivers, memoria) | **No por sí solo** | Es una propiedad del diseño, no una característica del lenguaje: detección de capacidades, abstracción de API gráfica con alternativa (Vulkan/Metal/DX12/GL), presupuesto de memoria para texturas y autoajuste de opciones. Hay que diseñarlo en cualquiera de los dos. |
| Ecosistema para este trabajo | Mixto | Rust: bindings de `sdl3` o `winit` + `wgpu` (API de GPU portable), `glam`, `zerocopy`/`bytemuck`. C++: SDL3, OpenGL, muchos ejemplos de formatos PS2. |
| Coste de cambiar | **El más bajo es ahora** | El motor es un archivo pequeño. Lo caro son los *parsers* (aún en Python), que de todos modos hay que reescribir. |

**Opciones**
1. **Seguir con C++20.** Mantener SDL3/OpenGL; endurecer los parsers con `span` con comprobación de límites, sanitizers (ASan/UBSan) y fuzzing.
2. **Motor en Rust.** Reescribir `dhview` y escribir los parsers nuevos directamente en Rust. Preferir `wgpu` por portabilidad y selección automática de backend.
3. **Híbrido.** Biblioteca núcleo en Rust (o C++) + front-end fino. No recomendado tan pronto: dos sistemas de compilación para un programa diminuto.

**Recomendación (visión del asistente, a confirmar por el mantenedor):** decidir *antes* de escribir el primer parser nativo, porque en ese momento el lenguaje queda fijado por la cantidad de código. Hacer un **spike** acotado en tiempo: portar el cargador de `.mdl` + modo caminar actuales (~200 líneas) a Rust con `wgpu` y comparar (a) líneas y claridad, (b) compilación/empaquetado en Linux y Windows, (c) tiempo de fotograma en una GPU modesta, (d) lo agradable que es escribir un lector con comprobación de límites para la relocalización `.PTR` y el grafo de escena `.NGP`. Si el spike no es claramente mejor, quedarse en C++20 con sanitizers y fuzzing. En ambos casos las herramientas Python siguen como implementación de referencia y oráculo de pruebas hasta que los parsers nativos coincidan byte a byte con su salida.

**Criterios de decisión** (para decidir con evidencia): seguridad de memoria de los parsers, tiempo hasta un nivel jugable, esfuerzo de empaquetado en Windows y Linux, margen de rendimiento en hardware modesto y facilidad para que colaboradores de la comunidad compilen y contribuyan.

## 4. Requisitos de "funciona bien en cualquier PC" (independientes del lenguaje)

- Detectar las capacidades de GPU/driver al arrancar; elegir backend y tamaño de textura según ellas; degradar en vez de fallar.
- Mantener un presupuesto de memoria configurable; las texturas son pequeñas (≤256², con paleta), así que conviene conservarlas con paleta o comprimidas en la GPU cuando se pueda.
- No depender nunca de comportamiento indefinido al leer archivos: todo offset y longitud leído de un archivo del juego se valida contra el tamaño del archivo antes de usarse.
- Simulación con paso fijo separada del renderizado, para que la tasa de fotogramas no cambie la jugabilidad.
- Un modo `--check` que imprima el hardware detectado y los ajustes elegidos, para que los informes de errores sean útiles.

## 5. Estrategia de pruebas durante una migración

Tratar las herramientas Python como oráculo: para cada nivel, el parser nativo debe producir los mismos datos de vértices/índices/texturas (hash del `.mdl`). Añadir esa comparación al CI en cuanto exista un parser nativo. Ningún dato del juego entra al repositorio; la comprobación se ejecuta en local sobre la imagen del propio colaborador.
