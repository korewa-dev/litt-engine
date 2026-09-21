// Litt Engine - systems umbrella
//
// Historical versions of this file duplicated dozens of real engine modules with
// no-op "blueprint" classes. That made compilation look complete while runtime
// behavior remained stubbed. The live header is now only an umbrella over the
// canonical implementations. New functionality belongs in its owning module.
#pragma once

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include "litt_math.h"
#include "litt_ecs.h"
#include "litt_event.h"
#include "litt_memory.h"
#include "litt_memory_pool.h"
#include "litt_scene.h"
#include "litt_engine.h"
#include "litt_renderer.h"
#include "litt_gpu.h"
#include "litt_gpu_software.h"
#include "litt_lighting.h"
#include "litt_material.h"
#include "litt_asset.h"
#include "litt_asset_pipeline.h"
#include "litt_physics.h"
#include "litt_audio.h"
#include "litt_input.h"
#include "litt_ui.h"
#include "litt_profiler.h"
#include "litt_lod.h"
#include "litt_serialization.h"
#include "litt_scripting_vm.h"
#include "litt_networking.h"
#include "litt_config.h"
#include "litt_world.h"

namespace litt {

struct EngineSystemsContract final {
    static constexpr uint32_t version = 1u;
    static constexpr const char* description =
        "canonical module umbrella; no duplicate placeholder subsystems";
};

} // namespace litt
