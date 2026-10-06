// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Implementación OpenGL (perfil de compatibilidad, matrices y arrays del cliente) de gfx::Renderer. Requiere un contexto GL actual y <SDL3/SDL_opengl.h> incluido antes.
#pragma once
#include "renderer.hpp"

namespace gfx {
class GLRenderer : public Renderer {
    struct Mesh { std::vector<float> v; std::vector<uint32_t> i; std::vector<TextureId> tex; };
    std::vector<GLuint> texs; std::vector<Mesh> meshes;
public:
    TextureId createTexture(const Texture& t) override {
        GLuint id = 0; glGenTextures(1, &id); glBindTexture(GL_TEXTURE_2D, id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, t.w, t.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, t.rgba.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        texs.push_back(id); return (TextureId)texs.size() - 1;
    }
    MeshId createMesh(const std::vector<float>& v, const std::vector<uint32_t>& idx, const std::vector<TextureId>& tex, bool) override { meshes.push_back({v, idx, tex}); return (MeshId)meshes.size() - 1; }
    void updateVertices(MeshId m, const std::vector<float>& v) override { if (m >= 0 && (size_t)m < meshes.size() && v.size() == meshes[m].v.size()) meshes[m].v = v; }
    void draw(MeshId id, const float model[16], float gain) override {
        if (id < 0 || (size_t)id >= meshes.size()) return; const Mesh& m = meshes[id];
        glPushMatrix(); glMultMatrixf(model);
        glEnable(GL_TEXTURE_2D); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE); glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE); glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_PRIMARY_COLOR); glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE, gain);
        glEnableClientState(GL_VERTEX_ARRAY); glEnableClientState(GL_TEXTURE_COORD_ARRAY); glEnableClientState(GL_COLOR_ARRAY);
        glVertexPointer(3, GL_FLOAT, 40, m.v.data()); glTexCoordPointer(2, GL_FLOAT, 40, m.v.data() + 3); glColorPointer(4, GL_FLOAT, 40, m.v.data() + 5);
        for (size_t t = 0; t + 2 < m.i.size();) {          // tandas con la misma textura
            int tid = (int)m.v[m.i[t] * 10 + 9]; size_t e = t;
            while (e + 2 < m.i.size() && (int)m.v[m.i[e] * 10 + 9] == tid) e += 3;
            glBindTexture(GL_TEXTURE_2D, tid >= 0 && (size_t)tid < m.tex.size() && m.tex[tid] >= 0 ? texs[m.tex[tid]] : 0);
            glDrawElements(GL_TRIANGLES, (GLsizei)(e - t), GL_UNSIGNED_INT, m.i.data() + t); t = e;
        }
        glPopMatrix();
    }
    void drawQuads2D(const std::vector<Quad>& q, int vw, int vh) override {
        glDisable(GL_TEXTURE_2D); glDisable(GL_DEPTH_TEST); glDisable(GL_FOG); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity(); glOrtho(0, vw, vh, 0, -1, 1); glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
        glBegin(GL_QUADS); for (const Quad& r : q) { glColor4f(r.r, r.g, r.b, r.a); glVertex2f(r.x, r.y); glVertex2f(r.x + r.w, r.y); glVertex2f(r.x + r.w, r.y + r.h); glVertex2f(r.x, r.y + r.h); } glEnd();
        glPopMatrix(); glMatrixMode(GL_PROJECTION); glPopMatrix(); glMatrixMode(GL_MODELVIEW); glEnable(GL_DEPTH_TEST); glDisable(GL_BLEND);
    }
};
}  // namespace gfx
