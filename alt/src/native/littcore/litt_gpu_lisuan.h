// LittGPU Lisuan Detection
// Lisuan Tech (砺算科技) GPU vendor detection and TrueGPU architecture support
// https://www.lisuantech.com/

#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace litt {

// =============================================================================
// Lisuan GPU Vendor IDs
// =============================================================================

// Lisuan PCI Vendor ID (example - would need real ID from hardware database)
constexpr uint32_t LISUAN_VENDOR_ID = 0x1D95; // Placeholder - check real HW ID

// Lisuan GPU product IDs
enum class LisuanGPU : uint32_t {
    UNKNOWN     = 0x0000,
    LX_7G100    = 0x0710,  // Consumer gaming GPU, 12GB GDDR6
    LX_7G200    = 0x0720,  // Next-gen consumer
    LX_PRO_100  = 0x1000,  // Professional/workstation
    LX_PRO_200  = 0x2000,  // Next-gen professional
};

// Lisuan TrueGPU architecture features
struct TrueGPUFeatures {
    bool gddr6_memory;          // GDDR6 memory support
    bool multi_display;         // Multi-display output
    bool hardware_encoding;     // Hardware video encoding
    bool hardware_decoding;     // Hardware video decoding
    bool ray_tracing;           // Hardware ray tracing support
    bool ai_accelerator;        // AI/ML acceleration (NPU-like)
    uint32_t max_display_outputs;
    uint32_t compute_units;
    uint32_t tensor_cores;
    uint32_t rt_cores;
    uint64_t vram_bytes;
};

// Lisuan GPU info
struct LisuanGPUInfo {
    LisuanGPU gpu_type;
    std::string gpu_name;
    std::string driver_version;
    TrueGPUFeatures features;
    uint32_t pci_vendor_id;
    uint32_t pci_device_id;
    uint32_t revision_id;
    uint32_t subsystem_id;
};

// =============================================================================
// Lisuan GPU Detection API
// =============================================================================

class LisuanGPUDetector {
public:
    // Detect Lisuan GPUs in the system
    static bool is_lisuan_gpu_present();
    
    // Get number of Lisuan GPUs
    static uint32_t get_gpu_count();
    
    // Get GPU info by index
    static bool get_gpu_info(uint32_t index, LisuanGPUInfo& info);
    
    // Get the primary (first) Lisuan GPU
    static bool get_primary_gpu(LisuanGPUInfo& info);
    
    // Check if a specific Lisuan GPU is present
    static bool has_gpu(LisuanGPU gpu);
    
    // Get the highest-performance Lisuan GPU
    static bool get_best_gpu(LisuanGPUInfo& info);
    
    // Get TrueGPU architecture features
    static TrueGPUFeatures get_truegpu_features(const LisuanGPUInfo& info);
    
    // Check if TrueGPU architecture supports a feature
    static bool supports_truegpu_feature(const LisuanGPUInfo& info, const std::string& feature);
    
    // Get driver version string
    static std::string get_driver_version();
    
    // Get GPU utilization (0-100%)
    static float get_gpu_utilization(const LisuanGPUInfo& info);
    
    // Get VRAM usage in bytes
    static uint64_t get_vram_usage(const LisuanGPUInfo& info);
    
    // Get GPU temperature in Celsius
    static float get_temperature(const LisuanGPUInfo& info);
    
    // Get GPU clock speed in MHz
    static uint32_t get_gpu_clock(const LisuanGPUInfo& info);
    
    // Get memory clock speed in MHz
    static uint32_t get_memory_clock(const LisuanGPUInfo& info);
    
    // Check if Lisuan GPU supports Vulkan
    static bool supports_vulkan(const LisuanGPUInfo& info);
    
    // Check if Lisuan GPU supports DirectX 12
    static bool supports_dx12(const LisuanGPUInfo& info);
    
    // Check if Lisuan GPU supports OpenGL
    static bool supports_opengl(const LisuanGPUInfo& info);
    
    // Get Vulkan API version supported
    static std::string get_vulkan_version(const LisuanGPUInfo& info);
    
    // Get DirectX feature level
    static std::string get_dx_feature_level(const LisuanGPUInfo& info);
    
    // Get GPU memory bandwidth in GB/s
    static float get_memory_bandwidth(const LisuanGPUInfo& info);
    
    // Get GPU TDP in watts
    static uint32_t get_tdp(const LisuanGPUInfo& info);
    
    // Check if Lisuan GPU is integrated or discrete
    static bool is_discrete(const LisuanGPUInfo& info);
    
    // Get GPU bus width in bits
    static uint32_t get_bus_width(const LisuanGPUInfo& info);
    
    // Get GPU memory type (GDDR5, GDDR6, GDDR6X, etc.)
    static std::string get_memory_type(const LisuanGPUInfo& info);
    
    // Enable/disable Lisuan-specific optimizations
    static void set_optimization_enabled(bool enabled);
    static bool is_optimization_enabled();
    
