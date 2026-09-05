// Phase 6: Advanced Features - Particle System

#pragma once

#include "litt_math.h"
#include <vector>
#include <memory>

namespace litt {

// Particle data
struct Particle {
    Vec3 position;
    Vec3 velocity;
    Vec3 acceleration;
    Vec4 color;
    Vec4 color_start;
    Vec4 color_end;
    float size;
    float size_start;
    float size_end;
    float life;
    float life_max;
    float rotation;
    float rotation_speed;
    bool alive;
};

// Particle emitter shape
enum class EmitterShape {
    POINT,
    SPHERE,
    BOX,
    CONE,
    CIRCLE
};

// Particle system
class ParticleSystem {
public:
    ParticleSystem(uint32_t max_particles = 1000);
    ~ParticleSystem();
    
    // Set emitter shape
    void set_emitter_shape(EmitterShape shape) { shape_ = shape; }
    EmitterShape get_emitter_shape() const { return shape_; }
    
    // Set emitter position
    void set_position(const Vec3& pos) { position_ = pos; }
    const Vec3& get_position() const { return position_; }
    
    // Set emission rate
    void set_emission_rate(float rate) { emission_rate_ = rate; }
    float get_emission_rate() const { return emission_rate_; }
    
    // Set particle lifetime
    void set_lifetime(float min, float max) { lifetime_min_ = min; lifetime_max_ = max; }
    
    // Set start size
    void set_start_size(float min, float max) { start_size_min_ = min; start_size_max_ = max; }
    
    // Set end size
    void set_end_size(float min, float max) { end_size_min_ = min; end_size_max_ = max; }
    
    // Set start color
    void set_start_color(const Vec4& color) { start_color_ = color; }
    
    // Set end color
    void set_end_color(const Vec4& color) { end_color_ = color; }
    
    // Set velocity
    void set_velocity(const Vec3& min, const Vec3& max) { velocity_min_ = min; velocity_max_ = max; }
    
    // Set acceleration
    void set_acceleration(const Vec3& accel) { acceleration_ = accel; }
    
    // Set gravity
    void set_gravity(const Vec3& gravity) { gravity_ = gravity; }
    
    // Play
    void play();
    
    // Pause
    void pause();
    
    // Stop
    void stop();
    
    // Emit particles
    void emit(uint32_t count);
    
    // Update particles
    void update(float delta_time);
    
    // Get particles
    const std::vector<Particle>& get_particles() const { return particles_; }
    
    // Get alive particle count
    uint32_t get_alive_count() const;
    
    // Get max particles
    uint32_t get_max_particles() const { return max_particles_; }

private:
    // Spawn particle
    void spawn_particle();
    
    // Kill particle
    void kill_particle(uint32_t index);
    
    // Update single particle
    void update_particle(Particle& p, float delta_time);
    
    // Generate random position based on emitter shape
    Vec3 random_emitter_position();
    
    uint32_t max_particles_;
    std::vector<Particle> particles_;
    EmitterShape shape_;
    Vec3 position_;
    float emission_rate_;
    float emission_accumulator_;
    float lifetime_min_;
    float lifetime_max_;
    float start_size_min_;
    float start_size_max_;
    float end_size_min_;
    float end_size_max_;
    Vec4 start_color_;
    Vec4 end_color_;
    Vec3 velocity_min_;
    Vec3 velocity_max_;
    Vec3 acceleration_;
    Vec3 gravity_;
    bool playing_;
};

// Particle manager
class ParticleManager {
public:
    static ParticleManager& get_instance() {
        static ParticleManager instance;
        return instance;
    }
    
    // Create particle system
    ParticleSystem* create_system(uint32_t max_particles = 1000);
    
    // Get system
    ParticleSystem* get_system(uint32_t index);
    
    // Remove system
    void remove_system(uint32_t index);
    
    // Update all systems
    void update(float delta_time);
    
    // Get system count
    size_t get_system_count() const { return systems_.size(); }

private:
    ParticleManager() = default;
    std::vector<std::unique_ptr<ParticleSystem>> systems_;
};

} // namespace litt
