#include "litt_engine.h"

#include <cstdio>

using namespace litt;

static int passed = 0;
static int failed = 0;

static void check(bool cond, const char* name) {
    if (cond) { ++passed; std::printf("  ok %s\n", name); }
    else { ++failed; std::printf("  FAIL %s\n", name); }
}

int main() {
    std::printf("[Engine lifecycle contract]\n");

    EngineConfig defaults;
    check(defaults.headless, "engine_defaults_headless");

    Engine engine;
    check(!engine.is_initialized() && !engine.is_running(), "engine_initial_state");
    check(engine.initialize(defaults), "engine_default_initialize");
    check(engine.is_initialized() && engine.is_running(), "engine_initialized_state");

    engine.stop();
    check(!engine.is_running() && engine.is_initialized(), "engine_stop");
    engine.shutdown();
    check(!engine.is_initialized() && !engine.is_running(), "engine_shutdown");

    check(engine.initialize(defaults), "engine_reinitialize_after_shutdown");
    engine.shutdown();

    EngineConfig invalid = defaults;
    invalid.width = 0;
    check(!engine.initialize(invalid), "engine_rejects_zero_width");

    invalid = defaults;
    invalid.target_fps = 0.0f;
    check(!engine.initialize(invalid), "engine_rejects_zero_fps");

    EngineConfig windowed = defaults;
    windowed.headless = false;
    check(!engine.initialize(windowed), "engine_unavailable_renderer_fails_honestly");

    uint32_t value = 0;
    check(parse_engine_u32("1280", value) && value == 1280, "engine_parse_width");
    check(!parse_engine_u32("-1", value) && !parse_engine_u32("abc", value),
          "engine_parse_width_rejects_invalid");

    float fps = 0.0f;
    check(parse_engine_fps("120", fps) && fps == 120.0f, "engine_parse_fps");
    check(!parse_engine_fps("nan", fps) && !parse_engine_fps("0", fps),
          "engine_parse_fps_rejects_invalid");

    std::printf("\nResults: %d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
