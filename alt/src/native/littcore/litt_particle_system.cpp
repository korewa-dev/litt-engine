#include "litt_particle_system.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace litt {
namespace {
constexpr uint32_t kMaxParticles = 65536u;
float unit_hash(uint32_t value) {
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return static_cast<float>(value & 0x00ffffffu) / static_cast<float>(0x01000000u);
}
float lerp_scalar(float a, float b, float t) { return a + (b - a) * t; }
Vec3 lerp_vec3(const Vec3& a, const Vec3& b, float t) {
    return Vec3{lerp_scalar(a.x,b.x,t),lerp_scalar(a.y,b.y,t),lerp_scalar(a.z,b.z,t)};
}
Vec4 lerp_vec4(const Vec4& a, const Vec4& b, float t) {
    return Vec4{lerp_scalar(a.x,b.x,t),lerp_scalar(a.y,b.y,t),lerp_scalar(a.z,b.z,t),lerp_scalar(a.w,b.w,t)};
}
}

ParticleSystem::ParticleSystem(uint32_t max_particles)
    : max_particles_(std::min(std::max(1u, max_particles), kMaxParticles)),
      shape_(EmitterShape::POINT), position_(Vec3::zero()), emission_rate_(0.0f),
      emission_accumulator_(0.0f), lifetime_min_(1.0f), lifetime_max_(1.0f),
      start_size_min_(1.0f), start_size_max_(1.0f), end_size_min_(0.0f), end_size_max_(0.0f),
      start_color_(Vec4::one()), end_color_(Vec4::zero()),
      velocity_min_(Vec3::zero()), velocity_max_(Vec3::zero()),
      acceleration_(Vec3::zero()), gravity_(Vec3{0.0f,-9.81f,0.0f}), playing_(false) {
    particles_.reserve(max_particles_);
}

ParticleSystem::~ParticleSystem() = default;
void ParticleSystem::play() { playing_ = true; }
void ParticleSystem::pause() { playing_ = false; }
void ParticleSystem::stop() {
    playing_ = false;
    emission_accumulator_ = 0.0f;
    for (auto& particle : particles_) particle.alive = false;
}

void ParticleSystem::emit(uint32_t count) {
    count = std::min(count, max_particles_);
    for (uint32_t i = 0; i < count && get_alive_count() < max_particles_; ++i) spawn_particle();
}

void ParticleSystem::update(float dt) {
    if (!std::isfinite(dt) || dt <= 0.0f) return;
    if (playing_ && std::isfinite(emission_rate_) && emission_rate_ > 0.0f) {
        emission_accumulator_ += emission_rate_ * dt;
        uint32_t count = static_cast<uint32_t>(std::min(emission_accumulator_, static_cast<float>(max_particles_)));
        emission_accumulator_ -= static_cast<float>(count);
        emit(count);
    }
    for (auto& particle : particles_) if (particle.alive) update_particle(particle, dt);
}

uint32_t ParticleSystem::get_alive_count() const {
    return static_cast<uint32_t>(std::count_if(particles_.begin(), particles_.end(),
        [](const Particle& particle) { return particle.alive; }));
}

