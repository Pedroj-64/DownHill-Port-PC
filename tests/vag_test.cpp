// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Decodificador PS2 ADPCM y contenedores VAGp/VPK (src/vag.hpp) con datos SINTÉTICOS (sin material del juego): aritmética exacta (división C hacia 0, historial sin recortar, recorte a 16 bits), banderas, intercalado VPK y rechazo de entradas inválidas.
#include "../src/vag.hpp"
#include <cstdio>
#include <cstdlib>
#define CHECK(c) do { if (!(c)) { std::printf("FALLO %s:%d %s\n", __FILE__, __LINE__, #c); std::exit(1); } } while (0)
using Bytes = std::vector<uint8_t>;
// bloque de 16 B: shift/pred, bandera y 28 nibbles (n[i] con signo -8..7)
static Bytes blk(int shift, int pred, int flag, const int (&n)[28]) { Bytes b(16, 0); b[0] = (uint8_t)(pred << 4 | shift); b[1] = (uint8_t)flag; for (int i = 0; i < 28; i++) b[2 + i / 2] |= (uint8_t)((n[i] & 15) << (4 * (i & 1))); return b; }
static void put32le(Bytes& v, size_t o, uint32_t x) { for (int k = 0; k < 4; k++) v[o + k] = (uint8_t)(x >> (8 * k)); }
int main() {
    int z[28] = {}; vag::State st; int16_t out[28];
    // sin filtro: muestra = (n << 12) >> shift
    { int n[28] = {}; n[0] = 1; n[1] = -1; n[2] = 7; n[3] = -8; Bytes b = blk(0, 0, 0, n); vag::block(b.data(), st, out); CHECK(out[0] == 4096 && out[1] == -4096 && out[2] == 28672 && out[3] == -32768 && out[4] == 0); }
    { vag::State s2; int n[28] = {}; n[0] = 7; n[1] = -8; Bytes b = blk(2, 0, 0, n); vag::block(b.data(), s2, out); CHECK(out[0] == 7168 && out[1] == -8192); }   // shift 2: 7*4096>>2
    // filtro 1 (60/64) con división C hacia 0: h1 = +40 -> 37 (no 38); h1 = -40 -> -37 (no -38)
    { vag::State s2; int a[28] = {}; a[27] = 5; Bytes A = blk(9, 0, 0, a); vag::block(A.data(), s2, out); CHECK(out[27] == 40 && s2.h1 == 40);
      Bytes B = blk(12, 1, 0, z); vag::block(B.data(), s2, out); CHECK(out[0] == 37); }
    { vag::State s2; int a[28] = {}; a[27] = -5; Bytes A = blk(9, 0, 0, a); vag::block(A.data(), s2, out); CHECK(out[27] == -40);
      Bytes B = blk(12, 1, 0, z); vag::block(B.data(), s2, out); CHECK(out[0] == -37); }
    // el historial NO se recorta: 28672 + 28672*60/64 = 55552 se satura a 32767 pero el estado vale 55552, y la muestra siguiente (55552*60/64 = 52080) también satura (con historial recortado sería 30719)
    { vag::State s2; int a[28] = {}; a[27] = 7; Bytes A = blk(0, 0, 0, a); vag::block(A.data(), s2, out); CHECK(out[27] == 28672 && s2.h1 == 28672);
      int b[28] = {}; b[0] = 7; Bytes B = blk(0, 1, 0, b); vag::block(B.data(), s2, out); CHECK(out[0] == 32767 && out[1] == 32767 && out[2] == 32767); }
    // bandera >= 7 = silencio; filtro inválido (>= 5) = silencio
    { vag::State s2; int a[28]; for (int& x : a) x = 7; Bytes A = blk(0, 0, 7, a); vag::block(A.data(), s2, out); for (int16_t v : out) CHECK(v == 0); Bytes B = blk(0, 9, 0, a); vag::block(B.data(), s2, out); for (int16_t v : out) CHECK(v == 0); }
    // VAGp mono: cabecera 0x30 (BE), frecuencia en +0x10
    { Bytes f(0x30, 0); std::memcpy(f.data(), "VAGp", 4); f[0x13] = 0x44; f[0x12] = 0xac; int n[28] = {}; n[0] = 3; Bytes b = blk(12, 0, 0, n); f.insert(f.end(), b.begin(), b.end()); Bytes e = blk(0, 0, 1, z); f.insert(f.end(), e.begin(), e.end());
      vag::Track t; CHECK(vag::decodeVag(f, t) && t.rate == 44100 && t.channels == 1 && t.pcm.size() == 56 && t.pcm[0] == 3); Bytes bad(f.begin(), f.begin() + 8); CHECK(!vag::decodeVag(bad, t)); f[0] = 'X'; CHECK(!vag::decodeVag(f, t)); }
    // VPK estéreo: bloque 32 B = 16 B por canal, alternando; el historial continúa dentro de cada canal
    { Bytes f(0x800, 0); std::memcpy(f.data(), " KPV", 4); put32le(f, 8, 0x800); put32le(f, 12, 32); put32le(f, 16, 22050); put32le(f, 20, 2);
      int l1[28] = {}, r1[28] = {}, l2[28] = {}, r2[28] = {}; l1[0] = 1; r1[0] = -1; l2[0] = 2; r2[0] = -2;
      for (const Bytes& b : {blk(12, 0, 0, l1), blk(12, 0, 0, r1), blk(12, 0, 0, l2), blk(12, 0, 0, r2)}) f.insert(f.end(), b.begin(), b.end());
      vag::Track t; CHECK(vag::decodeVpk(f, t) && t.rate == 22050 && t.channels == 2 && t.frames() == 56);
      CHECK(t.pcm[0] == 1 && t.pcm[1] == -1 && t.pcm[2 * 28] == 2 && t.pcm[2 * 28 + 1] == -2);
      put32le(f, 20, 3); CHECK(!vag::decodeVpk(f, t)); put32le(f, 20, 2); put32le(f, 12, 24); CHECK(!vag::decodeVpk(f, t)); put32le(f, 12, 32); put32le(f, 16, 0); CHECK(!vag::decodeVpk(f, t)); }
    std::printf("vag_test OK\n"); return 0;
}
