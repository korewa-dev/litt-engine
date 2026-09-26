// LittScene - Scene management for Litt Engine

#pragma once
#include <cstdint>
#include "litt_math.h"
#include "litt_ecs.h"
#include "litt_renderer.h"
#include "litt_lighting.h"
#include "litt_json.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <algorithm>
#include <utility>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace litt {

// Scene node with hierarchy
struct SceneNode {
    uint32_t id = 0;
    std::string name;
    Vec3 position = Vec3::zero();
    Quat rotation = Quat::identity();
    Vec3 scale = Vec3{1, 1, 1};
    Mat4 transform = Mat4::identity();
    Mat4 inverseTransform = Mat4::identity();
    bool inverseTransformValid = true;
    
    // Scene owns nodes. Hierarchy links are non-owning pointers so a node
    // cannot be owned both by Scene::nodes and by its parent.
    std::vector<SceneNode*> children;
    SceneNode* parent = nullptr;
    
    // SceneNode is the sole transform authority for scene-graph nodes.
    // Do not cache a raw Transform* from ECS storage here: packed component
    // storage can move elements, and a second mutable transform would let the
    // scene graph and ECS disagree about world position. Systems that bridge
    // Scene and ECS must copy/synchronize through an Entity handle instead.
    //
    // Scene-owned optional attachments. These values move with the node and
    // cannot dangle when caller-local component objects go out of scope.
    // ECS simulation components remain in World and should be synchronized
    // explicitly rather than cached as raw pointers here.
    std::unique_ptr<Collider> colliderComponent;
    std::unique_ptr<RigidBody> rigidBodyComponent;
    std::unique_ptr<MeshData> meshComponent;
    std::unique_ptr<RenderMaterial> materialComponent;
    std::unique_ptr<Light> lightComponent;
    std::unique_ptr<RenderCamera> cameraComponent;

    template<typename T, typename... Args>
    static T& emplaceAttachment(std::unique_ptr<T>& slot, Args&&... args) {
        slot = std::make_unique<T>(std::forward<Args>(args)...);
        return *slot;
    }

    Collider& setCollider(const Collider& value) {
        return emplaceAttachment(colliderComponent, value);
    }
    RigidBody& setRigidBody(const RigidBody& value) {
        return emplaceAttachment(rigidBodyComponent, value);
    }
    MeshData& setMesh(const MeshData& value) {
        return emplaceAttachment(meshComponent, value);
    }
    RenderMaterial& setMaterial(const RenderMaterial& value) {
        return emplaceAttachment(materialComponent, value);
    }
    Light& setLight(const Light& value) {
        return emplaceAttachment(lightComponent, value);
    }
    RenderCamera& setCamera(const RenderCamera& value) {
        return emplaceAttachment(cameraComponent, value);
    }
    
    // Visibility
    bool visible = true;
    bool cullable = true;
    
