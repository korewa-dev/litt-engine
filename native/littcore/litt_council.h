// ============================================================================
// litt_council.h - Engine Council / Decision System for the Litt Engine
//
// Decides which engine features exist and run, at two levels:
//   * Compile time - LITT_ENABLE_* macros cut whole modules from the binary.
//   * Run time     - a Council of voters loads/unloads features by weighted
//                    majority ballot, applies quality-tier presets, and takes
//                    manual overrides. Platform is detected at compile time.
//
// Header-only C++17, standard library only. All mutable state lives inside a
// Council instance; including this file has no side effects.
// ============================================================================
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#if !defined(__cplusplus) || (__cplusplus < 201703L && !defined(_MSVC_LANG))
#error "litt_council.h requires C++17 (-std=c++17 or /std:c++17)"
#endif

namespace litt {

// --- platform ---------------------------------------------------------------
/// Host platform, resolved at compile time from predefined macros.
enum class Platform : uint8_t { Windows, Linux, Android, Unknown };

constexpr Platform host_platform() noexcept {
#if defined(_WIN32)
    return Platform::Windows;
#elif defined(__ANDROID__)
    return Platform::Android;
#elif defined(__linux__) || defined(__unix__)
    return Platform::Linux;
#else
    return Platform::Unknown;
#endif
}

constexpr const char* to_string(Platform p) noexcept {
    switch (p) {
        case Platform::Windows: return "windows";
        case Platform::Linux:   return "linux";
        case Platform::Android: return "android";
        default:                return "unknown";
    }
}

// --- quality tiers ----------------------------------------------------------
/// Content/rendering quality tiers; the enum rank orders them low -> ultra.
enum class Tier : uint8_t { Low = 0, Medium = 1, High = 2, Ultra = 3 };

constexpr const char* to_string(Tier t) noexcept {
    switch (t) {
        case Tier::Low:    return "low";
        case Tier::Medium: return "medium";
        case Tier::High:   return "high";
        default:           return "ultra";
    }
}

// --- features ---------------------------------------------------------------
/// Modules the Council governs. `Count` is a sentinel, not a feature.
enum class Feature : uint8_t { Math, ECS, Input, Physics, Renderer, Audio, UI, Count };
constexpr size_t kFeatureCount = static_cast<size_t>(Feature::Count);

constexpr const char* to_string(Feature f) noexcept {
    switch (f) {
        case Feature::Math:     return "math";
        case Feature::ECS:      return "ecs";
        case Feature::Input:    return "input";
        case Feature::Physics:  return "physics";
        case Feature::Renderer: return "renderer";
        case Feature::Audio:    return "audio";
        case Feature::UI:       return "ui";
        default:                return "?";
    }
}

// --- compile-time feature flags --------------------------------------------
// Define any flag as 0 (-DLITT_ENABLE_X=0) to drop module X from the binary.
#ifndef LITT_ENABLE_MATH
#define LITT_ENABLE_MATH 1
#endif
#ifndef LITT_ENABLE_ECS
#define LITT_ENABLE_ECS 1
#endif
#ifndef LITT_ENABLE_INPUT
#define LITT_ENABLE_INPUT 1
#endif
#ifndef LITT_ENABLE_PHYSICS
#define LITT_ENABLE_PHYSICS 1
#endif
#ifndef LITT_ENABLE_RENDERER
#define LITT_ENABLE_RENDERER 1
#endif
#ifndef LITT_ENABLE_AUDIO
#define LITT_ENABLE_AUDIO 1
#endif
#ifndef LITT_ENABLE_UI
#define LITT_ENABLE_UI 1
#endif

/// Which modules were compiled into this binary (compile-time truth).
constexpr std::array<bool, kFeatureCount> k_compiled{{
    LITT_ENABLE_MATH != 0,     LITT_ENABLE_ECS != 0,      LITT_ENABLE_INPUT != 0,
    LITT_ENABLE_PHYSICS != 0,  LITT_ENABLE_RENDERER != 0, LITT_ENABLE_AUDIO != 0,
    LITT_ENABLE_UI != 0}};

// --- runtime voting ---------------------------------------------------------
/// One ballot. Zero-initialized storage reads as Abstain.
enum class Vote : int8_t { No = -1, Abstain = 0, Yes = 1 };

/// A Council seat: named voter whose ballots carry `weight`.
struct Voter { const char* name; int weight = 1; };

/// Runtime decision body: tallies weighted ballots and gates each feature.
/// Trivially copyable, no allocation, no locks - safe to embed anywhere.
class Council {
public:
    static constexpr size_t kMaxVoters = 8;

