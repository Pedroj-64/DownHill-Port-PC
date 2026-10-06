// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Interfaz mínima de renderizado (Textura / Malla): la lógica del juego (piloto, bici…) sólo habla con esto, nunca con OpenGL, para que el hito 5 pueda
// cambiar de API (GL moderno, Vulkan, …) sin reescribirla. Implementación actual: gl_renderer.hpp. Formato de vértice = .mdl: 10 f32 (x y z u v r g b a tex).
#pragma once
#include <cstdint>
#include <memory>
#include <vector>

namespace gfx {
using TextureId = int;      // -1 = sin textura
using MeshId = int;
struct Texture { int w = 0, h = 0; std::vector<uint8_t> rgba; };
struct Quad { float x, y, w, h, r, g, b, a; };   // rectángulo 2D de interfaz: píxeles de pantalla, origen arriba a la izquierda, color RGBA
class Renderer {
public:
    virtual ~Renderer() = default;
    virtual TextureId createTexture(const Texture& t) = 0;
    // Malla indexada; `textures[i]` = textura de los vértices con tex == i. `dynamic` = los vértices cambian cada fotograma (piel por CPU).
    virtual MeshId createMesh(const std::vector<float>& verts10, const std::vector<uint32_t>& indices, const std::vector<TextureId>& textures, bool dynamic) = 0;
    virtual void updateVertices(MeshId m, const std::vector<float>& verts10) = 0;
    // `model` = 4x4 por columnas (convención OpenGL) aplicada a los vértices; `gain` = sobrebrillo PS2 (x2 en modelos que no son de nivel)
    virtual void draw(MeshId m, const float model[16], float gain) = 0;
    // Rectángulos 2D de color plano (HUD, texto provisional de text.hpp) sobre la pantalla de viewW x viewH píxeles.
    virtual void drawQuads2D(const std::vector<Quad>& quads, int viewW, int viewH) = 0;
};
}  // namespace gfx
