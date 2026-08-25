// LittProfiler - Performance profiling for Litt Engine
// Replaces crates/profiler/lib.rs

#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <iostream>
#include <algorithm>

namespace litt {

class Timer {
public:
    Timer() : startTime_(std::chrono::high_resolution_clock::now()) {}
    
    void start() {
        startTime_ = std::chrono::high_resolution_clock::now();
    }
    
    float stop() {
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration<float, std::milli>(endTime - startTime_);
        return duration.count();
    }
    
    float elapsed() const {
        auto now = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration<float, std::milli>(now - startTime_);
        return duration.count();
    }
    
private:
    std::chrono::high_resolution_clock::time_point startTime_;
};

struct FrameStats {
    float frameTime = 0.0f;
    float cpuTime = 0.0f;
    float gpuTime = 0.0f;
    int drawCalls = 0;
    int triangles = 0;
    int vertices = 0;
    float fps = 0.0f;
    float frameTimeMs = 0.0f;
    
    void update(float frameTime, int drawCalls, int triangles, int vertices) {
        this->frameTime = frameTime;
        this->drawCalls = drawCalls;
        this->triangles = triangles;
        this->vertices = vertices;
        this->fps = frameTime > 0.0f ? 1000.0f / frameTime : 0.0f;
        this->frameTimeMs = frameTime;
    }
};

class Profiler {
public:
    Profiler() = default;

    /// Process-wide profiler for the LITT_PROFILE convenience macro.
    static Profiler& instance() {
        static Profiler p;
        return p;
    }

    void startFrame() {
        frameTimer_.start();
    }
    
    FrameStats endFrame() {
        float frameTime = frameTimer_.stop();
        FrameStats stats;
        stats.frameTime = frameTime;
        stats.fps = frameTime > 0.0f ? 1000.0f / frameTime : 0.0f;
        stats.frameTimeMs = frameTime;
        
        frameTimes_.push_back(frameTime);
        if (frameTimes_.size() > 60) frameTimes_.erase(frameTimes_.begin());
        
        return stats;
    }
    
    void mark(const std::string& label) {
        auto it = timings_.find(label);
        if (it != timings_.end()) {
            it->second.push_back(std::chrono::high_resolution_clock::now());
        } else {
            timings_[label] = {std::chrono::high_resolution_clock::now()};
        }
    }
    
    void startTimer(const std::string& label) {
        activeTimers_[label] = std::make_unique<Timer>();
        activeTimers_[label]->start();
    }
    
    float stopTimer(const std::string& label) {
        auto it = activeTimers_.find(label);
        if (it != activeTimers_.end()) {
            float elapsed = it->second->stop();
            timings_[label].push_back(std::chrono::high_resolution_clock::now());
            activeTimers_.erase(it);
            return elapsed;
        }
        return 0.0f;
    }
    
    void recordDrawCalls(int count) {
        drawCalls_ = count;
    }
    
    void recordTriangles(int count) {
        triangles_ = count;
    }
    
    void recordVertices(int count) {
        vertices_ = count;
    }
    
    FrameStats getFrameStats() const {
        FrameStats stats;
        stats.frameTime = frameTimes_.empty() ? 0.0f : frameTimes_.back();
        stats.fps = stats.frameTime > 0.0f ? 1000.0f / stats.frameTime : 0.0f;
        stats.drawCalls = drawCalls_;
        stats.triangles = triangles_;
        stats.vertices = vertices_;
        return stats;
    }
    
    float getAverageFrameTime() const {
        if (frameTimes_.empty()) return 0.0f;
        float sum = 0.0f;
        for (auto t : frameTimes_) sum += t;
        return sum / frameTimes_.size();
    }
    
    float getMinFrameTime() const {
        if (frameTimes_.empty()) return 0.0f;
        return *std::min_element(frameTimes_.begin(), frameTimes_.end());
    }
    
    float getMaxFrameTime() const {
        if (frameTimes_.empty()) return 0.0f;
        return *std::max_element(frameTimes_.begin(), frameTimes_.end());
    }
    
    void printReport() const {
        // Guard the empty history: frameTimes_.back() on an empty vector is UB.
        float last = frameTimes_.empty() ? 0.0f : frameTimes_.back();
        std::cout << "=== Profiler Report ===" << std::endl;
        std::cout << "Frame Time: " << last << " ms" << std::endl;
        std::cout << "FPS: " << (last > 0.0f ? 1000.0f / last : 0.0f) << std::endl;
        std::cout << "Draw Calls: " << drawCalls_ << std::endl;
        std::cout << "Triangles: " << triangles_ << std::endl;
        std::cout << "Vertices: " << vertices_ << std::endl;
        std::cout << "=======================" << std::endl;
    }
    
private:
    Timer frameTimer_;
    std::unordered_map<std::string, std::vector<std::chrono::high_resolution_clock::time_point>> timings_;
    std::unordered_map<std::string, std::unique_ptr<Timer>> activeTimers_;
    std::vector<float> frameTimes_;
    int drawCalls_ = 0;
    int triangles_ = 0;
    int vertices_ = 0;
};

// Scoped timer
class ScopedTimer {
public:
    ScopedTimer(const std::string& label, Profiler& profiler)
        : label_(label), profiler_(profiler) {
        profiler_.startTimer(label_);
    }
    
    ~ScopedTimer() {
        profiler_.stopTimer(label_);
    }
    
private:
    std::string label_;
    Profiler& profiler_;
};

} // namespace litt

#define LITT_PROFILE(label) litt::ScopedTimer _prof_##label(#label, litt::Profiler::instance())