    /// Register a seat; returns its id, or kMaxVoters if the council is full.
    size_t add_voter(const Voter& v) noexcept {
        if (seat_count_ >= kMaxVoters) return kMaxVoters;
        seats_[seat_count_] = v;
        return seat_count_++;
    }

    /// Cast (or re-cast) seat `id`'s ballot on `f`; invalid ids are ignored.
    void vote(size_t id, Feature f, Vote v) noexcept {
        if (id < seat_count_) ballots_[id][static_cast<size_t>(f)] = v;
    }

    /**
     * Tally ballots for `f` and load/unload it accordingly. The motion passes
     * when yes-weight outweighs no-weight AND turnout reaches `quorum` (a
     * fraction of total registered seat weight). Unloading is how running
     * subsystems get switched off mid-session; compiled-out ones stay off.
     */
    bool decide(Feature f, double quorum = 0.5) noexcept {
        const size_t fi = static_cast<size_t>(f);
        int score = 0, turnout = 0, total = 0;
        for (size_t i = 0; i < seat_count_; ++i) {
            total += seats_[i].weight;
            const int b = static_cast<int>(ballots_[i][fi]);
            if (b != 0) { score += seats_[i].weight * b; turnout += seats_[i].weight; }
        }
        set_loaded(f, total > 0 && score > 0 && turnout >= quorum * total);
        return active(f);
    }

    /// Manual load/unload that bypasses the vote entirely.
    void override_feature(Feature f, bool enabled) noexcept { set_loaded(f, enabled); }

    /**
     * Preset runtime loading by quality tier (compiled-in modules only).
     * Low keeps math+input; Medium adds ecs+physics; High adds renderer and
     * audio; Ultra enables everything.
     */
    void apply_tier(Tier t) noexcept {
        for (size_t i = 0; i < kFeatureCount; ++i)
            set_loaded(static_cast<Feature>(i),
                       tier_rank(static_cast<Feature>(i)) <= static_cast<int>(t));
    }

    /// True when module `f` exists in this binary AND is currently loaded.
    bool active(Feature f) const noexcept {
        return compiled(f) && loaded_[static_cast<size_t>(f)];
    }

    /// True when module `f` was compiled into this binary at all.
    static constexpr bool compiled(Feature f) noexcept {
        return k_compiled[static_cast<size_t>(f)];
    }

private:
    /// Lowest tier at which apply_tier() loads each feature.
    static constexpr int tier_rank(Feature f) noexcept {
        switch (f) {
            case Feature::Math:     case Feature::Input:    return 0;  // always on
            case Feature::ECS:      case Feature::Physics:  return 1;  // medium+
            case Feature::Renderer: case Feature::Audio:    return 2;  // high+
            default:                                        return 3;  // ui: ultra
        }
    }

    /// A feature can only ever be loaded if it was compiled in.
    void set_loaded(Feature f, bool on) noexcept {
        loaded_[static_cast<size_t>(f)] = on && compiled(f);
    }

    std::array<Voter, kMaxVoters> seats_{};
    std::array<std::array<Vote, kFeatureCount>, kMaxVoters> ballots_{};  // Abstain
    std::array<bool, kFeatureCount> loaded_{};
    size_t seat_count_ = 0;
};

}  // namespace litt
