// Main include header for Litt Engine C++ core
// Include this single header to use all libraries

#pragma once

// Core math library
#include "litt_math.h"

// Entity Component System
#include "litt_ecs.h"

// Input handling
#include "litt_input.h"

// World simulation
#include "litt_world.h"

// Scene management
#include "litt_scene.h"

// Physics system
#include "litt_physics.h"

// Audio system
#include "litt_audio.h"

// UI system
#include "litt_ui.h"

// Configuration
#include "litt_config.h"

// Profiler
#include "litt_profiler.h"

// Legacy C APIs
#include "litt_json.h"
#include "litt_obj.h"
#include "litt_world.h"

// Convenience namespace
namespace litt {
    using namespace ::litt;
}
