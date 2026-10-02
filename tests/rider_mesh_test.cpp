#include "../src/rider_mesh.hpp"
#include <cstdio>
#include <cstring>
#include <vector>

static void word(std::vector<uint8_t>& b, uint32_t v) {
    const size_t o = b.size(); b.resize(o + 4); std::memcpy(b.data() + o, &v, 4);
}
static void f32(std::vector<uint8_t>& b, float v) {
    const size_t o = b.size(); b.resize(o + 4); std::memcpy(b.data() + o, &v, 4);
}
static uint32_t cmd(uint8_t op, uint8_t n, uint16_t imm = 0) { return (uint32_t(op) << 24) | (uint32_t(n) << 16) | imm; }

static std::vector<uint8_t> sample() {
    std::vector<uint8_t> b; word(b, cmd(0x6c, 16, 0xb5));
    for (int i = 0; i < 16; ++i) { f32(b, i == 0 ? 0.f : float(i)); f32(b, 0); f32(b, 0); f32(b, 0.75f); }
    word(b, cmd(0x6a, 16));
    for (int i = 0; i < 16; ++i) { b.push_back(0); b.push_back(127); b.push_back(0); }
    word(b, cmd(0x6c, 1, 0x20));
    for (int i = 0; i < 4; ++i) word(b, i == 2 ? 1u : 0u);
    word(b, cmd(0x62, 3)); b.push_back(0); b.push_back(1); b.push_back(2); b.push_back(0);
    word(b, cmd(0x6f, 3));
    uint16_t c = 31 | (15u << 5) | (7u << 10); for (int i = 0; i < 3; ++i) { const size_t o = b.size(); b.resize(o + 2); std::memcpy(b.data() + o, &c, 2); } b.resize((b.size() + 3) & ~size_t(3));
    word(b, cmd(0x65, 3)); for (int i = 0; i < 3; ++i) { const size_t o = b.size(); b.resize(o + 4); int16_t uv[2] = {int16_t(i * 4096), 0}; std::memcpy(b.data() + o, uv, 4); }
    word(b, cmd(0x17, 0));
    return b;
}

int main() {
    const auto data = sample(); rider_mesh::Mesh m;
    if (!rider_mesh::load(data, 0, m) || m.positionCount != 16 || m.vertices.size() != 30 || m.indices.size() != 3) { std::puts("rider_mesh_test: FAIL parse"); return 1; }
    if (m.boneIds.size() != 9 || m.weights.size() != 9 || m.boneIds[0] != 0 || m.weights[0] < 0.74f || m.vertices[3] != 0.0f || m.vertices[13] <= 0.0f) { std::puts("rider_mesh_test: FAIL decoded fields"); return 1; }
    for (size_t n = 0; n < data.size(); ++n) { std::vector<uint8_t> cut(data.begin(), data.begin() + n); rider_mesh::Mesh bad; if (rider_mesh::load(cut, 0, bad)) { std::puts("rider_mesh_test: FAIL truncation"); return 1; } }
    for (size_t i = 0; i < 500; ++i) { std::vector<uint8_t> fuzz = data; fuzz[(i * 37) % fuzz.size()] ^= uint8_t(1u << (i & 7)); rider_mesh::Mesh ignored; rider_mesh::load(fuzz, i % (fuzz.size() + 1), ignored); }
    std::puts("rider_mesh_test OK"); return 0;
}
