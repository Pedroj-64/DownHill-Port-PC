// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Paso 0 del loader nativo (docs/native-loader-plan.md): lector ISO9660 (sectores de 2048 B) sobre la imagen del jugador, sin cargarla entera. Lista y extrae archivos; no necesita 7z ni herramientas externas.
// Solo lectura. Entrada no fiable: todo extent se valida contra el tamaño de la imagen, la recursión tiene límite de profundidad y detecta ciclos. El sufijo de versión «;1» y el punto final de «NOMBRE.» se quitan.
// No se usan extensiones Joliet/Rock Ridge (los nombres de este disco son 8.3 en mayúsculas). Los datos del juego nunca se copian al repo (regla 4): se leen de la imagen local.
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <vector>
#if defined(_WIN32)
#define DH_FSEEK _fseeki64
#else
#define DH_FSEEK fseeko
#endif

namespace iso9660 {
inline constexpr uint32_t kSector = 2048;
struct Entry { std::string path; uint32_t lba = 0, size = 0; bool dir = false; };

class Image {
public:
    Image() = default; Image(const Image&) = delete; Image& operator=(const Image&) = delete;
    ~Image() { if (f_) std::fclose(f_); }
    bool open(const std::string& path) {
        f_ = std::fopen(path.c_str(), "rb"); if (!f_) return false;
        if (DH_FSEEK(f_, 0, SEEK_END) != 0) return false; total_ = (uint64_t)ftellInt(); if (total_ < (uint64_t)17 * kSector) return false;
        std::vector<uint8_t> pvd; if (!readAt(16ull * kSector, kSector, pvd) || pvd[0] != 1 || std::memcmp(&pvd[1], "CD001", 5) != 0) return false;   // descriptor primario
        rootLba_ = u32le(&pvd[156 + 2]); rootSize_ = u32le(&pvd[156 + 10]); return rootSize_ >= 34 && (uint64_t)rootLba_ * kSector + rootSize_ <= total_;
    }
    // Todos los archivos y directorios (rutas con «/», sin «;1»), en el orden del disco.
    bool list(std::vector<Entry>& out) const { std::set<uint32_t> seen; return walk(rootLba_, rootSize_, "", 0, seen, out); }
    bool read(const Entry& e, std::vector<uint8_t>& data) const { if (e.dir) return false; return readAt((uint64_t)e.lba * kSector, e.size, data); }
    // Lee `n` bytes desde el offset `off` del archivo (acceso por trozos para archivos grandes).
    bool readRange(const Entry& e, uint64_t off, size_t n, std::vector<uint8_t>& data) const { if (e.dir || off + n > e.size) return false; return readAt((uint64_t)e.lba * kSector + off, n, data); }
    uint64_t totalBytes() const { return total_; }
private:
    FILE* f_ = nullptr; uint64_t total_ = 0; uint32_t rootLba_ = 0, rootSize_ = 0;
    static uint32_t u32le(const uint8_t* p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
    long long ftellInt() const {
#if defined(_WIN32)
        return _ftelli64(f_);
#else
        return (long long)ftello(f_);
#endif
    }
    bool readAt(uint64_t off, size_t n, std::vector<uint8_t>& out) const {
        if (!f_ || off > total_ || n > total_ - off) return false; out.resize(n); if (DH_FSEEK(f_, (long long)off, SEEK_SET) != 0) return false;
        return n == 0 || std::fread(out.data(), 1, n, f_) == n;
    }
    bool walk(uint32_t lba, uint32_t size, const std::string& prefix, int depth, std::set<uint32_t>& seen, std::vector<Entry>& out) const {
        if (depth > 16 || !seen.insert(lba).second) return false;   // ciclo o demasiado profundo
        std::vector<uint8_t> d; if (!readAt((uint64_t)lba * kSector, size, d)) return false;
        size_t o = 0;
        while (o < d.size()) {
            const uint8_t len = d[o];
            if (len == 0) { o = (o / kSector + 1) * kSector; continue; }   // el relleno hasta el final del sector
            if (len < 34 || o + len > d.size()) return false;
            const uint8_t nameLen = d[o + 32]; if (33u + nameLen > len) return false;
            const uint32_t elba = u32le(&d[o + 2]), esize = u32le(&d[o + 10]); const bool isDir = (d[o + 25] & 2) != 0;
            if (!(nameLen == 1 && (d[o + 33] == 0 || d[o + 33] == 1))) {   // se saltan «.» y «..»
                std::string name((const char*)&d[o + 33], nameLen); const size_t semi = name.find(';'); if (semi != std::string::npos) name.resize(semi);
                if (!isDir && name.size() > 1 && name.back() == '.') name.pop_back();   // «NOMBRE.» = sin extensión (los extractores lo dejan como «NOMBRE»)
                if (name.find('/') != std::string::npos || name.find('\\') != std::string::npos) return false;
                Entry e; e.path = prefix.empty() ? name : prefix + "/" + name; e.lba = elba; e.size = esize; e.dir = isDir;
                if ((uint64_t)elba * kSector + esize > total_ && !isDir) return false;
                out.push_back(e);
                if (isDir && !walk(elba, esize, e.path, depth + 1, seen, out)) return false;
            }
            o += len;
        }
        return true;
    }
};
}  // namespace iso9660
