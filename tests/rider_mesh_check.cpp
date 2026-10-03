#include "../src/rider_mesh.hpp"
#include "../src/ngp.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

namespace fs = std::filesystem;
struct ModelRef { uint32_t kind; size_t lo, hi; };
static uint32_t u32(const std::vector<uint8_t>& d, size_t o) { uint32_t v; std::memcpy(&v, d.data() + o, 4); return v; }
static uint16_t u16(const std::vector<uint8_t>& d, size_t o) { uint16_t v; std::memcpy(&v, d.data() + o, 2); return v; }
static bool ptr(const std::vector<uint8_t>& d, size_t o, size_t& out) {
    if (o + 4 > d.size()) return false; const uint32_t p = u32(d, o);
    if (p < ngp::kBase || p - ngp::kBase >= d.size()) return false; out = p - ngp::kBase; return true;
}
static bool readFile(const fs::path& p, std::vector<uint8_t>& d) {
    std::ifstream f(p, std::ios::binary); if (!f) return false;
    d.assign(std::istreambuf_iterator<char>(f), {}); return true;
}
static bool buildTextureResolver(const fs::path& base, const std::vector<uint8_t>& ngp, rider_mesh::TextureResolver& out) {
    std::vector<uint8_t> ptrData, texData, rtxData;
    if (!readFile(base.string() + ".PTR", ptrData) || !readFile(base.string() + ".TEX", texData) || !readFile(base.string() + ".RTX", rtxData)) return false;
    if (ptrData.size() < 4) return false;
    const uint32_t n0 = u32(ptrData, 0);
    if (n0 > (ptrData.size() - 4) / 4) return false;
    const size_t q = 4 + static_cast<size_t>(n0) * 4;
    if (q + 4 > ptrData.size()) return false;
    const uint32_t nt = u32(ptrData, q);
    if (nt > (ptrData.size() - q - 4) / 8) return false;
    const size_t texRefs = q + 4, tex0Count = texRefs + 4u * nt;
    if (tex0Count + 4 > ptrData.size()) return false;
    const uint32_t nc = u32(ptrData, tex0Count);
    if (nc != nt || nc > (ptrData.size() - tex0Count - 4) / 4) return false;
    const size_t tex0Refs = tex0Count + 4;
    std::map<uint16_t, bool> texIds;
    for (size_t p = static_cast<size_t>(u32(texData, 4)) * 16; p + 16 <= texData.size();) {
        const uint32_t next = u32(texData, p), id = u32(texData, p + 8);
        texIds[static_cast<uint16_t>(id)] = true;
        if (!next) break;
        const size_t step = static_cast<size_t>(next & ~3u) * 4;
        if (!step || p > texData.size() - step) break;
        p += step;
    }
    std::map<uint32_t, bool> clutIds;
    for (size_t p = 0; p + 16 <= rtxData.size();) {
        const uint32_t next = u32(rtxData, p), id = u32(rtxData, p + 8);
        clutIds[id >> 16] = true;
        if (!next) break;
        const size_t step = static_cast<size_t>(next & ~3u) * 4;
        if (!step || p > rtxData.size() - step) break;
        p += step;
    }
    for (uint32_t i = 0; i < nt; ++i) {
        const uint32_t texRef = u32(ptrData, texRefs + 4u * i), tex0Ref = u32(ptrData, tex0Refs + 4u * i);
        if (texRef + 2 > ptrData.size() || tex0Ref + 8 > ngp.size()) return false;
        uint64_t tex0; std::memcpy(&tex0, ngp.data() + tex0Ref, 8);
        const uint16_t id = u16(ptrData, texRef);
        const uint32_t psm = static_cast<uint32_t>((tex0 >> 20) & 63u), cbp = static_cast<uint32_t>((tex0 >> 37) & 0x3fffu);
        const bool decodable = (psm == 0x13 || psm == 0x14 || psm == 0x1b) && texIds.count(id) && clutIds.count(cbp);
        out.materials.push_back({id, cbp, decodable});
        const auto key = std::make_pair(id, cbp);
        if (decodable && std::find(out.firstKeys.begin(), out.firstKeys.end(), key) == out.firstKeys.end()) out.firstKeys.push_back(key);
    }
    return true;
}
static void findModels(const std::vector<uint8_t>& d, std::vector<ModelRef>& out) {
    std::map<uint32_t, ModelRef> unique;
    for (size_t o = 0; o + 0x30 <= d.size(); o += 4) {
        const uint32_t h = u32(d, o); const uint32_t type = h & 0x3f, kind = h >> 18;
        if (type != 25 || kind < 4000 || kind > 4499) continue;
        size_t group; if (!ptr(d, o + 0x2c, group) || group + 0x20 > d.size() || (u32(d, group) & 0x3f) != 1) continue;
        const uint16_t n = u16(d, group + 8);
        for (uint16_t i = 0; i < n && group + 0x24 + 4u * i <= d.size(); ++i) {
            size_t child; if (!ptr(d, group + 0x20 + 4u * i, child) || child + 8 > d.size() || (u32(d, child) & 0x3f) != 2) continue;
            const uint32_t count = u32(d, child + 4); size_t lo = d.size(), hi = child;
            for (uint32_t k = 0; k < count && k < 64 && child + 0x2c + 8u * k <= d.size(); ++k) {
                size_t q; if (ptr(d, child + 0x28 + 8u * k, q)) lo = std::min(lo, q);
            }
            if (lo < hi) unique.emplace(kind, ModelRef{kind, lo, hi});
        }
    }
    for (const auto& [_, m] : unique) out.push_back(m);
}
struct RefMesh { std::vector<float> v; std::vector<uint32_t> i; std::vector<uint8_t> ids; std::vector<float> w; };
static bool readMdl(const fs::path& p, RefMesh& out) {
    std::vector<uint8_t> d; if (!readFile(p, d) || d.size() < 16) return false;
    if (std::memcmp(d.data(), "DHM2", 4) && std::memcmp(d.data(), "DHM3", 4)) return false;
    uint32_t nt = u32(d, 4), nv = u32(d, 8), ni = u32(d, 12); size_t o = 16;
    for (uint32_t t = 0; t < nt; ++t) { if (o + 8 > d.size()) return false; const uint32_t w = u32(d, o), h = u32(d, o + 4); o += 8; if (w == 0 || h == 0 || (uint64_t)w * h * 4 > d.size() - o) return false; o += (size_t)w * h * 4; }
    if ((uint64_t)nv * 40 + (uint64_t)ni * 4 > d.size() - o) return false;
    out.v.resize((size_t)nv * 10); std::memcpy(out.v.data(), d.data() + o, out.v.size() * 4); o += out.v.size() * 4;
    out.i.resize(ni); std::memcpy(out.i.data(), d.data() + o, out.i.size() * 4); return true;
}
static bool readSkin(const fs::path& p, RefMesh& out) {
    std::vector<uint8_t> d; if (!readFile(p, d) || d.size() < 8 || std::memcmp(d.data(), "DHSK", 4)) return false;
    const uint32_t n = u32(d, 4); if (d.size() < 8 + (uint64_t)n * 16) return false;
    out.ids.resize((size_t)n * 3); out.w.resize((size_t)n * 3);
    for (uint32_t i = 0; i < n; ++i) { std::memcpy(out.ids.data() + 3u * i, d.data() + 8 + 16u * i, 3); std::memcpy(out.w.data() + 3u * i, d.data() + 12 + 16u * i, 12); }
    return true;
}
static float fieldDiff(const std::vector<float>& a, const std::vector<float>& b, size_t component) {
    float e = 0; const size_t n = std::min(a.size(), b.size()) / 10;
    for (size_t i = 0; i < n; ++i) e = std::max(e, std::fabs(a[10 * i + component] - b[10 * i + component]));
    return e;
}

