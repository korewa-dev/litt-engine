// Main include header for Litt Engine C++ core
// Include this single header to use all libraries

#pragma once

// Core math library
#include "littcore/litt_math.h"

// Entity Component System
#include "littcore/litt_ecs.h"

// Input handling
#include "littcore/litt_input.h"

// World simulation
#include "littcore/litt_world.h"

// Scene management
#include "littcore/litt_scene.h"

// Physics system
#include "littcore/litt_physics.h"

// Audio system
#include "littcore/litt_audio.h"

// UI system
#include "littcore/litt_ui.h"

// Configuration
#include "littcore/litt_config.h"

// Profiler
#include "littcore/litt_profiler.h"

// Legacy C APIs
#include "littcore/litt_json.h"
#include "littcore/litt_obj.h"
#include "littcore/litt_world.h"

// Convenience namespace
namespace litt {
    using namespace ::litt;
}
