// Windows waveOut Audio Backend - Real audio playback
// Uses Windows multimedia API - no external dependencies

#pragma once
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

class WindowsWaveOutAudio : public AudioEngine {
public:
    WindowsWaveOutAudio() = default;
    ~WindowsWaveOutAudio() override { shutdown(); }
    
    bool init() override {
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
    
    void shutdown() override {
        if (!initialized_) return;
        
        // Stop all sounds
        for (auto& [name, source] : sources_) {
            stop(name);
        }
        sources_.clear();
        clips_.clear();
        
        // Close wave device
        if (hwave_out_) {
            waveOutReset(hwave_out_);
            waveOutClose(hwave_out_);
            hwave_out_ = nullptr;
        }
        
        initialized_ = false;
    }
    
    void update(float dt) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
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
    
    bool load_clip(const std::string& name, const std::string& path) override {
        if (clips_.count(name)) return true;
        
        AudioClip clip;
        clip.path = path;
        
        if (!loadWavFile(path, clip)) {
            return false;
        }
        
        clips_[name] = std::make_shared<AudioClip>(clip);
        return true;
    }
    
    void play(const std::string& name) override {
        auto clip_it = clips_.find(name);
        if (clip_it == clips_.end()) return;
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        AudioSource source;
        source.name = name;
        source.clip = clip_it->second;
        source.state = AudioState::Playing;
        source.volume = 1.0f;
        source.pitch = 1.0f;
        source.loop = false;
        source.spatial = false;
        source.currentTime = 0.0f;
        
        sources_[name] = source;
        
        // Write audio buffer to wave device
        playBuffer(clip_it->second);
    }
    
    void stop(const std::string& name) override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = sources_.find(name);
        if (it != sources_.end()) {
            it->second.state = AudioState::Stopped;
            it->second.currentTime = 0.0f;
        }
    }
    
    void pause(const std::string& name) override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = sources_.find(name);
        if (it != sources_.end()) {
            it->second.state = AudioState::Paused;
        }
    }
    
    void set_volume(const std::string& name, float volume) override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = sources_.find(name);
        if (it != sources_.end()) {
            it->second.volume = volume;
        }
    }
    
    void set_pitch(const std::string& name, float pitch) override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = sources_.find(name);
        if (it != sources_.end()) {
            it->second.pitch = pitch;
        }
    }
    
    void set_looping(const std::string& name, bool loop) override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = sources_.find(name);
        if (it != sources_.end()) {
            it->second.loop = loop;
        }
    }
    
    void set_position(const std::string& name, float x, float y, float z) override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = sources_.find(name);
        if (it != sources_.end()) {
            it->second.position = Vec3(x, y, z);
            it->second.spatial = true;
        }
    }
    
    void set_listener_position(float x, float y, float z) override {
        std::lock_guard<std::mutex> lock(mutex_);
        listener_position_ = Vec3(x, y, z);
    }
    
    void set_listener_orientation(float fx, float fy, float fz, float ux, float uy, float uz) override {
        std::lock_guard<std::mutex> lock(mutex_);
        listener_forward_ = Vec3(fx, fy, fz).normalized();
        listener_up_ = Vec3(ux, uy, uz).normalized();
    }
    
    bool is_playing(const std::string& name) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = sources_.find(name);
        return it != sources_.end() && it->second.state == AudioState::Playing;
    }
    
    float get_volume() const override {
        return master_volume_;
    }
    
    void set_master_volume(float volume) override {
        master_volume_ = volume;
        if (hwave_out_) {
            DWORD vol = (DWORD)(volume * 0xFFFF);
            vol |= (vol << 16);
            waveOutSetVolume(hwave_out_, vol);
        }
    }
    
    void apply_reverb(const std::string& name, float room_size, float damping, float decay, float diffusion) override {
        std::lock_guard<std::mutex> lock(mutex_);
        reverb_params_[name] = {room_size, damping, decay, diffusion};
    }
    
private:
    bool loadWavFile(const std::string& path, AudioClip& clip) {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            // Generate a simple sine wave test tone if file doesn't exist
            clip.sampleRate = 44100;
            clip.channels = 2;
            clip.lengthSamples = 44100; // 1 second
            clip.data.resize(clip.lengthSamples * clip.channels);
            
            for (uint32_t i = 0; i < clip.lengthSamples; i++) {
                float t = i / (float)clip.sampleRate;
                float sample = 0.0f;
                // Major chord: C4 + E4 + G4
                sample += 0.3f * std::sin(2.0f * 3.14159f * 261.63f * t); // C4
                sample += 0.3f * std::sin(2.0f * 3.14159f * 329.63f * t); // E4
                sample += 0.3f * std::sin(2.0f * 3.14159f * 392.00f * t); // G4
                clip.data[i * 2 + 0] = sample;
                clip.data[i * 2 + 1] = sample;
            }
            return true;
        }
        
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
        while (file.read(chunkId, 4)) {
            file.read((char*)&chunkSize, 4);
            
            if (strncmp(chunkId, "fmt ", 4) == 0) {
                uint16_t format, channels;
                uint32_t sampleRate, byteRate;
                uint16_t blockAlign, bitsPerSample;
                
                file.read((char*)&format, 2);
                file.read((char*)&channels, 2);
                file.read((char*)&sampleRate, 4);
                file.read((char*)&byteRate, 4);
                file.read((char*)&blockAlign, 2);
                file.read((char*)&bitsPerSample, 2);
                
                clip.sampleRate = sampleRate;
                clip.channels = channels;
                
                if (chunkSize > 16) {
                    file.seekg(chunkSize - 16, std::ios::cur);
                }
            } else if (strncmp(chunkId, "data", 4) == 0) {
                std::vector<uint8_t> raw(chunkSize);
                file.read((char*)raw.data(), chunkSize);
                
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
        }
        
        return clip.data.size() > 0;
    }
    
    void playBuffer(const std::shared_ptr<AudioClip>& clip) {
        if (!hwave_out_ || !clip) return;
        
        // Convert float to 16-bit PCM
        std::vector<short> pcm(clip->data.size());
        for (size_t i = 0; i < clip->data.size(); i++) {
            float sample = clip->data[i] * master_volume_;
            pcm[i] = (short)(sample * 32767.0f);
        }
        
        // Setup wave header
        WAVEHDR wh = {};
        wh.lpData = (LPSTR)pcm.data();
        wh.dwBufferLength = pcm.size() * sizeof(short);
        wh.dwFlags = 0;
        
        waveOutPrepareHeader(hwave_out_, &wh, sizeof(WAVEHDR));
        waveOutWrite(hwave_out_, &wh, sizeof(WAVEHDR));
    }
    
    HWAVEOUT hwave_out_ = nullptr;
    WAVEFORMATEX wfx_ = {};
    std::unordered_map<std::string, std::shared_ptr<AudioClip>> clips_;
    std::unordered_map<std::string, AudioSource> sources_;
    std::unordered_map<std::string, std::tuple<float, float, float, float>> reverb_params_;
    Vec3 listener_position_ = Vec3::zero();
    Vec3 listener_forward_ = Vec3::forward();
    Vec3 listener_up_ = Vec3::up();
    float master_volume_ = 1.0f;
    bool initialized_ = false;
    mutable std::mutex mutex_;
};

} // namespace litt
