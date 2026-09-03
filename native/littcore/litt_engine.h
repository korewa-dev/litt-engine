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
        
        // Initialize renderer
        if (!renderer_.initialize(config_.width, config_.height, config_.backend)) {
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
        float target_frame_time = 1.0f / config_.target_fps;
        
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
            
            float elapsed = std::chrono::duration<float>(
                std::chrono::high_resolution_clock::now() - now).count();
            if (frame_time > target_frame_time) {
                std::this_thread::sleep_for(
                    std::chrono::milliseconds((int)((frame_time - target_frame_time) * 1000)));
            }
        }
    }
    
    void run_headless() {
        auto last_time = std::chrono::high_resolution_clock::now();
        
        while (running_) {
            auto now = std::chrono::high_resolution_clock::now();
            float frame_time = std::chrono::duration<float>(now - last_time).count();
            last_time = now;
            
            if (frame_time > 0.1f) frame_time = 0.1f;
            
            update(frame_time);
            
            Profiler::get_instance().update_fps();
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
    
    void load_scene(const std::string& path) {
        log_info("Loading scene: " + path);
    }
    
    void save_scene(const std::string& path) {
        log_info("Saving scene: " + path);
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
        
        // Create root node
        SceneNode* root = &scene.createNode("Root");
        
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
        
        // Update scene
        scene_manager_.update(dt);
        
        // Update audio
        audio_.update(dt);
    }
    
    void render() {
        renderer_.begin_frame();
        // Scene rendering uses RenderScene from litt_renderer.h
        // Scene nodes from litt_scene.h are separate - conversion happens in the renderer
        renderer_.end_frame();
        renderer_.present();
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