int main(int argc, char** argv) {
    // Sin ruta personal: argv[1] (base sin extensión), o $DH_UNPACKED/LVL/ALP2, o <repo>/unpacked/LVL/ALP2; si no existe, se omite.
    const char* up = std::getenv("DH_UNPACKED");
    const fs::path base = argc > 1 ? fs::path(argv[1]) : (up ? fs::path(up) : fs::path(__FILE__).parent_path().parent_path() / "unpacked") / "LVL" / "ALP2";
    const fs::path ngpPath = base.extension() == ".NGP" ? base : fs::path(base.string() + ".NGP");
    if (!fs::exists(ngpPath)) { std::puts("rider_mesh_check SKIP: ALP2.NGP no encontrado"); return 0; }
    std::vector<uint8_t> ngp; if (!readFile(ngpPath, ngp)) return 2;
    rider_mesh::TextureResolver textures;
    if (!buildTextureResolver(base, ngp, textures)) {
        std::fprintf(stderr, "rider_mesh_check FAIL: no se pudo leer PTR/TEX/RTX\n");
        return 1;
    }
    std::vector<ModelRef> models;
    const fs::path root = fs::path(__FILE__).parent_path().parent_path();
    const fs::path out = fs::temp_directory_path() / "dh-rider-mesh-check"; fs::create_directories(out);
    const fs::path ranges = out / "ranges.txt";
    const std::string discover = "python3 " + (root / "tools/export_riders.py").string() + " " + base.string() + " " + out.string() + " >" + ranges.string();
    if (std::system(discover.c_str()) != 0) { std::fprintf(stderr, "rider_mesh_check FAIL: no se pudo descubrir export_riders.py\n"); return 1; }
    std::ifstream rangeFile(ranges);
    unsigned kind; char sep; size_t lo, hi;
    std::string text;
    while (std::getline(rangeFile, text)) {
        if (std::sscanf(text.c_str(), "%u %zx..%zx", &kind, &lo, &hi) == 3) models.push_back({kind, lo, hi});
    }
    if (models.size() != 13) { std::fprintf(stderr, "rider_mesh_check FAIL: se esperaban 13 modelos, hay %zu\n", models.size()); return 1; }
    size_t failed = 0;
    for (const ModelRef& ref : models) {
        const fs::path mdl = out / (std::to_string(ref.kind) + ".mdl"), skin = out / (std::to_string(ref.kind) + ".skin");
        const std::string command = "DH_RANGE=" + std::to_string(ref.lo) + "," + std::to_string(ref.hi) + " DH_SKIN=" + skin.string() +
            " python3 " + (root / "tools/extract_model.py").string() + " " + base.string() + " " + mdl.string() + " >/dev/null";
        if (std::system(command.c_str()) != 0) { std::fprintf(stderr, "%u python extractor failed\n", ref.kind); ++failed; continue; }
        RefMesh py; rider_mesh::Mesh native;
        const bool mdlOk = readMdl(mdl, py), skinOk = mdlOk && readSkin(skin, py), nativeOk = skinOk && rider_mesh::loadRange(ngp, ref.lo, ref.hi, native, &textures);
        const bool ok = mdlOk && skinOk && nativeOk;
        if (!ok) std::fprintf(stderr, "%u status mdl=%d skin=%d native=%d range=%zx..%zx\n", ref.kind, mdlOk, skinOk, nativeOk, ref.lo, ref.hi);
        if (!ok) { std::fprintf(stderr, "%u parse failed\n", ref.kind); ++failed; continue; }
        const float pos = std::max({fieldDiff(native.vertices, py.v, 0), fieldDiff(native.vertices, py.v, 1), fieldDiff(native.vertices, py.v, 2)});
        const float uv = std::max(fieldDiff(native.vertices, py.v, 3), fieldDiff(native.vertices, py.v, 4));
        float rgba = 0, tex = fieldDiff(native.vertices, py.v, 9); for (size_t c = 5; c < 9; ++c) rgba = std::max(rgba, fieldDiff(native.vertices, py.v, c));
        float weights = 0; for (size_t i = 0; i < native.weights.size() && i < py.w.size(); ++i) weights = std::max(weights, std::fabs(native.weights[i] - py.w[i]));
        size_t idsDiff = 0; for (size_t i = 0; i < native.boneIds.size() && i < py.ids.size(); ++i) idsDiff += native.boneIds[i] != py.ids[i];
        const size_t trisNative = native.indices.size() / 3, trisPy = py.i.size() / 3;
        std::printf("%u pos=%.8g uv=%.8g rgba=%.8g tex=%.8g ids=%zu weights=%.8g tris=%zu/%zu packets=%u\n", ref.kind, pos, uv, rgba, tex, idsDiff, weights, trisNative, trisPy, native.packetCount);
        if (native.vertices.size() != py.v.size() || native.boneIds.size() != py.ids.size() || native.weights.size() != py.w.size() || idsDiff || trisNative != trisPy || pos > 1e-6f || uv > 1e-6f || rgba > 1e-6f || weights > 1e-6f || tex > 1e-6f) ++failed;
    }
    if (failed) { std::fprintf(stderr, "rider_mesh_check FAIL: %zu modelos\n", failed); return 1; }
    std::puts("rider_mesh_check OK: 13 modelos"); return 0;
}
