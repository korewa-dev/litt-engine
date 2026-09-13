// Profiler implementation
#include "litt_profiler.h"

namespace litt {

void Profiler::begin_sample(const std::string& name) {
    ProfileSample sample;
    sample.name = name;
    sample.start = std::chrono::high_resolution_clock::now();
    active_samples_[name] = sample;
}

void Profiler::end_sample(const std::string& name) {
    auto it = active_samples_.find(name);
    if (it == active_samples_.end()) return;
    it->second.end = std::chrono::high_resolution_clock::now();
    it->second.duration_ms = std::chrono::duration<double, std::milli>(it->second.end - it->second.start).count();
    auto& s = stats_[name];
    s.name = name;
    s.total_time_ms += it->second.duration_ms;
    s.sample_count++;
    if (it->second.duration_ms > s.max_time_ms) s.max_time_ms = it->second.duration_ms;
    if (s.sample_count == 1 || it->second.duration_ms < s.min_time_ms) s.min_time_ms = it->second.duration_ms;
    s.avg_time_ms = s.total_time_ms / s.sample_count;
    active_samples_.erase(it);
}

ProfilerStats Profiler::get_stats(const std::string& name) const {
    auto it = stats_.find(name);
    return it != stats_.end() ? it->second : ProfilerStats{};
}

std::vector<ProfilerStats> Profiler::get_all_stats() const {
    std::vector<ProfilerStats> result;
    for (const auto& [name, stats] : stats_) result.push_back(stats);
    return result;
}

void Profiler::reset() {
    stats_.clear();
    active_samples_.clear();
}

void Profiler::update_fps() {
    auto now = std::chrono::high_resolution_clock::now();
    frame_time_ms_ = std::chrono::duration<double, std::milli>(now - frame_start_).count();
    frame_start_ = now;
    fps_ = frame_time_ms_ > 0 ? 1000.0 / frame_time_ms_ : 0.0;
}

} // namespace litt