void ParticleSystem::spawn_particle() {
    Particle* particle = nullptr;
    uint32_t slot = 0;
    for (uint32_t i = 0; i < particles_.size(); ++i) {
        if (!particles_[i].alive) { particle = &particles_[i]; slot = i; break; }
    }
    if (!particle) {
        if (particles_.size() >= max_particles_) return;
        particles_.push_back(Particle{});
        slot = static_cast<uint32_t>(particles_.size() - 1u);
        particle = &particles_.back();
    }

    const float a = unit_hash(slot * 4u + get_alive_count() + 1u);
    const float b = unit_hash(slot * 4u + get_alive_count() + 2u);
    const float c = unit_hash(slot * 4u + get_alive_count() + 3u);
    const float life_min = std::max(0.001f, std::min(lifetime_min_, lifetime_max_));
    const float life_max = std::max(life_min, std::max(lifetime_min_, lifetime_max_));
    const float size_min = std::max(0.0f, std::min(start_size_min_, start_size_max_));
    const float size_max = std::max(size_min, std::max(start_size_min_, start_size_max_));
    const float end_min = std::max(0.0f, std::min(end_size_min_, end_size_max_));
    const float end_max = std::max(end_min, std::max(end_size_min_, end_size_max_));

    particle->position = random_emitter_position();
    particle->velocity = Vec3{lerp_scalar(velocity_min_.x,velocity_max_.x,a),
                              lerp_scalar(velocity_min_.y,velocity_max_.y,b),
                              lerp_scalar(velocity_min_.z,velocity_max_.z,c)};
    particle->acceleration = acceleration_;
    particle->color_start = start_color_;
    particle->color_end = end_color_;
    particle->color = start_color_;
    particle->size_start = lerp_scalar(size_min,size_max,a);
    particle->size_end = lerp_scalar(end_min,end_max,b);
    particle->size = particle->size_start;
    particle->life_max = lerp_scalar(life_min,life_max,c);
    particle->life = particle->life_max;
    particle->rotation = 0.0f;
    particle->rotation_speed = 0.0f;
    particle->alive = true;
}

void ParticleSystem::kill_particle(uint32_t index) {
    if (index < particles_.size()) particles_[index].alive = false;
}

void ParticleSystem::update_particle(Particle& p, float dt) {
    p.life -= dt;
    if (p.life <= 0.0f) { p.alive = false; return; }
    p.velocity += (p.acceleration + gravity_) * dt;
    p.position += p.velocity * dt;
    p.rotation += p.rotation_speed * dt;
    const float t = std::clamp(1.0f - p.life / p.life_max, 0.0f, 1.0f);
    p.size = lerp_scalar(p.size_start, p.size_end, t);
    p.color = lerp_vec4(p.color_start, p.color_end, t);
}

Vec3 ParticleSystem::random_emitter_position() {
    const uint32_t seed = static_cast<uint32_t>(particles_.size() * 17u + get_alive_count() * 31u + 1u);
    const float u = unit_hash(seed);
    const float v = unit_hash(seed + 1u);
    const float w = unit_hash(seed + 2u);
    switch (shape_) {
        case EmitterShape::POINT: return position_;
        case EmitterShape::BOX: return position_ + Vec3{u - 0.5f, v - 0.5f, w - 0.5f};
        case EmitterShape::CIRCLE: {
            const float angle = u * 6.28318530718f;
            const float radius = std::sqrt(v);
            return position_ + Vec3{std::cos(angle)*radius,0.0f,std::sin(angle)*radius};
        }
        case EmitterShape::SPHERE: {
            const float z = 2.0f * u - 1.0f;
            const float angle = v * 6.28318530718f;
            const float r = std::cbrt(w);
            const float xy = std::sqrt(std::max(0.0f,1.0f-z*z));
            return position_ + Vec3{r*xy*std::cos(angle), r*z, r*xy*std::sin(angle)};
        }
        case EmitterShape::CONE: {
            const float angle = u * 6.28318530718f;
            const float radius = v;
            return position_ + Vec3{std::cos(angle)*radius, w, std::sin(angle)*radius};
        }
    }
    return position_;
}

ParticleSystem* ParticleManager::create_system(uint32_t max_particles) {
    if (systems_.size() >= 1024u) return nullptr;
    auto system = std::make_unique<ParticleSystem>(max_particles);
    ParticleSystem* result = system.get();
    systems_.push_back(std::move(system));
    return result;
}
ParticleSystem* ParticleManager::get_system(uint32_t index) {
    return index < systems_.size() ? systems_[index].get() : nullptr;
}
void ParticleManager::remove_system(uint32_t index) {
    if (index < systems_.size()) systems_.erase(systems_.begin() + index);
}
void ParticleManager::update(float dt) {
    for (auto& system : systems_) if (system) system->update(dt);
}

} // namespace litt
