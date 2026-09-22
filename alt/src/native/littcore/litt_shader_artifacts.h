// Litt Engine - validated precompiled shader artifact loading
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace litt {

static constexpr size_t kMaxShaderArtifactBytes = 32u * 1024u * 1024u;

bool load_spirv_artifact(const std::string& path, std::vector<uint32_t>& out_words);
bool load_dxil_artifact(const std::string& path, std::vector<uint8_t>& out_bytes);

} // namespace litt
