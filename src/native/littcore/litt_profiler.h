// Phase 4: Optimization & Performance - Profiler

#pragma once

#include "litt_math.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>

namespace litt {

// Profile sample
struct ProfileSample {
    std::string name;
    std::chrono::high_resolution_clock::time_point start;
    std::chrono::high_resolution_clock::time_point end;
    double duration_ms;
    uint32_t thread_id;
};

// Profiler statistics
struct ProfilerStats {
    std::string name;
    double total_time_ms;
    double avg_time_ms;
    double max_time_ms;
    double min_time_ms;
    uint32_t sample_count;
};

// Profiler
class Profiler {
public:
    static Profiler& get_instance() {
        static Profiler instance;
        return instance;
    }
    
    // Begin sample
    void begin_sample(const std::string& name);
    
    // End sample
    void end_sample(const std::string& name);
    
    // Get stats
    ProfilerStats get_stats(const std::string& name) const;
    
    // Get all stats
    std::vector<ProfilerStats> get_all_stats() const;
    
    // Reset stats
    void reset();
    
    // Get FPS
    double get_fps() const { return fps_; }
    
    // Update FPS (call once per frame)
    void update_fps();
    
    // Get frame time
    double get_frame_time_ms() const { return frame_time_ms_; }

private:
    Profiler() = default;
    
    std::unordered_map<std::string, ProfileSample> active_samples_;
    std::unordered_map<std::string, ProfilerStats> stats_;
    std::chrono::high_resolution_clock::time_point frame_start_;
    double fps_ = 0.0;
    double frame_time_ms_ = 0.0;
};

// Scoped profile timer
class ScopedProfile {
public:
    ScopedProfile(const std::string& name) : name_(name) {
        Profiler::get_instance().begin_sample(name_);
    }
    
    ~ScopedProfile() {
        Profiler::get_instance().end_sample(name_);
    }

private:
    std::string name_;
};

// Macro for easy profiling
#define PROFILE_SCOPE(name) ScopedProfile _profile_##__LINE__(name)
#define PROFILE_FUNCTION() PROFILE_SCOPE(__FUNCTION__)

} // namespace litt
