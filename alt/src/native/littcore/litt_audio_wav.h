// Windows waveOut Audio Backend - Real audio playback
// Uses Windows multimedia API - no external dependencies

#pragma once
#include <memory>
#include <cstdint>
#include "litt_audio.h"
#include <windows.h>
#include <mmsystem.h>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include <cstring>

namespace litt {

class WindowsWaveOutAudio {
public:
    WindowsWaveOutAudio() = default;
    ~WindowsWaveOutAudio() { shutdown(); }
    
    bool init() {
        if (initialized_) return true;
        
        // Setup wave format
        wfx_.wFormatTag = WAVE_FORMAT_PCM;
        wfx_.nChannels = 2;
        wfx_.nSamplesPerSec = 44100;
        wfx_.wBitsPerSample = 16;
        wfx_.nBlockAlign = wfx_.nChannels * wfx_.wBitsPerSample / 8;
        wfx_.nAvgBytesPerSec = wfx_.nSamplesPerSec * wfx_.nBlockAlign;
        wfx_.cbSize = 0;
        
        // Open wave device
        MMRESULT result = waveOutOpen(&hwave_out_, WAVE_MAPPER, (LPCWAVEFORMATEX)&wfx_, 
                                       0, 0, CALLBACK_NULL);
        if (result != MMSYSERR_NOERROR) {
            std::cerr << "[Audio] waveOutOpen failed" << std::endl;
            return false;
        }
        
        initialized_ = true;
        std::cout << "[Audio] Windows waveOut initialized" << std::endl;
        return true;
    }
    
    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!initialized_) return;

        if (hwave_out_) {
            waveOutReset(hwave_out_);
            releaseBuffersLocked(true);
            waveOutClose(hwave_out_);
            hwave_out_ = nullptr;
        }

        sources_.clear();
        clips_.clear();
        initialized_ = false;
    }

    void update(float dt) {

        if (!std::isfinite(dt) || dt < 0.0f) return;
        std::lock_guard<std::mutex> lock(mutex_);
        releaseBuffersLocked(false);

        // Update all active sources
        for (auto& [name, source] : sources_) {
            if (source.state == AudioState::Playing && source.clip) {
                source.currentTime += dt * source.pitch;
                if (source.currentTime >= source.clip->duration()) {
                    if (source.loop) {
                        source.currentTime = 0.0f;
                    } else {
                        source.state = AudioState::Stopped;
                        source.currentTime = 0.0f;
                    }
                }
            }
        }
    }
    
    bool load_clip(const std::string& name, const std::string& path) {
        if (clips_.count(name)) return true;
        
        AudioClip clip;
        clip.path = path;
        
        if (!loadWavFile(path, clip)) {
            return false;
        }
        
        clips_[name] = std::make_shared<AudioClip>(clip);
        return true;
    }
    
    bool play(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!initialized_ || !hwave_out_) return false;
        releaseBuffersLocked(false);
        auto clip_it = clips_.find(name);
        if (clip_it == clips_.end() || !clip_it->second) return false;
        auto existing = sources_.find(name);
        if (existing != sources_.end() && existing->second.state == AudioState::Playing) return false;

        // waveOut is opened as 44.1 kHz stereo PCM16. Until a resampler/mixer is
        // implemented, reject mismatched clips instead of playing them at the
        // wrong speed or with the wrong channel layout.
        if (clip_it->second->sampleRate != 44100 || clip_it->second->channels != 2) {
            return false;
        }

        AudioSource source;
        source.name = name;
        source.clip = clip_it->second;
        source.state = AudioState::Playing;
        sources_[name] = source;
        if (!playBufferLocked(name, clip_it->second)) {
            sources_.erase(name);
            return false;
        }
        return true;
    }

    bool stop(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = sources_.find(name);
        if (it == sources_.end()) return false;

        // waveOutReset is device-wide. Pretending it can stop one source would
        // also kill unrelated playback, so per-source stop is unsupported until
        // this backend has a real mixer/voice layer.
        return false;
    }

    bool pause(const std::string& name) {
        (void)name;
        return false;
    }

    bool set_volume(const std::string& name, float volume) {
        (void)name; (void)volume;
        return false;
    }

    bool set_pitch(const std::string& name, float pitch) {
        (void)name; (void)pitch;
        return false;
    }

    bool set_looping(const std::string& name, bool loop) {
        (void)name; (void)loop;
        return false;
    }

    bool set_position(const std::string& name, float x, float y, float z) {
        (void)name; (void)x; (void)y; (void)z;
        return false;
    }

    void set_listener_position(float x, float y, float z) {
        std::lock_guard<std::mutex> lock(mutex_);
        listener_position_ = Vec3(x, y, z);
    }
    
    void set_listener_orientation(float fx, float fy, float fz, float ux, float uy, float uz) {
        std::lock_guard<std::mutex> lock(mutex_);
        listener_forward_ = Vec3(fx, fy, fz).normalized();
        listener_up_ = Vec3(ux, uy, uz).normalized();
    }
    
    bool is_playing(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = sources_.find(name);
        return it != sources_.end() && it->second.state == AudioState::Playing;
    }
    
    float get_volume() const {
        return master_volume_;
    }
    
    bool set_master_volume(float volume) {
        if (!std::isfinite(volume)) return false;
        const float clamped = std::clamp(volume, 0.0f, 1.0f);
        if (!hwave_out_) return false;
        const DWORD channel = static_cast<DWORD>(clamped * 65535.0f);
        const DWORD packed = channel | (channel << 16);
        if (waveOutSetVolume(hwave_out_, packed) != MMSYSERR_NOERROR) return false;
        master_volume_ = clamped;
        return true;
    }

    bool apply_reverb(const std::string& name, float room_size, float damping, float decay, float diffusion) {
        (void)name; (void)room_size; (void)damping; (void)decay; (void)diffusion;
        return false;
    }

