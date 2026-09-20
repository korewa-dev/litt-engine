// LittEngine - Main engine entry point
// Ties all subsystems together

#pragma once
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
    bool headless = false;
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
        config_ = config;
        
        // A headless engine does not need a graphics backend. Interactive
        // modes still fail honestly when the selected backend is unavailable.
        if (!config_.headless &&
            !renderer_.initialize(config_.width, config_.height, config_.backend)) {
            log_error("Failed to initialize renderer");
            return false;
        }
        
        // Initialize audio
        audio_.init();
        
        // Create default scene
        create_default_scene();
        
        log_info("Engine initialized successfully");
        return true;
    }
    
    void shutdown() {
        log_info("Shutting down engine...");
        
        // Shutdown in reverse order
        renderer_.shutdown();
        audio_.shutdown();
        
        log_info("Engine shutdown complete");
    }
    
    // =====================================================================
    // Game Loop
    // =====================================================================
    
    void run() {
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
    bool is_running() const { return running_; }
    
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
    std::atomic<bool> running_{true};
};

// =============================================================================
// Free Functions
// =============================================================================

inline int run_engine(int argc, char** argv) {
    Engine engine;
    
    EngineConfig config;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--width") == 0 && i + 1 < argc) {
            config.width = std::stoi(argv[i + 1]);
            i++;
        }
        else if (strcmp(argv[i], "--height") == 0 && i + 1 < argc) {
            config.height = std::stoi(argv[i + 1]);
            i++;
        }
        else if (strcmp(argv[i], "--fullscreen") == 0) {
            config.fullscreen = true;
        }
        else if (strcmp(argv[i], "--headless") == 0) {
            config.headless = true;
        }
    }
    
    if (!engine.initialize(config)) {
        return 1;
    }
    
    engine.run();
    engine.shutdown();
    
    return 0;
}

} // namespace litt
