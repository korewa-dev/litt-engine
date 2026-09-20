#include <cmath>
#include <cstdio>
#include <string>
#include "litt_scene.h"

using namespace litt;
static int passed = 0, failed = 0;
static void check(bool ok, const char* name) { if (ok) { ++passed; std::printf("  ok %s\n", name); } else { ++failed; std::printf("  FAIL %s\n", name); } }
static bool near(float a, float b) { return std::fabs(a-b) < 1e-4f; }

int main() {
    Scene source;
    source.root->name = "Root \"quoted\"";
    SceneNode& parent = source.createNode("Parent");
    SceneNode& child = source.createNode("Child\nLine");
    parent.position = Vec3(1.25f, -2.0f, 3.5f);
    parent.rotation = Quat::from_axis_angle(Vec3::unit_y(), 0.7f);
    parent.scale = Vec3(2.0f, 3.0f, 4.0f);
    parent.visible = false;
    child.cullable = false;
    check(source.setParent(child.id, parent.id), "scene_persistence_parent_setup");
    source.update();

    const std::string json = source.serializeToJson();
    check(!json.empty() && json.find("\"version\":1") != std::string::npos,
          "scene_persistence_serializes_versioned_json");

    Scene restored;
    check(restored.deserializeFromJson(json), "scene_persistence_roundtrip_loads");
    SceneNode* rp = restored.getNode("Parent");
    SceneNode* rc = restored.getNode("Child\nLine");
    check(restored.root && restored.root->name == "Root \"quoted\"",
          "scene_persistence_escapes_names");
    check(rp && rc && rc->parent == rp, "scene_persistence_restores_hierarchy");
    check(rp && near(rp->position.x, 1.25f) && near(rp->position.y, -2.0f) && near(rp->scale.z, 4.0f),
          "scene_persistence_restores_transform");
    check(rp && !rp->visible && rc && !rc->cullable,
          "scene_persistence_restores_flags");

    const size_t before = restored.nodes.size();
    const std::string before_name = restored.root->name;
    check(!restored.deserializeFromJson("{broken"), "scene_persistence_rejects_malformed_json");
    check(restored.nodes.size() == before && restored.root && restored.root->name == before_name,
          "scene_persistence_failure_is_transactional");

    const char* missing_parent = "{\"version\":1,\"root\":0,\"nodes\":[{\"id\":0,\"name\":\"Root\",\"parent\":null,\"position\":[0,0,0],\"rotation\":[0,0,0,1],\"scale\":[1,1,1],\"visible\":true,\"cullable\":true},{\"id\":1,\"name\":\"Child\",\"parent\":99,\"position\":[0,0,0],\"rotation\":[0,0,0,1],\"scale\":[1,1,1],\"visible\":true,\"cullable\":true}]}";
    check(!restored.deserializeFromJson(missing_parent), "scene_persistence_rejects_missing_parent");

    const char* cycle = "{\"version\":1,\"root\":0,\"nodes\":[{\"id\":0,\"name\":\"Root\",\"parent\":null,\"position\":[0,0,0],\"rotation\":[0,0,0,1],\"scale\":[1,1,1],\"visible\":true,\"cullable\":true},{\"id\":1,\"name\":\"A\",\"parent\":2,\"position\":[0,0,0],\"rotation\":[0,0,0,1],\"scale\":[1,1,1],\"visible\":true,\"cullable\":true},{\"id\":2,\"name\":\"B\",\"parent\":1,\"position\":[0,0,0],\"rotation\":[0,0,0,1],\"scale\":[1,1,1],\"visible\":true,\"cullable\":true}]}";
    check(!restored.deserializeFromJson(cycle), "scene_persistence_rejects_cycle");

    const char* nan_value = "{\"version\":1,\"root\":0,\"nodes\":[{\"id\":0,\"name\":\"Root\",\"parent\":null,\"position\":[1e999,0,0],\"rotation\":[0,0,0,1],\"scale\":[1,1,1],\"visible\":true,\"cullable\":true}]}";
    check(!restored.deserializeFromJson(nan_value), "scene_persistence_rejects_nonfinite_transform");

    std::printf("Results: %d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
