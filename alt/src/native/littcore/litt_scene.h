// LittScene - Scene management for Litt Engine

#pragma once
#include "litt_math.h"
#include "litt_ecs.h"
#include "litt_renderer.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <algorithm>

namespace litt {

// Scene node with hierarchy
struct SceneNode {
    uint32_t id = 0;
    std::string name;
    Vec3 position = Vec3::zero();
    Vec3 rotation = Vec3::zero();
    Vec3 scale = Vec3{1, 1, 1};
    Mat4 transform = Mat4::identity();
    Mat4 inverseTransform = Mat4::identity();
    
    // Scene owns nodes. Hierarchy links are non-owning pointers so a node
    // cannot be owned both by Scene::nodes and by its parent.
    std::vector<SceneNode*> children;
    SceneNode* parent = nullptr;
    
    // Components (renderer types from litt_renderer.h)
    Transform* transformComponent = nullptr;
    Collider* colliderComponent = nullptr;
    RigidBody* rigidBodyComponent = nullptr;
    MeshData* meshComponent = nullptr;
    RenderMaterial* materialComponent = nullptr;
    Light* lightComponent = nullptr;
    RenderCamera* cameraComponent = nullptr;
    
    // Visibility
    bool visible = true;
    bool cullable = true;
    
    void updateTransform() {
        // Recalculate local transform matrix
        Mat4 local = Mat4::translation(position) *
                     Mat4::rot_y(rotation.y) *
                     Mat4::rot_x(rotation.x) *
                     Mat4::rot_z(rotation.z) *
                     Mat4::scale(scale);

        // Apply parent world transform
        if (parent) {
            transform = parent->transform * local;
        } else {
            transform = local;
        }

        // Inverse for lighting calculations
        inverseTransform = transform.affine_inverse();

        // Update children
        for (SceneNode* child : children) {
            if (child) child->updateTransform();
        }
    }
    
    // Hierarchy links are non-owning. Scene remains the sole node owner.
    bool addChild(SceneNode* child) {
        if (!child || child == this) return false;

        // Reject cycles: child may not be this node or any ancestor of it.
        for (SceneNode* p = this; p; p = p->parent) {
            if (p == child) return false;
        }

        if (child->parent == this) return true;
        if (child->parent) child->parent->removeChild(child);
        child->parent = this;
        children.push_back(child);
        return true;
    }
    
    bool removeChild(SceneNode* child) {
        if (!child) return false;
        const auto old_size = children.size();
        children.erase(std::remove(children.begin(), children.end(), child), children.end());
        if (children.size() == old_size) return false;
        if (child->parent == this) child->parent = nullptr;
        return true;
    }
    
    SceneNode* findChild(const std::string& name) {
        for (SceneNode* child : children) {
            if (!child) continue;
            if (child->name == name) return child;
            auto found = child->findChild(name);
            if (found) return found;
        }
        return nullptr;
    }
};

// Scene graph
class Scene {
public:
    SceneNode* root = nullptr;
    uint32_t nextId = 0;
    std::unordered_map<uint32_t, std::unique_ptr<SceneNode>> nodes;
    
    Scene() {
        root = &createNode("Root");
    }
    
    ~Scene() {
        clear();
    }
    
    SceneNode& createNode(const std::string& name) {
        auto node = std::make_unique<SceneNode>();
        node->id = nextId++;
        node->name = name;

        // Capture identity before moving ownership. Referencing node after
        // std::move can dereference a null unique_ptr depending on evaluation
        // order, which previously made Scene construction crash.
        const uint32_t id = node->id;
        SceneNode* created = node.get();
        nodes.emplace(id, std::move(node));
        return *created;
    }
    
    SceneNode* getNode(uint32_t id) {
        auto it = nodes.find(id);
        return it != nodes.end() ? it->second.get() : nullptr;
    }
    
