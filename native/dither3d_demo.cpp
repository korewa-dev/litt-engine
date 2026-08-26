// Dither3D Demo Application
// Demonstrates the Dither3D integration in Litt Engine
//
// This is a simplified demo that shows how to use the Dither3D API.
// Full renderer integration requires Vulkan/DirectX backend.

#include <iostream>
#include <memory>
#include "littcore/litt.h"
#include "littcore/litt_renderer.h"
#include "littcore/litt_dither.h"

using namespace litt;

int main(int argc, char* argv[]) {
    std::cout << "========================================" << std::endl;
    std::cout << "  Litt Engine - Dither3D Demo" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    // Create dither asset manager
    DitherAssetManager assets;
    assets.generate_textures();

    std::cout << "Dither3D Asset Manager initialized" << std::endl;
    std::cout << "  Generated 4 pattern sizes:" << std::endl;

    // Test loading textures
    const char* patternNames[] = {"1x1", "2x2", "4x4", "8x8"};
    for (int i = 0; i < 4; ++i) {
        auto pattern = static_cast<DitherPattern>(i);
        const auto& tex = assets.get_texture(pattern);
        std::cout << "    " << patternNames[i] << ": "
                  << tex.width << "x" << tex.height
                  << "x" << tex.depth
                  << " (" << tex.data.size() << " bytes)" << std::endl;
    }

    std::cout << std::endl;
    std::cout << "Ramp texture: " 
              << assets.get_ramp().width << "x" 
              << assets.get_ramp().height
              << " (" << assets.get_ramp().data.size() << " bytes)" << std::endl;

    std::cout << std::endl;
    std::cout << "Dither3D API Test:" << std::endl;

    // Test DitherMaterial
    DitherMaterial mat;
    mat.enabled = true;
    mat.color_mode = DitherColorMode::Grayscale;
    mat.pattern = DitherPattern::P8x8;
    mat.scale = 5.0f;
    mat.size_variability = 0.0f;
    mat.contrast = 1.0f;
    mat.input_exposure = 1.0f;
    mat.input_offset = 0.0f;

    auto uniforms = mat.to_uniforms();
    std::cout << "  Material uniforms:" << std::endl;
    std::cout << "    enabled = " << uniforms.dither_enabled << std::endl;
    std::cout << "    scale = " << uniforms.dither_scale << std::endl;
    std::cout << "    color_mode = " << uniforms.dither_color_mode << std::endl;
    std::cout << "    pattern = " << uniforms.dither_pattern << std::endl;

    std::cout << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "  Demo completed successfully!" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;
    std::cout << "To run the full graphical demo:" << std::endl;
    std::cout << "  1. Install Vulkan SDK" << std::endl;
    std::cout << "  2. Build with: cmake -DVULKAN_SDK=... -B build && cmake --build build" << std::endl;
    std::cout << "  3. Run: ./bin/dither3d_demo" << std::endl;
    std::cout << std::endl;
    std::cout << "Shader files:" << std::endl;
    std::cout << "  shaders/dither3d/include.glsl" << std::endl;
    std::cout << "  shaders/dither3d/mesh.vert.glsl" << std::endl;
    std::cout << "  shaders/dither3d/mesh.frag.glsl" << std::endl;
    std::cout << std::endl;
    std::cout << "Documentation:" << std::endl;
    std::cout << "  docs/rendering/dither3d.md" << std::endl;
    std::cout << "  docs/rendering/dither3d-checklist.md" << std::endl;
    std::cout << "  docs/rendering/dither3d-summary.md" << std::endl;

    return 0;
}
