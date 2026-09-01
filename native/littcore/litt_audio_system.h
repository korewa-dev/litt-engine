// Phase 6: Advanced Features - Audio System

#pragma once

#include "litt_math.h"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

namespace litt {

// Audio source state
enum class AudioState {
    STOPPED,
    PLAYING,
    PAUSED
};

// Audio format
enum class AudioFormat {
    MONO8,
    STEREO8,
    MONO16,
    STEREO16
};

// Audio buffer
class AudioBuffer {
public:
    AudioBuffer(uint32_t id, const std::string& path);
    ~AudioBuffer();
    
    // Load from file
    bool load(const std::string& path);
    
    // Get buffer ID
    uint32_t get_id() const { return buffer_id_; }
    
    // Get duration in seconds
    float get_duration() const { return duration_; }
    
    // Get format
    AudioFormat get_format() const { return format_; }

private:
    uint32_t buffer_id_;
    std::string path_;
    AudioFormat format_;
    float duration_;
    std::vector<uint8_t> pcm_data_;
};

// Audio source
class AudioSource {
public:
    AudioSource();
    ~AudioSource();
    
    // Play
    void play();
    
    // Pause
    void pause();
    
    // Stop
    void stop();
    
    // Set buffer
    void set_buffer(AudioBuffer* buffer);
    
    // Set loop
    void set_loop(bool loop) { loop_ = loop; }
    bool is_looping() const { return loop_; }
    
    // Set volume
    void set_volume(float volume) { volume_ = volume; }
    float get_volume() const { return volume_; }
    
    // Set pitch
    void set_pitch(float pitch) { pitch_ = pitch; }
    float get_pitch() const { return pitch_; }
    
    // Set position
    void set_position(const Vec3& pos) { position_ = pos; }
    const Vec3& get_position() const { return position_; }
    
    // Get state
    AudioState get_state() const { return state_; }

private:
    uint32_t source_id_;
    AudioBuffer* buffer_;
    AudioState state_;
    bool loop_;
    float volume_;
    float pitch_;
    Vec3 position_;
};

// Audio listener
class AudioListener {
public:
    AudioListener() = default;
    
    // Set position
    void set_position(const Vec3& pos) { position_ = pos; }
    const Vec3& get_position() const { return position_; }
    
    // Set orientation
    void set_orientation(const Vec3& forward, const Vec3& up);
    
    // Set velocity
    void set_velocity(const Vec3& vel) { velocity_ = vel; }

private:
    Vec3 position_;
    Vec3 orientation_forward_;
    Vec3 orientation_up_;
    Vec3 velocity_;
};

// Audio engine
class AudioEngine {
public:
    static AudioEngine& get_instance() {
        static AudioEngine instance;
        return instance;
    }
    
    // Initialize audio
    bool initialize();
    
    // Shutdown audio
    void shutdown();
    
    // Create buffer
    AudioBuffer* create_buffer(const std::string& path);
    
    // Create source
    AudioSource* create_source();
    
    // Get listener
    AudioListener* get_listener() { return &listener_; }
    
    // Play one-shot sound
    void play_one_shot(const std::string& path, float volume = 1.0f);
    
    // Update audio
    void update(float delta_time);

private:
    AudioEngine() = default;
    AudioListener listener_;
    std::unordered_map<std::string, std::unique_ptr<AudioBuffer>> buffers_;
    std::vector<std::unique_ptr<AudioSource>> sources_;
};

} // namespace litt
