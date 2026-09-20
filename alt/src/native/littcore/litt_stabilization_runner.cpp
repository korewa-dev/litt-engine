// Transitional runner keeps the historical stabilization suite intact while
// replacing its obsolete scene-persistence-unavailable assertion. Remove this
// wrapper once the legacy test file is consolidated.
#define main litt_legacy_stabilization_main
#include "litt_stabilization_tests.cpp"
#undef main
#ifdef _WIN32
#include "litt_audio_wav.h"
#endif

static void test_scene_lifecycle_current_contract() {
    SceneManager manager;
    Scene& first = manager.createScene("Level");
    manager.setActiveScene("Level");
    Scene& duplicate = manager.createScene("Level");
    check(&first == &duplicate && manager.getActiveScene() == &first,
          "scene_duplicate_does_not_replace_active");

    SceneNode& parent = first.createNode("Parent");
    SceneNode& child = first.createNode("Child");
    const uint32_t parent_id = parent.id;
    const uint32_t child_id = child.id;
    check(first.setParent(child_id, parent_id) && child.parent == &parent &&
          parent.children.size() == 1 && parent.children[0] == &child &&
          first.getNode(child_id) == &child,
          "scene_registry_owns_hierarchy_nodes");
    check(!first.setParent(parent_id, child_id), "scene_hierarchy_rejects_cycles");
    first.removeNode(parent_id);
    check(first.getNode(parent_id) == nullptr && first.getNode(child_id) == nullptr,
          "scene_remove_parent_removes_owned_subtree");
    check(first.root != nullptr && first.getNode(first.root->id) == first.root,
          "scene_root_survives_subtree_removal");

    const std::string json = first.serializeToJson();
    check(!json.empty(), "scene_serialize_available");
    Scene restored;
    check(restored.deserializeFromJson(json), "scene_deserialize_available");
    check(!first.deserializeFromJson("{}"), "scene_deserialize_rejects_invalid_contract");
}

#ifdef _WIN32
static void test_windows_audio_failure_contracts() {
    WindowsWaveOutAudio audio;
    check(!audio.load_clip("missing", "litt_definitely_missing_audio.wav"),
          "windows_audio_missing_file_fails_honestly");
    check(!audio.pause("missing"), "windows_audio_pause_reports_unsupported");
    check(!audio.stop("missing"), "windows_audio_stop_missing_rejected");
    check(!audio.set_pitch("missing", 2.0f), "windows_audio_pitch_reports_unsupported");
    check(!audio.set_volume("missing", 0.5f), "windows_audio_source_volume_reports_unsupported");
    check(!audio.set_looping("missing", true), "windows_audio_loop_reports_unsupported");
    check(!audio.set_position("missing", 1.0f, 2.0f, 3.0f),
          "windows_audio_spatial_reports_unsupported");
    check(!audio.apply_reverb("missing", 1.0f, 1.0f, 1.0f, 1.0f),
          "windows_audio_reverb_reports_unsupported");
}
#endif

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::printf("[Stabilization]\n");
    test_physics_normals();
    test_physics_registration();
    test_physics_resolution();
    test_physics_invalid_inputs();
    test_memory();
    test_ecs_generations();
    test_affine_inverse();
    test_scripting_vm();
    test_event_dispatcher();
    test_audio_wav_loading();
#ifdef _WIN32
    test_software_renderer_hardening();
    test_windows_audio_failure_contracts();
#endif
    test_quaternion_transform_contract();
    test_scene_component_ownership();
    test_scene_lifecycle_current_contract();
    std::printf("\nResults: %d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
