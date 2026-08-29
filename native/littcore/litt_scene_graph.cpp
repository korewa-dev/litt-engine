// =============================================================================
// Scene Graph Implementation
// =============================================================================

#include "litt_scene_graph.h"
#include "litt_json.h"
#include <fstream>
#include <algorithm>

namespace litt {

// =============================================================================
// SceneNode Implementation
// =============================================================================

SceneNode::SceneNode(EntityId entity, const std::string& name)
    : entity_(entity), name_(name), world_matrix_(Mat4::identity()) {
}

SceneNode::~SceneNode() {
}

void SceneNode::set_position(const Vec3& pos) {
    local_position_ = pos;
    mark_dirty();
}

void SceneNode::set_rotation(const Quat& rot) {
    local_rotation_ = rot;
    mark_dirty();
}

void SceneNode::set_scale(const Vec3& scale) {
    local_scale_ = scale;
    mark_dirty();
}

void SceneNode::translate(const Vec3& delta) {
    local_position_ = local_position_ + delta;
    mark_dirty();
}

void SceneNode::rotate(const Quat& delta) {
    local_rotation_ = delta * local_rotation_;
    mark_dirty();
}

void SceneNode::scale(const Vec3& factor) {
    local_scale_ = {local_scale_.x * factor.x, local_scale_.y * factor.y, local_scale_.z * factor.z};
    mark_dirty();
}

void SceneNode::set_parent(SceneNode* parent) {
    parent_ = parent;
    mark_dirty();
}

void SceneNode::add_child(SceneNode* child) {
    child->set_parent(this);
}

void SceneNode::remove_child(SceneNode* child) {
    if (!child) return;
    
    std::vector<SceneNode*>& children = get_children();
    auto it = std::find(children.begin(), children.end(), child);
    if (it != children.end()) {
        children.erase(it);
    }
    child->set_parent(nullptr);
}

void SceneNode::clear_children() {
    get_children().clear();
}

SceneNode* SceneNode::find_child(const std::string& name) const {
    for (auto* child : children_) {
        if (child->name_ == name) {
            return child;
        }
    }
    return nullptr;
}

SceneNode* SceneNode::find_descendant(const std::string& name) const {
    for (auto* child : children_) {
        if (child->name_ == name) {
            return child;
        }
        SceneNode* found = child->find_descendant(name);
        if (found) return found;
    }
    return nullptr;
}

void SceneNode::mark_dirty() {
    matrix_dirty_ = true;
}

void SceneNode::update_matrix_hierarchy() {
    if (!matrix_dirty_) return;

    // Calculate local matrix from components
    Mat4 local_matrix =
        Mat4::translation(local_position_) *
        Mat4::scale(local_scale_);

    // Apply rotation from quaternion
    float x = local_rotation_.x;
    float y = local_rotation_.y;
    float z = local_rotation_.z;
    float w = local_rotation_.w;

    Mat4 rot;
    rot.m[0] = 1 - 2*(y*y + z*z);
    rot.m[1] = 2*(x*y + w*z);
    rot.m[2] = 2*(x*z - w*y);
    rot.m[4] = 2*(x*y - w*z);
    rot.m[5] = 1 - 2*(x*x + z*z);
    rot.m[6] = 2*(y*z + w*x);
    rot.m[8] = 2*(x*z + w*y);
    rot.m[9] = 2*(y*z - w*x);
    rot.m[10] = 1 - 2*(x*x + y*y);

    local_matrix = local_matrix * rot;

    // Apply parent's world matrix if exists
    if (parent_) {
        world_matrix_ = parent_->get_world_matrix() * local_matrix;
    } else {
        world_matrix_ = local_matrix;
    }

    // Update world position (column 3, indices 12, 13, 14)
    world_position_ = Vec3(
        world_matrix_.m[12],
        world_matrix_.m[13],
        world_matrix_.m[14]
    );

    matrix_dirty_ = false;
}

// =============================================================================
// Scene Implementation
// =============================================================================

Scene::Scene() {
}

Scene::~Scene() {
    for (auto* node : root_nodes_) {
        delete node;
    }
    root_nodes_.clear();
    
    entity_nodes_.clear();
    name_nodes_.clear();
}

EntityId Scene::create_entity(const std::string& name) {
    EntityId entity = static_cast<EntityId>(entity_nodes_.size() + 1);
    
    SceneNode* node = new SceneNode(entity, name);
    entity_nodes_[entity] = node;
    name_nodes_[name] = node;
    root_nodes_.push_back(node);
    
    return entity;
}

void Scene::destroy_entity(EntityId entity) {
    auto it = entity_nodes_.find(entity);
    if (it == entity_nodes_.end()) return;
    
    SceneNode* node = it->second;
    
    // Remove from name map
    name_nodes_.erase(node->get_name());
    
    // Remove from root nodes if present
    auto root_it = std::find(root_nodes_.begin(), root_nodes_.end(), node);
    if (root_it != root_nodes_.end()) {
        root_nodes_.erase(root_it);
    }
    
    // Remove from parent's children
    if (node->get_parent()) {
        std::vector<SceneNode*>& children = const_cast<std::vector<SceneNode*>&>(
            node->get_parent()->get_children());
        auto cit = std::find(children.begin(), children.end(), node);
        if (cit != children.end()) {
            children.erase(cit);
        }
    }
    
    delete node;
    entity_nodes_.erase(it);
}

bool Scene::entity_exists(EntityId entity) const {
    return entity_nodes_.find(entity) != entity_nodes_.end();
}

SceneNode* Scene::get_node(EntityId entity) const {
    auto it = entity_nodes_.find(entity);
    return (it != entity_nodes_.end()) ? it->second : nullptr;
}

SceneNode* Scene::create_node(const std::string& name) {
    EntityId entity = static_cast<EntityId>(entity_nodes_.size() + 1);
    
    SceneNode* node = new SceneNode(entity, name);
    entity_nodes_[entity] = node;
    name_nodes_[name] = node;
    root_nodes_.push_back(node);
    
    return node;
}

void Scene::destroy_node(SceneNode* node) {
    if (!node) return;
    
    EntityId entity = node->get_entity();
    
    // Remove from name map
    name_nodes_.erase(node->get_name());
    
    // Remove children first
    std::vector<SceneNode*>& children = const_cast<std::vector<SceneNode*>&>(
        node->get_children());
    children.clear();
    
    // Remove from parent's children
    if (node->get_parent()) {
        std::vector<SceneNode*>& parent_children = const_cast<std::vector<SceneNode*>&>(
            node->get_parent()->get_children());
        auto it = std::find(parent_children.begin(), parent_children.end(), node);
        if (it != parent_children.end()) {
            parent_children.erase(it);
        }
    }
    
    // Remove from root nodes
    auto root_it = std::find(root_nodes_.begin(), root_nodes_.end(), node);
    if (root_it != root_nodes_.end()) {
        root_nodes_.erase(root_it);
    }
    
    delete node;
    entity_nodes_.erase(entity);
}

void Scene::attach_to_scene(SceneNode* node, SceneNode* parent) {
    if (!node) return;
    
    // Remove from old parent if exists
    if (node->get_parent()) {
        std::vector<SceneNode*>& old_children = const_cast<std::vector<SceneNode*>&>(
            node->get_parent()->get_children());
        auto it = std::find(old_children.begin(), old_children.end(), node);
        if (it != old_children.end()) {
            old_children.erase(it);
        }
    }
    
    // Remove from root nodes if present
    auto root_it = std::find(root_nodes_.begin(), root_nodes_.end(), node);
    if (root_it != root_nodes_.end()) {
        root_nodes_.erase(root_it);
    }
    
    if (parent) {
        std::vector<SceneNode*>& children = parent->get_children();
        children.push_back(node);
        node->set_parent(parent);
    } else {
        root_nodes_.push_back(node);
        node->set_parent(nullptr);
    }
}

std::vector<SceneNode*> Scene::get_all_nodes() const {
    std::vector<SceneNode*> all_nodes;
    
    std::function<void(SceneNode*)> collect = [&](SceneNode* node) {
        if (!node) return;
        all_nodes.push_back(node);
        for (auto* child : node->get_children()) {
            collect(child);
        }
    };
    
    for (auto* root : root_nodes_) {
        collect(root);
    }
    
    return all_nodes;
}

SceneNode* Scene::find_by_name(const std::string& name) const {
    auto it = name_nodes_.find(name);
    return (it != name_nodes_.end()) ? it->second : nullptr;
}

void Scene::update_transforms() {
    for (auto* root : root_nodes_) {
        std::function<void(SceneNode*)> update = [&](SceneNode* node) {
            if (!node) return;
            node->update_matrix_hierarchy();
            for (auto* child : node->get_children()) {
                update(child);
            }
        };
        update(root);
    }
}

void Scene::update(float delta_time) {
    update_transforms();
}

void Scene::save(const std::string& filepath) const {
    // TODO: Implement scene serialization
    (void)filepath; // Unused
}

void Scene::load(const std::string& filepath) {
    // TODO: Implement scene deserialization
    (void)filepath; // Unused
}

} // namespace litt
