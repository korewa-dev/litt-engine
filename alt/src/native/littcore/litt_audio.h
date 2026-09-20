// LittAudio - Audio system for Litt Engine
// Audio system module

#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <fstream>
#include <cstring>
#include <algorithm>
#include <cmath>

#include "litt_math.h"

namespace litt {

enum class AudioState {
    Stopped,
    Playing,
    Paused
};

// Audio PCM format
enum class AudioFormat {
    MONO8,
    STEREO8,
    MONO16,
    STEREO16
};

struct AudioClip {
    std::string path;
    std::vector<float> data;
    uint32_t sampleRate = 44100;
    uint16_t channels = 2;
    uint32_t lengthSamples = 0;
    
    float duration() const {
        return sampleRate == 0 ? 0.0f : (float)lengthSamples / sampleRate;
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
        if (state != AudioState::Playing || !clip || !std::isfinite(dt) || dt <= 0.0f ||
            !std::isfinite(pitch) || pitch <= 0.0f) return;
        const float duration = clip->duration();
        if (!(duration > 0.0f) || !std::isfinite(duration)) {
            stop();
            return;
        }
        currentTime += dt * pitch;
        if (currentTime >= duration) {
            if (loop) {
                currentTime = std::fmod(currentTime, duration);
            } else {
                state = AudioState::Stopped;
                currentTime = 0.0f;
                velocity = 0.0f;
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
        if (!loadAudioFile(path, *clip)) return nullptr;
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
        if (!std::isfinite(volume)) return;
        masterVolume_ = std::clamp(volume, 0.0f, 1.0f);
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
    
    bool loadAudioFile(const std::string& path, AudioClip& clip) {
        std::ifstream file(path, std::ios::binary);
        if (!file) return false;

        char riff[4]{}, wave[4]{};
        uint32_t riff_size = 0;
        if (!file.read(riff, 4) ||
            !file.read(reinterpret_cast<char*>(&riff_size), 4) ||
            !file.read(wave, 4) ||
            std::memcmp(riff, "RIFF", 4) != 0 ||
            std::memcmp(wave, "WAVE", 4) != 0) {
            return false;
        }

        uint16_t format = 0;
        uint16_t channels = 0;
        uint32_t sample_rate = 0;
        uint16_t bits_per_sample = 0;
        std::vector<uint8_t> pcm;
        bool have_fmt = false;

        while (file) {
            char id[4]{};
            uint32_t size = 0;
            if (!file.read(id, 4) ||
                !file.read(reinterpret_cast<char*>(&size), 4)) break;

            if (std::memcmp(id, "fmt ", 4) == 0) {
                if (size < 16) return false;
                if (have_fmt) return false;
                uint32_t byte_rate = 0;
                uint16_t block_align = 0;
                if (!file.read(reinterpret_cast<char*>(&format), 2) ||
                    !file.read(reinterpret_cast<char*>(&channels), 2) ||
                    !file.read(reinterpret_cast<char*>(&sample_rate), 4) ||
                    !file.read(reinterpret_cast<char*>(&byte_rate), 4) ||
                    !file.read(reinterpret_cast<char*>(&block_align), 2) ||
                    !file.read(reinterpret_cast<char*>(&bits_per_sample), 2)) {
                    return false;
                }
                if (format != 1 || channels == 0 || channels > 2 || sample_rate == 0 ||
                    (bits_per_sample != 8 && bits_per_sample != 16) ||
                    block_align != channels * (bits_per_sample / 8u) ||
                    byte_rate != sample_rate * block_align) return false;
                if (size > 16) file.seekg(static_cast<std::streamoff>(size - 16), std::ios::cur);
                if (!file) return false;
                have_fmt = true;
            } else if (std::memcmp(id, "data", 4) == 0) {
                if (!have_fmt || format != 1 || channels == 0 ||
                    sample_rate == 0 || (bits_per_sample != 8 && bits_per_sample != 16)) {
                    return false;
                }
                constexpr uint32_t kMaxPcmBytes = 256u * 1024u * 1024u;
                if (size == 0 || size > kMaxPcmBytes) return false;
                const uint32_t bytes_per_frame = channels * (bits_per_sample / 8u);
                if (bytes_per_frame == 0 || size % bytes_per_frame != 0) return false;
                pcm.resize(size);
                if (!file.read(reinterpret_cast<char*>(pcm.data()), size)) return false;
            } else {
                file.seekg(static_cast<std::streamoff>(size), std::ios::cur);
            }

            if (size & 1u) file.seekg(1, std::ios::cur);
            if (!pcm.empty()) break;
        }

        if (pcm.empty()) return false;

        clip.sampleRate = sample_rate;
        clip.channels = channels;
        clip.data.clear();

        if (bits_per_sample == 8) {
            clip.data.reserve(pcm.size());
            for (uint8_t sample : pcm) {
                clip.data.push_back((static_cast<float>(sample) - 128.0f) / 128.0f);
            }
        } else {
            if (pcm.size() % 2 != 0) return false;
            clip.data.reserve(pcm.size() / 2);
            for (size_t i = 0; i < pcm.size(); i += 2) {
                const int16_t sample = static_cast<int16_t>(
                    static_cast<uint16_t>(pcm[i]) |
                    (static_cast<uint16_t>(pcm[i + 1]) << 8));
                clip.data.push_back(static_cast<float>(sample) / 32768.0f);
            }
        }

        if (clip.data.size() % channels != 0) return false;
        clip.lengthSamples = static_cast<uint32_t>(clip.data.size() / channels);
        return clip.lengthSamples > 0;
    }
};

} // namespace litt
