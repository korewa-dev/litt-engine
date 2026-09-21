// Litt Engine - precompiled shader artifact loading
#include "litt_shader_artifacts.h"

#include <cstring>
#include <fstream>
#include <limits>

namespace litt {
namespace {

bool read_binary(const std::string& path, std::vector<uint8_t>& bytes) {
    if (path.empty()) return false;
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    file.seekg(0, std::ios::end);
    const std::streamoff size = file.tellg();
    if (size <= 0 || static_cast<uint64_t>(size) > kMaxShaderArtifactBytes) return false;
    file.seekg(0, std::ios::beg);
    bytes.resize(static_cast<size_t>(size));
    return static_cast<bool>(file.read(reinterpret_cast<char*>(bytes.data()), size));
}

} // namespace

bool load_spirv_artifact(const std::string& path, std::vector<uint32_t>& out_words) {
    std::vector<uint8_t> bytes;
    if (!read_binary(path, bytes) || bytes.size() < 20u ||
        bytes.size() % sizeof(uint32_t) != 0u) {
        return false;
    }
    uint32_t magic = 0;
    std::memcpy(&magic, bytes.data(), sizeof(magic));
    if (magic != 0x07230203u) return false;

    std::vector<uint32_t> words(bytes.size() / sizeof(uint32_t));
    std::memcpy(words.data(), bytes.data(), bytes.size());
    out_words.swap(words);
    return true;
}

bool load_dxil_artifact(const std::string& path, std::vector<uint8_t>& out_bytes) {
    std::vector<uint8_t> bytes;
    if (!read_binary(path, bytes) || bytes.size() < 4u) return false;
    // DXIL is stored in a DXBC container.
    if (std::memcmp(bytes.data(), "DXBC", 4) != 0) return false;
    out_bytes.swap(bytes);
    return true;
}

} // namespace litt
