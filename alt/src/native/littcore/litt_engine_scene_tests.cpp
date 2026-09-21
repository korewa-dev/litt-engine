#include <cstdio>
#include <fstream>
#include <string>
#include "litt_engine.h"

using namespace litt;

static int passed = 0;
static int failed = 0;
static void check(bool condition, const char* name) {
    if (condition) { ++passed; std::printf("  ok %s\n", name); }
    else { ++failed; std::printf("  FAIL %s\n", name); }
}

int main() {
    const char* path = "litt_engine_scene_contract.json";
    const char* missing = "litt_engine_scene_missing.json";
    std::remove(path);
    std::remove(missing);

    Engine engine;
    EngineConfig config;
    config.headless = true;
    check(engine.initialize(config), "engine_headless_initialize");

    Scene* scene = engine.scene_manager().getActiveScene();
    check(scene != nullptr, "engine_has_active_scene");
    SceneNode& child = scene->createNode("Facade child");
    child.position = Vec3(4.0f, 5.0f, 6.0f);
    child.visible = false;
    check(scene->setParent(child.id, scene->root->id), "engine_scene_parent_setup");
    scene->update();

    check(engine.save_scene(path), "engine_scene_save");
    {
        std::ifstream saved(path, std::ios::binary);
        check(saved.good() && saved.peek() != std::ifstream::traits_type::eof(),
              "engine_scene_save_nonempty_file");
    }

    child.position = Vec3(99.0f, 99.0f, 99.0f);
    child.visible = true;
    check(engine.load_scene(path), "engine_scene_load");
    Scene* restored = engine.scene_manager().getActiveScene();
    SceneNode* restored_child = restored ? restored->getNode("Facade child") : nullptr;
    check(restored_child && restored_child->position == Vec3(4.0f, 5.0f, 6.0f) &&
          !restored_child->visible && restored_child->parent == restored->root,
          "engine_scene_roundtrip_state");

    // Existing destinations are replaceable, not append-only.
    if (restored_child) restored_child->position = Vec3(7.0f, 8.0f, 9.0f);
    check(engine.save_scene(path), "engine_scene_replace_existing_save");
    if (restored_child) restored_child->position = Vec3::zero();
    check(engine.load_scene(path), "engine_scene_reload_replacement");
    restored_child = restored ? restored->getNode("Facade child") : nullptr;
    check(restored_child && restored_child->position == Vec3(7.0f, 8.0f, 9.0f),
          "engine_scene_replacement_contents");

    const std::string before_bad = restored ? restored->serializeToJson() : std::string();
    {
        std::ofstream bad(path, std::ios::binary | std::ios::trunc);
        bad << "{not valid json";
    }
    check(!engine.load_scene(path), "engine_scene_malformed_load_fails");
    check(restored && restored->serializeToJson() == before_bad,
          "engine_scene_malformed_load_nonmutating");
    check(!engine.load_scene(missing), "engine_scene_missing_file_fails");
    check(!engine.save_scene(""), "engine_scene_empty_save_path_fails");

    const char* oversized_path = "litt_engine_scene_oversized.json";
    {
        std::ofstream oversized_file(oversized_path, std::ios::binary | std::ios::trunc);
        oversized_file.seekp(static_cast<std::streamoff>(Scene::MAX_SERIALIZED_BYTES));
        oversized_file.put('x');
    }
    const std::string before_oversized = restored ? restored->serializeToJson() : std::string();
    check(!engine.load_scene(oversized_path), "engine_scene_oversized_file_fails");
    check(restored && restored->serializeToJson() == before_oversized,
          "engine_scene_oversized_file_nonmutating");

    engine.stop();
    check(!engine.is_running(), "engine_stop_state");
    check(engine.initialize(config), "engine_reinitialize");
    check(engine.is_running(), "engine_reinitialize_resets_running");
    Scene* reinitialized = engine.scene_manager().getActiveScene();
    check(reinitialized && reinitialized->nodes.size() == 1 &&
          reinitialized->root && reinitialized->root->name == "Root",
          "engine_reinitialize_resets_scene_manager");

    engine.shutdown();
    check(!engine.is_running(), "engine_shutdown_clears_running");
    std::remove(path);
    std::remove(oversized_path);
    std::remove((std::string(path) + ".litt-tmp").c_str());

    std::printf("Engine scene facade: %d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