    void updateTransform() {
        // Recalculate local transform matrix
        Mat4 local = Mat4::translation(position) *
                     rotation.to_mat4() *
                     Mat4::scale(scale);

        // Apply parent world transform
        if (parent) {
            transform = parent->transform * local;
        } else {
            transform = local;
        }

        // Singular transforms (for example a zero scale axis) have no
        // inverse. Keep that state explicit instead of asserting/crashing.
        const float det =
            transform.m[0] * (transform.m[5] * transform.m[10] - transform.m[9] * transform.m[6]) -
            transform.m[4] * (transform.m[1] * transform.m[10] - transform.m[9] * transform.m[2]) +
            transform.m[8] * (transform.m[1] * transform.m[6] - transform.m[5] * transform.m[2]);
        inverseTransformValid = std::isfinite(det) && std::abs(det) > MATH_EPS;
        inverseTransform = inverseTransformValid ? transform.affine_inverse() : Mat4::identity();

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
    static constexpr size_t MAX_SERIALIZED_BYTES = 8u * 1024u * 1024u;
    static constexpr size_t MAX_NODES = 65536u;
    static constexpr size_t MAX_NODE_NAME_BYTES = 4096u;

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
        if (name.size() > MAX_NODE_NAME_BYTES) {
            throw std::length_error("scene node name exceeds supported limit");
        }
        if (nodes.size() >= MAX_NODES) {
            throw std::length_error("scene node count exceeds supported limit");
        }
        if (nextId == UINT32_MAX || nodes.count(nextId)) {
            throw std::overflow_error("scene node id space exhausted");
        }
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
    
    // Serialization contract v1 persists scene-graph identity, hierarchy,
    // transforms and visibility flags. Runtime/render attachments are
    // intentionally outside v1 until their individual persistence contracts
    // are defined.
    std::string serializeToJson() const {
        auto finite3 = [](const Vec3& v) {
            return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
        };
        auto finite_quat = [](const Quat& q) {
            return std::isfinite(q.x) && std::isfinite(q.y) &&
                   std::isfinite(q.z) && std::isfinite(q.w);
        };

        if (!root || nodes.empty() || nodes.size() > MAX_NODES) return {};
        const auto root_it = std::find_if(
            nodes.begin(), nodes.end(),
            [this](const auto& entry) { return entry.second.get() == root; });
        if (root_it == nodes.end() || !root_it->second ||
            root_it->first != root_it->second->id) {
            return {};
        }

        for (const auto& [id, node] : nodes) {
            if (!node || node->id != id || node->name.size() > MAX_NODE_NAME_BYTES ||
                !finite3(node->position) || !finite3(node->scale) ||
                !finite_quat(node->rotation)) {
                return {};
            }
        }

        auto escape = [](const std::string& s) {
            std::string out;
            out.reserve(s.size() + 8);
            for (unsigned char ch : s) {
                switch (ch) {
                    case '"': out += "\\\""; break;
                    case '\\': out += "\\\\"; break;
                    case '\b': out += "\\b"; break;
                    case '\f': out += "\\f"; break;
                    case '\n': out += "\\n"; break;
                    case '\r': out += "\\r"; break;
                    case '\t': out += "\\t"; break;
                    default:
                        if (ch < 0x20) {
                            static const char hex[] = "0123456789abcdef";
                            out += "\\u00";
                            out += hex[(ch >> 4) & 0xf];
                            out += hex[ch & 0xf];
                        } else out += static_cast<char>(ch);
                }
            }
            return out;
        };

        std::vector<uint32_t> ids;
        ids.reserve(nodes.size());
        for (const auto& entry : nodes) ids.push_back(entry.first);
        std::sort(ids.begin(), ids.end());

        std::ostringstream out;
        out << std::setprecision(9);
        out << "{\"version\":1,\"root\":" << (root ? root->id : 0) << ",\"nodes\":[";
        bool first = true;
        for (uint32_t id : ids) {
            const SceneNode& n = *nodes.at(id);
            if (!first) out << ',';
            first = false;
            out << "{\"id\":" << n.id
                << ",\"name\":\"" << escape(n.name) << "\""
                << ",\"parent\":" << (n.parent ? std::to_string(n.parent->id) : "null")
                << ",\"position\":[" << n.position.x << ',' << n.position.y << ',' << n.position.z << ']'
                << ",\"rotation\":[" << n.rotation.x << ',' << n.rotation.y << ',' << n.rotation.z << ',' << n.rotation.w << ']'
                << ",\"scale\":[" << n.scale.x << ',' << n.scale.y << ',' << n.scale.z << ']'
                << ",\"visible\":" << (n.visible ? "true" : "false")
                << ",\"cullable\":" << (n.cullable ? "true" : "false") << '}';
        }
        out << "]}";
        std::string serialized = out.str();
        return serialized.size() <= MAX_SERIALIZED_BYTES ? serialized : std::string();
    }
    
    bool deserializeFromJson(const std::string& json) {
        if (json.empty() || json.size() > MAX_SERIALIZED_BYTES) return false;
        LvJson* doc = lvj_parse_strict(json.c_str());
        if (!doc || doc->kind != LJ_OBJ) { lvj_free(doc); return false; }
        const LvJson* version = lvj_get(doc, "version");
        const LvJson* rootValue = lvj_get(doc, "root");
        const LvJson* array = lvj_get(doc, "nodes");
        if (!version || version->kind != LJ_NUM || version->num != 1 ||
            !rootValue || rootValue->kind != LJ_NUM ||
            !std::isfinite(rootValue->num) || rootValue->num < 0 ||
            rootValue->num > UINT32_MAX || std::floor(rootValue->num) != rootValue->num ||
            !array || array->kind != LJ_ARR || array->count <= 0 ||
            static_cast<size_t>(array->count) > MAX_NODES) {
            lvj_free(doc); return false;
        }

        Scene candidate;
        candidate.clear();
        struct PendingParent { uint32_t child; bool hasParent; uint32_t parent; };
        std::vector<PendingParent> pending;
        uint32_t maxId = 0;
        bool ok = true;
        auto finite3 = [](const LvJson* v, Vec3& dst) {
            float a[3];
            if (!lvj_arr_f3(v, a) || !std::isfinite(a[0]) || !std::isfinite(a[1]) || !std::isfinite(a[2])) return false;
            dst = Vec3(a[0], a[1], a[2]); return true;
        };
        for (int i = 0; ok && i < array->count; ++i) {
            const LvJson* item = lvj_at(array, i);
            if (!item || item->kind != LJ_OBJ) { ok = false; break; }
            const LvJson* idv = lvj_get(item, "id");
            const LvJson* namev = lvj_get(item, "name");
            const LvJson* parentv = lvj_get(item, "parent");
            const LvJson* posv = lvj_get(item, "position");
            const LvJson* rotv = lvj_get(item, "rotation");
            const LvJson* scalev = lvj_get(item, "scale");
            const LvJson* visv = lvj_get(item, "visible");
            const LvJson* cullv = lvj_get(item, "cullable");
            if (!idv || idv->kind != LJ_NUM || idv->num < 0 || idv->num > UINT32_MAX ||
                std::floor(idv->num) != idv->num || !namev || namev->kind != LJ_STR ||
                !namev->str || std::strlen(namev->str) > MAX_NODE_NAME_BYTES ||
                !rotv || rotv->kind != LJ_ARR || rotv->count != 4 ||
                !visv || visv->kind != LJ_BOOL || !cullv || cullv->kind != LJ_BOOL) { ok = false; break; }
            const uint32_t id = static_cast<uint32_t>(idv->num);
            if (candidate.nodes.count(id)) { ok = false; break; }
            auto node = std::make_unique<SceneNode>();
            node->id = id; node->name = namev->str ? namev->str : "";
            if (!finite3(posv, node->position) || !finite3(scalev, node->scale)) { ok = false; break; }
            float q[4];
            for (int qn=0; qn<4; ++qn) {
                const LvJson* qv = lvj_at(rotv, qn);
                if (!qv || qv->kind != LJ_NUM || !std::isfinite(qv->num)) { ok=false; break; }
                q[qn]=static_cast<float>(qv->num);
            }
            if (!ok) break;
            node->rotation = Quat(q[0],q[1],q[2],q[3]).normalized();
            node->visible = visv->boolean != 0; node->cullable = cullv->boolean != 0;
            bool hasParent = false; uint32_t parentId = 0;
            if (parentv && parentv->kind != LJ_NULL) {
                if (parentv->kind != LJ_NUM || parentv->num < 0 || parentv->num > UINT32_MAX ||
                    std::floor(parentv->num) != parentv->num) { ok=false; break; }
                hasParent=true; parentId=static_cast<uint32_t>(parentv->num);
            }
            candidate.nodes.emplace(id, std::move(node));
            pending.push_back({id,hasParent,parentId});
            maxId = std::max(maxId,id);
        }
        const uint32_t rootId = static_cast<uint32_t>(rootValue->num);
        if (ok) {
            candidate.root = candidate.getNode(rootId);
            if (!candidate.root) ok=false;
        }
        if (ok) {
            for (const auto& p : pending) {
                if (!p.hasParent) continue;
                if (p.child == rootId || !candidate.setParent(p.child,p.parent)) { ok=false; break; }
            }
        }
        if (ok && candidate.root->parent) ok=false;
        if (ok) {
            size_t topLevel=0;
            for (const auto& e : candidate.nodes) if (!e.second->parent) ++topLevel;
            if (topLevel == 0) ok=false;
        }
        if (ok) {
            if (maxId == UINT32_MAX) { ok=false; }
            else candidate.nextId = maxId + 1;
            if (!ok) { lvj_free(doc); return false; }
            candidate.update();
            clear();
            nodes = std::move(candidate.nodes);
            root = getNode(rootId);
            nextId = candidate.nextId;
        }
        lvj_free(doc);
        return ok;
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
