// LittEngine - Main engine entry point
// Ties all subsystems together

#pragma once
#include <cstdint>
#include "litt_math.h"
#include "litt_ecs.h"
#include "litt_input.h"
#include "litt_scene.h"
#include "litt_renderer.h"
#include "litt_audio.h"
#include "litt_config.h"
#include "litt_profiler.h"
#include "litt_memory.h"

#include <string>
#include <memory>
#include <chrono>
#include <thread>
#include <atomic>
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <cmath>
#include <limits>

namespace litt {

// =============================================================================
// Engine Configuration
// =============================================================================

struct EngineConfig {
    std::string window_title = "Litt Engine";
    uint32_t width = 1920;
    uint32_t height = 1080;
    bool fullscreen = false;
    bool vsync = true;
    RenderBackend backend = RenderBackend::Vulkan;
    float target_fps = 60.0f;
    bool headless = true;
};

// =============================================================================
// Main Engine Class
// =============================================================================

class Engine {
public:
    Engine() = default;
    ~Engine() = default;
    
    // Non-copyable
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
    
    // =====================================================================
    // Initialization
    // =====================================================================
    
    bool initialize(const EngineConfig& config) {
        if (initialized_) shutdown();

        if (config.width == 0 || config.height == 0 ||
            config.width > 16384 || config.height > 16384 ||
            !std::isfinite(config.target_fps) || config.target_fps <= 0.0f ||
            config.target_fps > 1000.0f) {
            log_error("Invalid engine configuration");
            return false;
        }

        config_ = config;
        running_ = true;

        // Litt is headless-first. Interactive modes fail honestly until a
        // production renderer backend is wired into Renderer.
        if (!config_.headless &&
            !renderer_.initialize(config_.width, config_.height, config_.backend)) {
            running_ = false;
            log_error("Requested renderer backend is unavailable");
            return false;
        }

        audio_.init();
        create_default_scene();
        initialized_ = true;

        log_info("Engine initialized successfully");
        return true;
    }
    
    void shutdown() {
        if (!initialized_) {
            running_ = false;
            return;
        }

        log_info("Shutting down engine...");
        running_ = false;
        if (!config_.headless) renderer_.shutdown();
        audio_.shutdown();
        initialized_ = false;
        log_info("Engine shutdown complete");
    }
    
    // =====================================================================
    // Game Loop
    // =====================================================================
    
    void run() {
        if (!initialized_) {
            log_error("Engine::run called before initialize");
            return;
        }
        if (config_.headless) {
            run_headless();
            return;
        }
        
        auto last_time = std::chrono::high_resolution_clock::now();
        const float target_fps = config_.target_fps > 0.0f ? config_.target_fps : 60.0f;
        const float target_frame_time = 1.0f / target_fps;
        
        while (running_) {
            auto now = std::chrono::high_resolution_clock::now();
            float frame_time = std::chrono::duration<float>(now - last_time).count();
            last_time = now;
            
            if (frame_time > 0.1f) frame_time = 0.1f;
            
            input_.update();
            process_input();
            
            update(frame_time);
            render();

            Profiler::get_instance().update_fps();

            auto work_end = std::chrono::high_resolution_clock::now();
            float work_time = std::chrono::duration<float>(work_end - now).count();
            if (work_time < target_frame_time) {
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(
                        (int)((target_frame_time - work_time) * 1000)));
            }
        }
    }
    
    void run_headless() {
        if (!initialized_) {
            log_error("Engine::run_headless called before initialize");
            return;
        }
        using Clock = std::chrono::steady_clock;
        const float target_fps = config_.target_fps > 0.0f ? config_.target_fps : 60.0f;
        const float fixed_dt = 1.0f / target_fps;
        const auto tick = std::chrono::duration_cast<Clock::duration>(
            std::chrono::duration<float>(fixed_dt));
        auto next_tick = Clock::now();

        while (running_) {
            update(fixed_dt);
            Profiler::get_instance().update_fps();

            next_tick += tick;
            const auto now = Clock::now();
            if (next_tick > now) {
                std::this_thread::sleep_until(next_tick);
            } else if (now - next_tick > tick * 4) {
                // Do not try to replay an unbounded backlog after a stall.
                next_tick = now;
            }
        }
    }
    
    void stop() {
        running_ = false;
    }
    
    // =====================================================================
    // Accessors
    // =====================================================================
    
    World& ecs_world() { return ecs_world_; }
    Renderer& renderer() { return renderer_; }
    Input& input() { return input_; }
    AudioManager& audio() { return audio_; }
    SceneManager& scene_manager() { return scene_manager_; }
    
    const EngineConfig& get_config() const { return config_; }
    bool is_running() const { return initialized_ && running_; }
    bool is_initialized() const { return initialized_; }
    
