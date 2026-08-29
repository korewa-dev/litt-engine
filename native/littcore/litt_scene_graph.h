// LittScene - Scene management and hierarchy for Litt Engine
// Implements scene graph with transform hierarchy and entity management

#pragma once
#include "litt_math.h"
#include "litt_ecs.h"
#include "litt_memory.h"
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>

namespace litt {

// Forward declarations
struct TransformComponent;
class SceneNode;

// =============================================================================
// Scene Node - Represents a transform hierarchy node
// =============================================================================
class SceneNode {
public:
    SceneNode(EntityId entity, const std::string& name = "");
    ~SceneNode();

    // Non-copyable, non-movable
    SceneNode(const SceneNode&) = delete;
    SceneNode& operator=(const SceneNode&) = delete;

    // Getters
    EntityId get_entity() const { return entity_; }
    const std::string& get_name() const { return name_; }
    SceneNode* get_parent() const { return parent_; }
    std::vector<SceneNode*>& get_children() { return children_; }
    const std::vector<SceneNode*>& get_children() const { return children_; }

    // Local transforms
    const Vec3& get_local_position() const { return local_position_; }
    const Quat& get_local_rotation() const { return local_rotation_; }
    const Vec3& get_local_scale() const { return local_scale_; }

    // World transforms
    const Vec3& get_world_position() const { return world_position_; }
    const Mat4& get_world_matrix() const { return world_matrix_; }

    // Set transforms
    void set_position(const Vec3& pos);
    void set_rotation(const Quat& rot);
    void set_scale(const Vec3& scale);
    void translate(const Vec3& delta);
    void rotate(const Quat& delta);
    void scale(const Vec3& factor);

    // Hierarchy management
    void set_parent(SceneNode* parent);
    void add_child(SceneNode* child);
    void remove_child(SceneNode* child);
    void clear_children();
    SceneNode* find_child(const std::string& name) const;
    SceneNode* find_descendant(const std::string& name) const;

    // Update hierarchy
    void mark_dirty();
    void update_matrix_hierarchy();

private:
    EntityId entity_;
    std::string name_;
    SceneNode* parent_ = nullptr;
    std::vector<SceneNode*> children_;

    // Local transforms
    Vec3 local_position_ = Vec3::zero();
    Quat local_rotation_ = Quat(0, 0, 0, 1); // Identity
    Vec3 local_scale_ = Vec3(1, 1, 1);

    // World transforms (updated by update_matrix_hierarchy)
    Vec3 world_position_;
    Mat4 world_matrix_;
    bool matrix_dirty_ = true;
};

// =============================================================================
// Scene - Root of a scene graph
// =============================================================================
class Scene {
public:
    Scene();
    ~Scene();

    // Entity management
    EntityId create_entity(const std::string& name);
    void destroy_entity(EntityId entity);
    bool entity_exists(EntityId entity) const;
    SceneNode* get_node(EntityId entity) const;

    // Node management
    SceneNode* create_node(const std::string& name);
    void destroy_node(SceneNode* node);
    void attach_to_scene(SceneNode* node, SceneNode* parent);

    // Find nodes
    std::vector<SceneNode*> get_all_nodes() const;
    SceneNode* find_by_name(const std::string& name) const;

    // Update
    void update(float delta_time);
    void update_transforms();

    // Save/Load
    void save(const std::string& filepath) const;
    void load(const std::string& filepath);

private:
    std::vector<SceneNode*> root_nodes_;
    std::unordered_map<EntityId, SceneNode*> entity_nodes_;
    std::unordered_map<std::string, SceneNode*> name_nodes_;
};

// =============================================================================
// Scene Utilities
// =============================================================================
namespace SceneUtils {
    // Simple frustum culling (axis-aligned bounding box test)
    bool is_in_frustum(const SceneNode* node, const Mat4 frustum_planes[6]);

    // Get distance from camera
    float distance_to_camera(const SceneNode* node, const Vec3& camera_pos);

    // Find closest node to point
    SceneNode* find_closest(const SceneNode* root, const Vec3& point, float max_distance);
}

} // namespace litt