    // Set GPU power profile
    enum class PowerProfile {
        POWER_SAVING,
        BALANCED,
        PERFORMANCE,
        MAX_PERFORMANCE
    };
    static void set_power_profile(const LisuanGPUInfo& info, PowerProfile profile);
    static PowerProfile get_power_profile(const LisuanGPUInfo& info);
    
    // Set GPU fan speed (0-100%)
    static void set_fan_speed(const LisuanGPUInfo& info, uint32_t percent);
    static uint32_t get_fan_speed(const LisuanGPUInfo& info);
    
    // GPU overclocking (if supported)
    static bool supports_overclocking(const LisuanGPUInfo& info);
    static void set_gpu_overclock(const LisuanGPUInfo& info, int32_t mhz_offset);
    static void set_memory_overclock(const LisuanGPUInfo& info, int32_t mhz_offset);
    static int32_t get_gpu_overclock(const LisuanGPUInfo& info);
    static int32_t get_memory_overclock(const LisuanGPUInfo& info);
    
    // Reset overclock to defaults
    static void reset_overclock(const LisuanGPUInfo& info);
    
    // Get Lisuan GPU serial number
    static std::string get_serial_number(const LisuanGPUInfo& info);
    
    // Get Lisuan GPU BIOS version
    static std::string get_bios_version(const LisuanGPUInfo& info);
    
    // Check if Lisuan GPU supports specific display outputs
    static bool supports_hdmi(const LisuanGPUInfo& info);
    static bool supports_displayport(const LisuanGPUInfo& info);
    static bool supports_usbc_display(const LisuanGPUInfo& info);
    static uint32_t get_hdmi_version(const LisuanGPUInfo& info);
    static uint32_t get_displayport_version(const LisuanGPUInfo& info);
    
    // Get maximum resolution supported
    static void get_max_resolution(const LisuanGPUInfo& info, uint32_t& width, uint32_t& height, uint32_t& refresh_rate);
    
    // Lisuan AI accelerator (NPU-like) support
    static bool has_ai_accelerator(const LisuanGPUInfo& info);
    static uint32_t get_ai_tops(const LisuanGPUInfo& info); // Tera Operations Per Second
    static bool supports_on_device_llm(const LisuanGPUInfo& info);
    
    // Lisuan TrueGPU-specific rendering features
    static bool supports_truegpu_upscale(const LisuanGPUInfo& info); // Lisuan's FSR-like upscaling
    static bool supports_truegpu_framegen(const LisuanGPUInfo& info); // Lisuan's frame generation
    static bool supports_truegpu_denoiser(const LisuanGPUInfo& info); // Lisuan's ray tracing denoiser
    
    // Get TrueGPU architecture version
    static uint32_t get_truegpu_version(const LisuanGPUInfo& info); // 1, 2, 3, etc.
    
    // Lisuan GPU multi-GPU support
    static bool supports_multi_gpu();
    static uint32_t get_multi_gpu_count();
    static bool enable_multi_gpu(bool enabled);
    static bool is_multi_gpu_enabled();
    
    // Lisuan GPU debugging/profiling
    static void begin_gpu_profile(const std::string& name);
    static void end_gpu_profile(const std::string& name);
    static float get_gpu_profile_time(const std::string& name);
    
    // Lisuan GPU memory management
    static bool allocate_vram(const LisuanGPUInfo& info, uint64_t size, void*& out_ptr);
    static void free_vram(const LisuanGPUInfo& info, void* ptr);
    static uint64_t get_total_vram(const LisuanGPUInfo& info);
    static uint64_t get_free_vram(const LisuanGPUInfo& info);
    
    // Lisuan GPU synchronization
    static void gpu_wait_idle(const LisuanGPUInfo& info);
    static void gpu_flush(const LisuanGPUInfo& info);
    
    // Lisuan GPU error handling
    static std::string get_last_error();
    static void clear_last_error();
    static bool has_error();
    
    // Lisuan GPU logging
    static void set_logging_enabled(bool enabled);
    static bool is_logging_enabled();
    static void set_log_level(uint32_t level); // 0=none, 1=error, 2=warn, 3=info, 4=debug
    static uint32_t get_log_level();
    
private:
    static bool s_optimization_enabled;
    static bool s_logging_enabled;
    static uint32_t s_log_level;
    static std::string s_last_error;
};

// =============================================================================
// Lisuan GPU Initialization Helper
// =============================================================================

class LisuanGPUInitializer {
public:
    // Initialize Lisuan GPU subsystem
    static bool initialize();
    static void shutdown();
    static bool is_initialized();
    
    // Get initialization error message
    static std::string get_init_error();
    
    // Get the best available GPU backend for Lisuan
    enum class Backend {
        VULKAN,
        DIRECTX12,
        OPENGL
    };
    static Backend get_best_backend(const LisuanGPUInfo& info);
    
    // Create a GPU device for Lisuan
    static class IGPUDevice* create_device(const LisuanGPUInfo& info, Backend backend);
    
    // Get recommended backend for a Lisuan GPU
    static Backend get_recommended_backend(const LisuanGPUInfo& info);
    
private:
    static bool s_initialized;
    static std::string s_init_error;
};

} // namespace litt