    // =====================================================================
    // Scene Management
    // =====================================================================
    
    bool load_scene(const std::string& path) {
        (void)path;
        log_error("Scene loading is unavailable: Scene deserialization is not implemented");
        return false;
    }
    
    bool save_scene(const std::string& path) {
        (void)path;
        log_error("Scene saving is unavailable: Scene serialization is not implemented");
        return false;
    }
    
    // =====================================================================
    // Logging
    // =====================================================================
    
    void log_info(const std::string& msg) {
        std::cout << "[INFO] " << msg << std::endl;
    }
    
    void log_warning(const std::string& msg) {
        std::cerr << "[WARN] " << msg << std::endl;
    }
    
    void log_error(const std::string& msg) {
        std::cerr << "[ERROR] " << msg << std::endl;
    }
    
private:
    void create_default_scene() {
        Scene& scene = scene_manager_.createScene("Default");
        scene_manager_.setActiveScene("Default");
        
        // Scene already owns its root node.
        
        // Create camera
        RenderCamera cam;
        cam.position = Vec3(0, 5, -10);
        cam.target = Vec3::zero();
        cam.update();
        renderer_.add_camera(std::make_shared<RenderCamera>(cam));
        
        // Create light
        Light light;
        light.position = Vec3(5, 10, 5);
        light.color = Vec3::one();
        light.type = LightType::DIRECTIONAL;
        renderer_.add_light(std::make_shared<Light>(light));
    }
    
    void process_input() {
        if (input_.key_pressed(Key::Escape)) {
            running_ = false;
        }
    }
    
    void update(float dt) {
        // Update ECS world systems
        ecs_world_.update(dt);

        // Update scene
        scene_manager_.update(dt);

        // Update audio
        audio_.update(dt);
    }

    void render() {
        renderer_.begin_frame();
        if (Scene* scene = scene_manager_.getActiveScene()) {
            // Scene nodes store renderer component pointers directly. Submit
            // every top-level node and recurse through its hierarchy.
            for (auto& [id, node] : scene->nodes) {
                if (!node->parent) submit_scene_node(*node);
            }
        }
        renderer_.end_frame();
        renderer_.present();
    }

    void submit_scene_node(const SceneNode& node) {
        if (!node.visible) return;

        if (node.cameraComponent) {
            RenderCamera camera = *node.cameraComponent;
            camera.update();
            renderer_.set_camera(camera);
        }
        if (node.meshComponent && node.materialComponent) {
            RenderMesh mesh;
            mesh.data = *node.meshComponent;
            renderer_.draw_mesh(mesh, node.transform, *node.materialComponent);
        }
        for (const auto& child : node.children) {
            submit_scene_node(*child);
        }
    }
    
    // Subsystems
    World ecs_world_;
    Renderer renderer_;
    Input input_;
    AudioManager audio_;
    SceneManager scene_manager_;
    
    // Config
    EngineConfig config_;
    std::atomic<bool> running_{false};
    bool initialized_ = false;
};

// =============================================================================
// Free Functions
// =============================================================================

inline bool parse_engine_u32(const char* text, uint32_t& out) {
    if (!text || !*text || *text == '-') return false;
    errno = 0;
    char* end = nullptr;
    const unsigned long value = std::strtoul(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' ||
        value == 0 || value > 16384ul) {
        return false;
    }
    out = static_cast<uint32_t>(value);
    return true;
}

inline bool parse_engine_fps(const char* text, float& out) {
    if (!text || !*text) return false;
    errno = 0;
    char* end = nullptr;
    const float value = std::strtof(text, &end);
    if (errno != 0 || end == text || *end != '\0' ||
        !std::isfinite(value) || value <= 0.0f || value > 1000.0f) {
        return false;
    }
    out = value;
    return true;
}

inline int run_engine(int argc, char** argv) {
    Engine engine;
    EngineConfig config;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--width") == 0) {
            if (i + 1 >= argc || !parse_engine_u32(argv[++i], config.width)) return 2;
        } else if (std::strcmp(argv[i], "--height") == 0) {
            if (i + 1 >= argc || !parse_engine_u32(argv[++i], config.height)) return 2;
        } else if (std::strcmp(argv[i], "--fps") == 0) {
            if (i + 1 >= argc || !parse_engine_fps(argv[++i], config.target_fps)) return 2;
        } else if (std::strcmp(argv[i], "--fullscreen") == 0) {
            config.fullscreen = true;
        } else if (std::strcmp(argv[i], "--headless") == 0) {
            config.headless = true;
        } else if (std::strcmp(argv[i], "--windowed") == 0) {
            config.headless = false;
        } else {
            return 2;
        }
    }

    if (!engine.initialize(config)) return 1;
    engine.run();
    engine.shutdown();
    return 0;
}

} // namespace litt
