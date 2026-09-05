// Dither3D Renderer Integration
// Manages dither texture upload and shader binding
//
// This file is a stub that provides the interface.
// Full Vulkan implementation is in litt_dither_vulkan.cpp

#include "litt_dither.h"
#include "litt_renderer.h"
#include <cstring>

namespace litt {

// =============================================================================
// Dither Pass (abstract interface)
// =============================================================================

struct DitherPass {
    bool enabled = false;
    DitherMaterial material;
    DitherAssetManager assets;

    void* pipeline = nullptr;
    void* descriptor_set = nullptr;
    void* dither_tex_3d = nullptr;
    void* dither_ramp_2d = nullptr;

    void initialize() {
        assets.generate_textures();
        enabled = true;
    }

    void set_material(const DitherMaterial& mat) {
        material = mat;
    }
};

// =============================================================================
// Dither Manager (backend-agnostic)
// =============================================================================

class DitherManager {
public:
    DitherManager() = default;
    ~DitherManager() = default;

    // Initialize dithering system
    bool initialize() {
        assets_.generate_textures();
        return true;
    }

    // Upload textures to GPU (stub - implemented in litt_dither_vulkan.cpp)
    bool uploadTextures() {
        // Backend-specific implementation
        return true;
    }

    // Get the dither material
    const DitherMaterial& getMaterial() const { return material_; }
    DitherMaterial& getMaterial() { return material_; }

    // Set dither material
    void setMaterial(const DitherMaterial& mat) {
        material_ = mat;
    }

    // Get texture by pattern
    const DitherTexture& getTexture(DitherPattern pattern) const {
        return assets_.get_texture(pattern);
    }

    const DitherRampTexture& getRamp() const {
        return assets_.get_ramp();
    }

private:
    DitherMaterial material_;
    DitherAssetManager assets_;
};

} // namespace litt
