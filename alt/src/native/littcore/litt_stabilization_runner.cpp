// Transitional runner keeps the historical stabilization suite intact while
// replacing its obsolete scene-persistence-unavailable assertion. Remove this
// wrapper once the legacy test file is consolidated.
#define main litt_legacy_stabilization_main
#include "litt_stabilization_tests.cpp"
#undef main

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
    test_asset_facade_truthfulness();
#ifdef _WIN32
    test_software_renderer_hardening();
#endif
    test_quaternion_transform_contract();
    test_scene_component_ownership();
    test_scene_lifecycle_current_contract();
    std::printf("\nResults: %d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
