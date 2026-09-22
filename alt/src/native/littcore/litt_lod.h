// Litt Engine - bounded level-of-detail system
#pragma once

#include "litt_math.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace litt {

struct LODLevel {
    float distance_threshold = 0.0f;
    uint32_t index_count = 0;
    uint32_t vertex_count = 0;
    float quality = 1.0f;
};

class LODGroup {
public:
    bool add_level(const LODLevel& level) {
        if (!std::isfinite(level.distance_threshold) || level.distance_threshold < 0.0f ||
            !std::isfinite(level.quality) || level.quality < 0.0f || level.quality > 1.0f ||
            levels_.size() >= kMaxLevels) {
            return false;
        }
        levels_.push_back(level);
        std::stable_sort(levels_.begin(), levels_.end(),
            [](const LODLevel& a, const LODLevel& b) {
                return a.distance_threshold < b.distance_threshold;
            });
        return true;
    }

    uint32_t get_lod_for_distance(float distance) const {
        if (levels_.empty()) return kInvalidLod;
        if (!std::isfinite(distance) || distance < 0.0f) return 0;
        for (uint32_t i = 0; i < static_cast<uint32_t>(levels_.size()); ++i) {
            if (distance < levels_[i].distance_threshold) return i;
        }
        return static_cast<uint32_t>(levels_.size() - 1u);
    }

    const LODLevel* get_level(uint32_t index) const {
        return index < levels_.size() ? &levels_[index] : nullptr;
    }

    uint32_t get_level_count() const {
        return static_cast<uint32_t>(levels_.size());
    }

    float get_lod_factor(float distance) const {
        if (levels_.size() <= 1u) return 0.0f;
        const uint32_t lod = get_lod_for_distance(distance);
        if (lod == kInvalidLod) return 0.0f;
        return static_cast<float>(lod) /
               static_cast<float>(levels_.size() - 1u);
    }

    static constexpr uint32_t kInvalidLod = std::numeric_limits<uint32_t>::max();
    static constexpr size_t kMaxLevels = 32u;

private:
    std::vector<LODLevel> levels_;
};

struct LODComponent {
    LODGroup* group = nullptr;
    Vec3 world_position = Vec3::zero();
    uint32_t current_lod = 0;
    float distance_to_camera = 0.0f;

    void update_lod(const Vec3& camera_pos, float bias = 1.0f) {
        if (!group || !std::isfinite(bias) || bias <= 0.0f) {
            current_lod = 0;
            distance_to_camera = 0.0f;
            return;
        }
        const Vec3 delta = world_position - camera_pos;
        distance_to_camera = delta.length();
        if (!std::isfinite(distance_to_camera)) {
            distance_to_camera = 0.0f;
            current_lod = 0;
            return;
        }
        current_lod = group->get_lod_for_distance(distance_to_camera * bias);
        if (current_lod == LODGroup::kInvalidLod) current_lod = 0;
    }
};

class LODSystem {
public:
    static constexpr size_t kMaxGroups = 1024u;
    static constexpr size_t kMaxComponents = 65536u;

    static LODSystem& get_instance() {
        static LODSystem instance;
        return instance;
    }

    LODGroup* create_group(const std::string& name) {
        if (name.empty() || groups_.size() >= kMaxGroups || groups_.count(name)) return nullptr;
        auto group = std::make_unique<LODGroup>();
        LODGroup* raw = group.get();
        groups_.emplace(name, std::move(group));
        return raw;
    }

    LODGroup* get_group(const std::string& name) {
        const auto it = groups_.find(name);
        return it == groups_.end() ? nullptr : it->second.get();
    }

    const LODGroup* get_group(const std::string& name) const {
        const auto it = groups_.find(name);
        return it == groups_.end() ? nullptr : it->second.get();
    }

    bool remove_group(const std::string& name) {
        auto it = groups_.find(name);
        if (it == groups_.end()) return false;
        LODGroup* victim = it->second.get();
        for (LODComponent* component : components_) {
            if (component && component->group == victim) component->group = nullptr;
        }
        groups_.erase(it);
        return true;
    }

    bool register_component(LODComponent* component) {
        if (!component || components_.size() >= kMaxComponents ||
            std::find(components_.begin(), components_.end(), component) != components_.end()) {
            return false;
        }
        components_.push_back(component);
        return true;
    }

    bool unregister_component(LODComponent* component) {
        const auto it = std::find(components_.begin(), components_.end(), component);
        if (it == components_.end()) return false;
        components_.erase(it);
        return true;
    }

    void update(const Vec3& camera_pos) {
        if (!enabled_) return;
        for (LODComponent* component : components_) {
            if (component) component->update_lod(camera_pos, lod_bias_);
        }
    }

    bool set_lod_bias(float bias) {
        if (!std::isfinite(bias) || bias <= 0.0f || bias > 16.0f) return false;
        lod_bias_ = bias;
        return true;
    }

    float get_lod_bias() const { return lod_bias_; }
    void set_enabled(bool enabled) { enabled_ = enabled; }
    bool is_enabled() const { return enabled_; }

    void clear() {
        components_.clear();
        groups_.clear();
        lod_bias_ = 1.0f;
        enabled_ = true;
    }

private:
    LODSystem() = default;
    std::unordered_map<std::string, std::unique_ptr<LODGroup>> groups_;
    std::vector<LODComponent*> components_;
    float lod_bias_ = 1.0f;
    bool enabled_ = true;
};

} // namespace litt
