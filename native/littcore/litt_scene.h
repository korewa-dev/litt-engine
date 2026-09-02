// LittScene - Scene management for Litt Engine

#pragma once
#include "litt_math.h"
#include "litt_ecs.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

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
    
    std::vector<std::unique_ptr<SceneNode>> children;
    SceneNode* parent = nullptr;
    
    // Components
    Transform* transformComponent = nullptr;
    Collider* colliderComponent = nullptr;
    RigidBody* rigidBodyComponent = nullptr;
    Mesh* meshComponent = nullptr;
    Material* materialComponent = nullptr;
    Light* lightComponent = nullptr;
    Camera* cameraComponent = nullptr;
    
    // Visibility
    bool visible = true;
    bool cullable = true;
    
    void updateTransform() {
        // Recalculate transform matrix
        transform = Mat4::translation(position) * 
                    Mat4::rot_y(rotation.y) *
                    Mat4::rot_x(rotation.x) *
                    Mat4::rot_z(rotation.z) *
                    Mat4::scale(scale);
        
        // Inverse for lighting calculations
        inverseTransform = transform.affine_inverse();
        
        // Update children
        for (auto& child : children) {
            child->updateTransform();
        }
    }
    
    void addChild(std::unique_ptr<SceneNode> child) {
        child->parent = this;
        children.push_back(std::move(child));
    }
    
    void removeChild(SceneNode* child) {
        children.erase(std::remove_if(children.begin(), children.end(),
            [child](const std::unique_ptr<SceneNode>& c) { return c.get() == child; }),
            children.end());
    }
    
    SceneNode* findChild(const std::string& name) {
        for (auto& child : children) {
            if (child->name == name) return child.get();
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
        nodes[node->id] = std::move(node);
        return *nodes[node->id];
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
    
    void removeNode(uint32_t id) {
        nodes.erase(id);
    }
    
    void update() {
        if (root) root->updateTransform();
    }
    
    void clear() {
        nodes.clear();
        root = nullptr;
        nextId = 0;
    }
    
    // Serialization
    std::string serializeToJson() const {
        // Implementation would use JSON library
        return "{}";
    }
    
    bool deserializeFromJson(const std::string&) {
        // Implementation would parse JSON and create nodes
        return true;
    }
    
private:
    void destroyNode(SceneNode* node) {
        if (!node) return;
        for (auto& child : node->children) {
            destroyNode(child.get());
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
        auto scene = std::make_unique<Scene>();
        scenes[name] = std::move(scene);
        return *scenes[name];
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