    SceneNode* getNode(const std::string& name) {
        for (auto& [id, node] : nodes) {
            if (node->name == name) return node.get();
        }
        return nullptr;
    }
    
    bool setParent(uint32_t childId, uint32_t parentId) {
        SceneNode* child = getNode(childId);
        SceneNode* newParent = getNode(parentId);
        if (!child || !newParent || child == root) return false;
        return newParent->addChild(child);
    }

    bool clearParent(uint32_t childId) {
        SceneNode* child = getNode(childId);
        if (!child || !child->parent) return false;
        return child->parent->removeChild(child);
    }
    
    void removeNode(uint32_t id) {
        auto it = nodes.find(id);
        if (it == nodes.end()) return;

        SceneNode* target = it->second.get();
        // The root is a scene invariant. Removing it would leave the Scene in
        // a half-valid state, so callers must clear/destroy the Scene instead.
        if (target == root) return;

        if (target->parent) target->parent->removeChild(target);
        destroyNode(target);
    }
    
    void update() {
        // Nodes created through Scene::createNode live in the scene map.
        // Update every top-level node so independently-created nodes are not
        // silently skipped. Children are updated recursively by their parent.
        for (auto& [id, node] : nodes) {
            if (!node->parent) node->updateTransform();
        }
    }
    
    void clear() {
        // Break all non-owning hierarchy links before releasing node storage.
        for (auto& [id, node] : nodes) {
            node->parent = nullptr;
            node->children.clear();
        }
        nodes.clear();
        root = nullptr;
        nextId = 0;
    }
    
    // Serialization
    std::string serializeToJson() const {
        // Serialization is not implemented yet.  An empty result is an
        // explicit failure signal; do not return valid-looking placeholder
        // JSON that callers can mistake for persisted state.
        return {};
    }
    
    bool deserializeFromJson(const std::string&) {
        // Do not claim success until scene state is actually restored.
        return false;
    }
    
private:
    void destroyNode(SceneNode* node) {
        if (!node) return;

        // Copy because recursive removal destroys the referenced children.
        const std::vector<SceneNode*> descendants = node->children;
        node->children.clear();
        for (SceneNode* child : descendants) {
            if (!child) continue;
            child->parent = nullptr;
            auto it = nodes.find(child->id);
            if (it != nodes.end() && it->second.get() == child) {
                destroyNode(child);
            }
        }
        nodes.erase(node->id);
    }
};

// Scene manager
class SceneManager {
public:
    std::unordered_map<std::string, std::unique_ptr<Scene>> scenes;
    Scene* currentScene = nullptr;
    
    Scene& createScene(const std::string& name) {
        // Never replace an existing scene behind currentScene. Returning the
        // existing instance keeps the raw active pointer stable.
        auto existing = scenes.find(name);
        if (existing != scenes.end()) return *existing->second;

        auto scene = std::make_unique<Scene>();
        Scene* created = scene.get();
        scenes.emplace(name, std::move(scene));
        return *created;
    }
    
    Scene* getScene(const std::string& name) {
        auto it = scenes.find(name);
        return it != scenes.end() ? it->second.get() : nullptr;
    }
    
    void setActiveScene(const std::string& name) {
        auto it = scenes.find(name);
        if (it != scenes.end()) {
            currentScene = it->second.get();
        }
    }
    
    Scene* getActiveScene() const {
        return currentScene;
    }
    
    void update(float) {
        if (currentScene) {
            currentScene->update();
        }
    }
    
    void unloadScene(const std::string& name) {
        // Previous version did scenes[name] AFTER erase(), which
        // default-constructed a null unique_ptr and dereferenced it (crash),
        // and left currentScene dangling when the active scene was removed.
        auto it = scenes.find(name);
        if (it == scenes.end()) return;
        if (currentScene == it->second.get()) currentScene = nullptr;
        scenes.erase(it);
    }
    
    void clearAll() {
        scenes.clear();
        currentScene = nullptr;
    }
};

} // namespace litt