private:
    bool loadWavFile(const std::string& path, AudioClip& clip) {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) return false;
        
        // Read WAV header
        char riff[4];
        file.read(riff, 4);
        if (strncmp(riff, "RIFF", 4) != 0) return false;
        
        uint32_t fileSize;
        file.read((char*)&fileSize, 4);
        
        char wave[4];
        file.read(wave, 4);
        if (strncmp(wave, "WAVE", 4) != 0) return false;
        
        // Find fmt chunk
        char chunkId[4];
        uint32_t chunkSize;
        uint16_t bitsPerSample = 0;
        bool haveFormat = false;
        while (file.read(chunkId, 4)) {
            file.read((char*)&chunkSize, 4);
            
            if (strncmp(chunkId, "fmt ", 4) == 0) {
                if (haveFormat || chunkSize < 16) return false;
                uint16_t format, channels;
                uint32_t sampleRate, byteRate;
                uint16_t blockAlign;
                
                file.read((char*)&format, 2);
                file.read((char*)&channels, 2);
                file.read((char*)&sampleRate, 4);
                file.read((char*)&byteRate, 4);
                file.read((char*)&blockAlign, 2);
                file.read((char*)&bitsPerSample, 2);
                
                if (!file || format != 1 || channels == 0 || channels > 2 ||
                    sampleRate == 0 || (bitsPerSample != 8 && bitsPerSample != 16) ||
                    blockAlign != channels * (bitsPerSample / 8u) ||
                    byteRate != sampleRate * blockAlign) return false;
                clip.sampleRate = sampleRate;
                clip.channels = channels;
                haveFormat = true;
                
                if (chunkSize > 16) {
                    file.seekg(chunkSize - 16, std::ios::cur);
                }
            } else if (strncmp(chunkId, "data", 4) == 0) {
                if (!haveFormat || clip.channels == 0 || (bitsPerSample != 8 && bitsPerSample != 16)) return false;
                constexpr uint32_t kMaxPcmBytes = 256u * 1024u * 1024u;
                const uint32_t bytesPerFrame = clip.channels * (bitsPerSample / 8u);
                if (chunkSize == 0 || chunkSize > kMaxPcmBytes ||
                    bytesPerFrame == 0 || chunkSize % bytesPerFrame != 0) return false;
                std::vector<uint8_t> raw(chunkSize);
                if (!file.read((char*)raw.data(), chunkSize)) return false;
                
                clip.lengthSamples = chunkSize / (clip.channels * (bitsPerSample / 8));
                clip.data.resize(clip.lengthSamples * clip.channels);
                
                if (bitsPerSample == 16) {
                    const short* samples = (const short*)raw.data();
                    for (size_t i = 0; i < clip.lengthSamples * clip.channels; i++) {
                        clip.data[i] = samples[i] / 32768.0f;
                    }
                } else if (bitsPerSample == 8) {
                    for (size_t i = 0; i < clip.lengthSamples * clip.channels; i++) {
                        clip.data[i] = (raw[i] - 128) / 128.0f;
                    }
                }
                break;
            } else {
                file.seekg(chunkSize, std::ios::cur);
            }
            if (chunkSize & 1u) file.seekg(1, std::ios::cur);
            if (!file) return false;
        }
        
        return clip.data.size() > 0;
    }
    
    struct PlaybackBuffer {
        std::string source;
        std::vector<short> pcm;
        WAVEHDR header{};
        bool prepared = false;
    };

    bool playBufferLocked(const std::string& source, const std::shared_ptr<AudioClip>& clip) {
        if (!hwave_out_ || !clip || clip->data.empty()) return false;

        auto buffer = std::make_unique<PlaybackBuffer>();
        buffer->source = source;
        buffer->pcm.resize(clip->data.size());
        for (size_t i = 0; i < clip->data.size(); ++i) {
            const float sample = std::clamp(clip->data[i], -1.0f, 1.0f);
            buffer->pcm[i] = static_cast<short>(sample * 32767.0f);
        }

        buffer->header.lpData = reinterpret_cast<LPSTR>(buffer->pcm.data());
        buffer->header.dwBufferLength = static_cast<DWORD>(buffer->pcm.size() * sizeof(short));
        if (waveOutPrepareHeader(hwave_out_, &buffer->header, sizeof(WAVEHDR)) != MMSYSERR_NOERROR) {
            return false;
        }
        buffer->prepared = true;
        if (waveOutWrite(hwave_out_, &buffer->header, sizeof(WAVEHDR)) != MMSYSERR_NOERROR) {
            waveOutUnprepareHeader(hwave_out_, &buffer->header, sizeof(WAVEHDR));
            return false;
        }

        buffers_.push_back(std::move(buffer));
        return true;
    }

    void releaseBuffersLocked(bool force) {
        for (auto it = buffers_.begin(); it != buffers_.end();) {
            PlaybackBuffer& buffer = **it;
            if (!force && (buffer.header.dwFlags & WHDR_DONE) == 0) {
                ++it;
                continue;
            }
            if (buffer.prepared && hwave_out_) {
                const MMRESULT result = waveOutUnprepareHeader(hwave_out_, &buffer.header, sizeof(WAVEHDR));
                if (!force && result == WAVERR_STILLPLAYING) {
                    ++it;
                    continue;
                }
                buffer.prepared = false;
            }
            auto source = sources_.find(buffer.source);
            if (source != sources_.end()) {
                source->second.state = AudioState::Stopped;
                source->second.currentTime = 0.0f;
            }
            it = buffers_.erase(it);
        }
    }

    HWAVEOUT hwave_out_ = nullptr;
    WAVEFORMATEX wfx_ = {};
    std::unordered_map<std::string, std::shared_ptr<AudioClip>> clips_;
    std::unordered_map<std::string, AudioSource> sources_;
    Vec3 listener_position_ = Vec3::zero();
    Vec3 listener_forward_ = Vec3::forward();
    Vec3 listener_up_ = Vec3::up();
    float master_volume_ = 1.0f;
    bool initialized_ = false;
    std::vector<std::unique_ptr<PlaybackBuffer>> buffers_;
    mutable std::mutex mutex_;
};

} // namespace litt
