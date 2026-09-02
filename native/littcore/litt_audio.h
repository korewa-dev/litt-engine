// LittAudio - Audio system for Litt Engine
// Audio system module

#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>

#include "litt_math.h"

namespace litt {

enum class AudioState {
    Stopped,
    Playing,
    Paused
};

struct AudioClip {
    std::string path;
    std::vector<float> data;
    uint32_t sampleRate = 44100;
    uint16_t channels = 2;
    uint32_t lengthSamples = 0;
    
    float duration() const {
        return (float)lengthSamples / sampleRate;
    }
};

struct AudioSource {
    std::string name;
    std::shared_ptr<AudioClip> clip;
    AudioState state = AudioState::Stopped;
    float volume = 1.0f;
    float pitch = 1.0f;
    bool loop = false;
    bool spatial = false;
    
    // 3D audio
    Vec3 position = Vec3::zero();
    float minDistance = 1.0f;
    float maxDistance = 100.0f;
    float rolloff = 1.0f;
    
    // Playback state
    float currentTime = 0.0f;
    float velocity = 0.0f;
    
    void play() {
        state = AudioState::Playing;
        velocity = 1.0f;
    }
    
    void pause() {
        state = AudioState::Paused;
        velocity = 0.0f;
    }
    
    void stop() {
        state = AudioState::Stopped;
        currentTime = 0.0f;
        velocity = 0.0f;
    }
    
    void update(float dt) {
        if (state != AudioState::Playing || !clip) return;  // null-clip guard
        currentTime += dt * pitch;
        if (currentTime >= clip->duration()) {
            if (loop) {
                currentTime = 0.0f;
            } else {
                state = AudioState::Stopped;
            }
        }
    }
};

struct AudioListener {
    Vec3 position = Vec3::zero();
    Vec3 forward = Vec3::forward();
    Vec3 up = Vec3::up();
    Vec3 velocity = Vec3::zero();
    
    void update(const Vec3& newPos, const Vec3& newForward) {
        Vec3 delta = newPos - position;
        velocity = delta * (1.0f / 0.016f); // approximate velocity per frame
        position = newPos;
        forward = newForward.normalized();
    }
};

class AudioManager {
public:
    AudioManager() = default;
    ~AudioManager() = default;
    
    void init(int sampleRate = 44100, int bufferFrames = 2048) {
        sampleRate_ = sampleRate;
        bufferFrames_ = bufferFrames;
        // Initialize audio backend (OpenAL, WASAPI, etc.)
    }
    
    void shutdown() {
        sources_.clear();
        clips_.clear();
        listeners_.clear();
    }
    
    std::shared_ptr<AudioClip> loadClip(const std::string& path) {
        auto it = clips_.find(path);
        if (it != clips_.end()) return it->second;
        
        auto clip = std::make_shared<AudioClip>();
        clip->path = path;
        // Load audio data
        loadAudioFile(path, *clip);
        clips_[path] = clip;
        return clip;
    }
    
    AudioSource& addSource(const std::string& name, const std::string& clipPath) {
        auto clip = loadClip(clipPath);
        sources_[name] = std::make_shared<AudioSource>();
        sources_[name]->name = name;
        sources_[name]->clip = clip;
        return *sources_[name];
    }
    
    void removeSource(const std::string& name) {
        sources_.erase(name);
    }
    
    AudioSource* getSource(const std::string& name) {
        auto it = sources_.find(name);
        return it != sources_.end() ? it->second.get() : nullptr;
    }
    
    void setListener(const Vec3& position, const Vec3& forward) {
        if (!listeners_.empty()) {
            listeners_.front().update(position, forward);
        }
    }
    
    void addListener(const Vec3& position) {
        listeners_.emplace_back();
        listeners_.back().position = position;
    }
    
    void update(float dt) {
        for (auto& source : sources_) {
            source.second->update(dt);
        }
        for (auto& listener : listeners_) {
            (void)listener; // Update listener state
        }
    }
    
    void setMasterVolume(float volume) {
        masterVolume_ = volume;
    }
    
    float getMasterVolume() const {
        return masterVolume_;
    }
    
private:
    std::unordered_map<std::string, std::shared_ptr<AudioClip>> clips_;
    std::unordered_map<std::string, std::shared_ptr<AudioSource>> sources_;
    std::vector<AudioListener> listeners_;
    
    int sampleRate_ = 44100;
    int bufferFrames_ = 2048;
    float masterVolume_ = 1.0f;
    
    void loadAudioFile(const std::string&, AudioClip& clip) {
        // Simplified - would use actual audio library
        clip.sampleRate = sampleRate_;
        clip.channels = 2;
        // Load and decode audio file
    }
};

} // namespace litt
