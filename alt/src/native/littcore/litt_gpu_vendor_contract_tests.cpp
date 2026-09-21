#include "litt_gpu_lisuan.h"

#include <cassert>
#include <cstdint>
#include <string>

using namespace litt;

int main() {
    assert(LISUAN_VENDOR_ID == 0u);
    assert(!LisuanGPUDetector::is_lisuan_gpu_present());
    assert(LisuanGPUDetector::get_gpu_count() == 0u);

    LisuanGPUInfo info{};
    info.gpu_type = LisuanGPU::LX_7G100;
    info.gpu_name = "caller supplied";
    info.driver_version = "caller supplied";
    info.features.gddr6_memory = true;
    info.features.ray_tracing = true;
    info.features.ai_accelerator = true;
    info.features.vram_bytes = 16ull * 1024ull * 1024ull * 1024ull;

    LisuanGPUInfo queried{};
    assert(!LisuanGPUDetector::get_gpu_info(0, queried));
    assert(queried.gpu_type == LisuanGPU::UNKNOWN);
    assert(queried.gpu_name.empty());
    assert(queried.driver_version.empty());

    assert(LisuanGPUDetector::get_driver_version() == "UNKNOWN");
    assert(!LisuanGPUDetector::supports_vulkan(info));
    assert(!LisuanGPUDetector::supports_dx12(info));
    assert(!LisuanGPUDetector::supports_opengl(info));
    assert(LisuanGPUDetector::get_vulkan_version(info) == "UNKNOWN");
    assert(LisuanGPUDetector::get_dx_feature_level(info) == "UNKNOWN");

    const TrueGPUFeatures features = LisuanGPUDetector::get_truegpu_features(info);
    assert(!features.gddr6_memory);
    assert(!features.ray_tracing);
    assert(!features.ai_accelerator);
    assert(features.vram_bytes == 0u);
    assert(!LisuanGPUDetector::supports_truegpu_feature(info, "ray_tracing"));
    assert(!LisuanGPUDetector::has_ai_accelerator(info));
    assert(!LisuanGPUDetector::supports_on_device_llm(info));
    assert(!LisuanGPUDetector::supports_truegpu_upscale(info));
    assert(!LisuanGPUDetector::supports_truegpu_framegen(info));
    assert(!LisuanGPUDetector::supports_truegpu_denoiser(info));
    assert(LisuanGPUDetector::get_truegpu_version(info) == 0u);

    assert(!LisuanGPUDetector::is_discrete(info));
    assert(LisuanGPUDetector::get_bus_width(info) == 0u);
    assert(LisuanGPUDetector::get_memory_type(info) == "Unknown");
    assert(LisuanGPUDetector::get_total_vram(info) == 0u);
    assert(LisuanGPUDetector::get_free_vram(info) == 0u);
    assert(!LisuanGPUDetector::supports_hdmi(info));
    assert(!LisuanGPUDetector::supports_displayport(info));

    uint32_t width = 1, height = 1, refresh = 1;
    LisuanGPUDetector::get_max_resolution(info, width, height, refresh);
    assert(width == 0u && height == 0u && refresh == 0u);

    assert(LisuanGPUInitializer::get_best_backend(info) ==
           LisuanGPUInitializer::Backend::UNAVAILABLE);
    assert(LisuanGPUInitializer::get_recommended_backend(info) ==
           LisuanGPUInitializer::Backend::UNAVAILABLE);
    assert(LisuanGPUInitializer::create_device(
        info, LisuanGPUInitializer::Backend::UNAVAILABLE) == nullptr);

    return 0;
}
