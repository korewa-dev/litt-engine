#include "litt_shader_artifacts.h"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <vector>

using namespace litt;

int main() {
    const char* spv = "shader_contract.spv";
    {
        const uint32_t words[5] = {0x07230203u, 0x00010000u, 0u, 1u, 0u};
        std::ofstream out(spv, std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(words), sizeof(words));
    }
    std::vector<uint32_t> spirv;
    assert(load_spirv_artifact(spv, spirv));
    assert(spirv.size() == 5u && spirv[0] == 0x07230203u);

    const char* dxil = "shader_contract.dxil";
    {
        const uint8_t bytes[8] = {'D','X','B','C',1,2,3,4};
        std::ofstream out(dxil, std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(bytes), sizeof(bytes));
    }
    std::vector<uint8_t> dxil_bytes;
    assert(load_dxil_artifact(dxil, dxil_bytes));
    assert(dxil_bytes.size() == 8u);

    const char* bad = "shader_contract.bad";
    {
        std::ofstream out(bad, std::ios::binary | std::ios::trunc);
        out << "bad";
    }
    assert(!load_spirv_artifact(bad, spirv));
    assert(!load_dxil_artifact(bad, dxil_bytes));

    std::remove(spv);
    std::remove(dxil);
    std::remove(bad);
    return 0;
}
