// LittEngine - Main engine entry point
// Ties all subsystems together

#pragma once
#include "littcore/litt_math.h"
#include "littcore/litt_ecs.h"
#include "littcore/litt_input.h"
#include "littcore/litt_world.h"
#include "littcore/litt_scene.h"
#include "littcore/litt_physics.h"
#include "littcore/litt_audio.h"
#include "littcore/litt_ui.h"
#include "littcore/litt_config.h"
#include "littcore/litt_profiler.h"
#include "littcore/litt_renderer.h"
#include "littcore/litt_obj_cpp.h"

#include <string>
#include <memory>
#include <chrono>
#include <thread>
#include <atomic>

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
        
        // Initialize subsystems
        if (!init_subsystems()) {
            return false;
        }
        
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
        physics_.shutdown();
        
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
        float frame_time = 0.0f;
        float target_frame_time = 1.0f / config_.target_fps;
        
        while (running_) {
            auto now = std::chrono::high_resolution_clock::now();
            frame_time = std::chrono::duration<float>(now - last_time).count();
            last_time = now;
            
            // Cap frame time
            if (frame_time > 0.1f) frame_time = 0.1f;
            
            // Process input
            input_.update();
            process_input();
            
            // Update game
            update(frame_time);
            
            // Render
            render();
            
            // Frame timing
            profiler_.record_frame(frame_time * 1000.0f);
            
            // Throttle to target FPS
            float elapsed = std::chrono::duration<float>(
                std::chrono::high_resolution_clock::now() - now).count();
            if (frame_time > target_frame_time) {
                std::this_thread::sleep_for(
                    std::chrono::milliseconds((int)((frame_time - target_frame_time) * 1000)));
            }
        }
    }
    
    void run_headless() {
        // Headless mode - no rendering
        auto last_time = std::chrono::high_resolution_clock::now();
        float frame_time = 0.0f;
        
        while (running_) {
            auto now = std::chrono::high_resolution_clock::now();
            frame_time = std::chrono::duration<float>(now - last_time).count();
            last_time = now;
            
            if (frame_time > 0.1f) frame_time = 0.1f;
            
            input_.update();
            update(frame_time);
            
            profiler_.record_frame(frame_time * 1000.0f);
        }
    }
    
    void stop() {
        running_ = false;
    }
    
    // =====================================================================
    // Accessors
    // =====================================================================
    
    WorldManager& world() { return world_; }
    World& ecs_world() { return ecs_world_; }
    Renderer& renderer() { return renderer_; }
    InputState& input() { return input_; }
    PhysicsSystem& physics() { return physics_; }
    AudioManager& audio() { return audio_; }
    Profiler& profiler() { return profiler_; }
    SceneManager& scene_manager() { return scene_manager_; }
    
    const EngineConfig& get_config() const { return config_; }
    bool is_running() const { return running_; }
    
    // =====================================================================
    // Scene Management
    // =====================================================================
    
    void load_scene(const std::string& path) {
        // Load scene from file
        log_info("Loading scene: " + path);
        // Implementation...
    }
    
    void save_scene(const std::string& path) {
        log_info("Saving scene: " + path);
        // Implementation...
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
    bool init_subsystems() {
        // Initialize renderer
        if (!renderer_.initialize(config_.width, config_.height, config_.backend)) {
            log_error("Failed to initialize renderer");
            return false;
        }
        
        // Initialize audio
        audio_.init();
        
        // Initialize physics
        physics_.init();
        
        // Load default config
        config_.load_default();
        
        return true;
    }
    
    void create_default_scene() {
        auto& scene = scene_manager_.create_scene("Default");
        
        // Create root node
        auto* root = scene.create_node("Root");
        
        // Create camera
        auto* cam_node = root->create_child("Camera");
        cam_node->add_component<Transform>({0, 5, -10});
        cam_node->add_component<Camera>({60.0f, 16.0f/9.0f, 0.1f, 1000.0f});
        renderer_.add_camera(cam_node->get_component<Camera>());
        
        // Create light
        auto* light_node = root->create_child("Light");
        light_node->add_component<Transform>({5, 10, 5});
        light_node->add_component<Light>({Light::Type::Directional, {1, 1, 1}, 1.0f});
        renderer_.add_light(light_node->get_component<Light>());
        
        // Create ground
        auto* ground = root->create_child("Ground");
        ground->add_component<Transform>({0, -1, 0, 0, 0, 0, 100, 1, 100});
        ground->add_component<Mesh>(0);
        ground->add_component<Material>({0.3f, 0.3f, 0.3f});
    }
    
    void process_input() {
        // Handle quit
        if (input_.is_key_just_pressed(Key::Escape)) {
            running_ = false;
        }
        
        // Handle reload
        if (input_.is_key_just_pressed(Key::F5)) {
            // Reload scene
        }
        
        // Handle toggle fullscreen
        if (input_.is_key_just_pressed(Key::F11)) {
            config_.fullscreen = !config_.fullscreen;
            // Resize window
        }
    }
    
    void update(float dt) {
        // Update world
        world_.update(dt);
        
        // Update physics
        physics_.update(dt);
        
        // Update scene
        scene_manager_.update(dt);
        
        // Update audio
        audio_.update(dt);
        
        // Update profiler
        profiler_.update(dt);
    }
    
    void render() {
        renderer_.begin_frame();
        renderer_.render_scene(scene_manager_.get_active_scene());
        renderer_.end_frame();
        renderer_.present();
    }
    
    // Subsystems
    WorldManager world_;
    World ecs_world_;
    Renderer renderer_;
    InputState input_;
    PhysicsSystem physics_;
    AudioManager audio_;
    Profiler profiler_;
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
    
    // Parse command line
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
        else if (strcmp(argv[i], "--scene") == 0 && i + 1 < argc) {
            config.scene_path = argv[i + 1];
            i++;
        }
    }
    
    // Initialize
    if (!engine.initialize(config)) {
        return 1;
    }
    
    // Run
    engine.run();
    
    // Shutdown
    engine.shutdown();
    
    return 0;
}

} // namespace litt
