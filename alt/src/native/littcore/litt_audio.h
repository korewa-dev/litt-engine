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
#include <cmath>
#include <stdexcept>
#include <limits>
#include <algorithm>

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
        return sampleRate == 0 ? 0.0f : static_cast<float>(lengthSamples) /
                                      static_cast<float>(sampleRate);
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
        if (state != AudioState::Playing || !clip ||
            !std::isfinite(dt) || dt <= 0.0f ||
            !std::isfinite(pitch) || pitch <= 0.0f) {
            return;
        }

        const float duration_s = clip->duration();
        if (!(duration_s > 0.0f) || !std::isfinite(duration_s)) {
            stop();
            return;
        }

        currentTime += dt * pitch;
        if (currentTime >= duration_s) {
            if (loop) {
                currentTime = std::fmod(currentTime, duration_s);
            } else {
                stop();
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
        sampleRate_ = (sampleRate >= 8000 && sampleRate <= 192000) ? sampleRate : 44100;
        bufferFrames_ = bufferFrames > 0 ? bufferFrames : 2048;
    }
    
    void shutdown() {
        sources_.clear();
        clips_.clear();
        listeners_.clear();
    }
    
    std::shared_ptr<AudioClip> loadClip(const std::string& path) {
        if (path.empty()) return nullptr;
        auto it = clips_.find(path);
        if (it != clips_.end()) return it->second;

        auto clip = std::make_shared<AudioClip>();
        clip->path = path;
        if (!loadAudioFile(path, *clip)) return nullptr;
        clips_[path] = clip;
        return clip;
    }
    
    AudioSource& addSource(const std::string& name, const std::string& clipPath) {
        if (name.empty()) throw std::invalid_argument("audio source name must not be empty");
        auto clip = loadClip(clipPath);
        if (!clip) throw std::runtime_error("failed to load audio clip");
        auto source = std::make_shared<AudioSource>();
        source->name = name;
        source->clip = std::move(clip);
        sources_[name] = std::move(source);
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
    
    static uint16_t read_u16_le(const uint8_t* p) {
        return static_cast<uint16_t>(p[0]) |
               static_cast<uint16_t>(static_cast<uint16_t>(p[1]) << 8);
    }

    static uint32_t read_u32_le(const uint8_t* p) {
        return static_cast<uint32_t>(p[0]) |
               (static_cast<uint32_t>(p[1]) << 8) |
               (static_cast<uint32_t>(p[2]) << 16) |
               (static_cast<uint32_t>(p[3]) << 24);
    }

    bool loadAudioFile(const std::string& path, AudioClip& clip) {
        constexpr uint32_t kMaxAudioBytes = 64u * 1024u * 1024u;
        std::ifstream file(path, std::ios::binary);
        if (!file) return false;

        uint8_t header[12]{};
        if (!file.read(reinterpret_cast<char*>(header), sizeof(header))) return false;
        if (std::memcmp(header, "RIFF", 4) != 0 ||
            std::memcmp(header + 8, "WAVE", 4) != 0) {
            return false;
        }

        bool have_format = false;
        uint16_t channels = 0;
        uint16_t bits_per_sample = 0;
        uint32_t sample_rate = 0;
        std::vector<uint8_t> pcm;

        while (file) {
            uint8_t chunk_header[8]{};
            if (!file.read(reinterpret_cast<char*>(chunk_header), sizeof(chunk_header))) break;
            const uint32_t chunk_size = read_u32_le(chunk_header + 4);
            if (chunk_size > kMaxAudioBytes) return false;

            if (std::memcmp(chunk_header, "fmt ", 4) == 0) {
                if (chunk_size < 16) return false;
                std::vector<uint8_t> fmt(chunk_size);
                if (!file.read(reinterpret_cast<char*>(fmt.data()),
                               static_cast<std::streamsize>(fmt.size()))) return false;

                const uint16_t format = read_u16_le(fmt.data());
                channels = read_u16_le(fmt.data() + 2);
                sample_rate = read_u32_le(fmt.data() + 4);
                bits_per_sample = read_u16_le(fmt.data() + 14);
                if (format != 1 || (channels != 1 && channels != 2) ||
                    sample_rate < 8000 || sample_rate > 192000 ||
                    (bits_per_sample != 8 && bits_per_sample != 16)) {
                    return false;
                }
                have_format = true;
            } else if (std::memcmp(chunk_header, "data", 4) == 0) {
                if (!have_format) return false;
                pcm.resize(chunk_size);
                if (chunk_size != 0 &&
                    !file.read(reinterpret_cast<char*>(pcm.data()),
                               static_cast<std::streamsize>(pcm.size()))) return false;
            } else {
                file.seekg(static_cast<std::streamoff>(chunk_size), std::ios::cur);
                if (!file) return false;
            }

            if ((chunk_size & 1u) != 0u) {
                file.seekg(1, std::ios::cur);
                if (!file) return false;
            }

            if (have_format && !pcm.empty()) break;
        }

        if (!have_format || pcm.empty()) return false;
        const uint32_t bytes_per_sample = bits_per_sample / 8u;
        const uint32_t frame_bytes = bytes_per_sample * channels;
        if (frame_bytes == 0 || pcm.size() % frame_bytes != 0) return false;

        const size_t sample_count = pcm.size() / bytes_per_sample;
        clip.data.clear();
        clip.data.reserve(sample_count);

        if (bits_per_sample == 8) {
            for (uint8_t sample : pcm) {
                clip.data.push_back((static_cast<int>(sample) - 128) / 128.0f);
            }
        } else {
            for (size_t i = 0; i + 1 < pcm.size(); i += 2) {
                const uint16_t raw = read_u16_le(pcm.data() + i);
                const int16_t sample = static_cast<int16_t>(raw);
                clip.data.push_back(static_cast<float>(sample) / 32768.0f);
            }
        }

        clip.sampleRate = sample_rate;
        clip.channels = channels;
        clip.lengthSamples = static_cast<uint32_t>(pcm.size() / frame_bytes);
        return clip.lengthSamples > 0;
    }
};

} // namespace litt
