// LittECS - High-performance Entity Component System
// Archetype-based storage for cache-friendly iteration

#pragma once
#include "litt_math.h"
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <typeindex>
#include <memory>
#include <functional>
#include <algorithm>

namespace litt {

using EntityId = uint32_t;
constexpr EntityId INVALID = 0xFFFFFFFFu;

struct Entity {
    EntityId id;
    uint16_t gen;
    Entity() : id(INVALID), gen(0) {}
    Entity(EntityId i, uint16_t g) : id(i), gen(g) {}
    bool valid() const { return id != INVALID; }
    bool operator==(const Entity& o) const { return id == o.id && gen == o.gen; }
};

// Type-erased storage interface
struct StorageBase {
    virtual ~StorageBase() = default;
    virtual void remove(Entity e) = 0;
    virtual bool has(Entity e) const = 0;
    virtual size_t size() const = 0;
};

// Typed storage with O(1) add/remove
template<typename T>
struct Storage : StorageBase {
    std::vector<T> data;
    std::vector<EntityId> entities;
    std::vector<EntityId> index;
    
    void add(Entity e, T comp) {
        EntityId idx = (EntityId)data.size();
        data.push_back(std::move(comp));
        entities.push_back(e.id);
        if (index.size() <= e.id) index.resize(e.id + 1, INVALID);
        index[e.id] = idx;
    }
    
    virtual void remove(Entity e) override {
        if (e.id >= index.size()) return;
        EntityId idx = index[e.id];
        if (idx >= data.size()) return;
        EntityId last = entities.back();
        if (idx != last) {
            data[idx] = std::move(data[last]);
            entities[idx] = last;
            index[last] = idx;
        }
        data.pop_back();
        entities.pop_back();
        index[e.id] = INVALID;
    }
    
    T* get(Entity e) {
        if (e.id >= index.size()) return nullptr;
        EntityId idx = index[e.id];
        return idx < data.size() ? &data[idx] : nullptr;
    }
    
    virtual bool has(Entity e) const override {
        return e.id < index.size() && index[e.id] != INVALID;
    }
    
    virtual size_t size() const override { return data.size(); }
};

// Entity Component System container
class World {
public:
    World() : next_(0) {}
    
    Entity create() {
        Entity e(next_, 0);
        alive_.insert(next_);
        next_++;
        return e;
    }
    void destroy(Entity e) { alive_.erase(e.id); }
    bool is_alive(Entity e) const { return alive_.count(e.id) > 0; }
    
    template<typename T>
    T& add(Entity e, T comp) {
        ensure<T>();
        auto* s = static_cast<Storage<T>*>(storages_[typeid(T)].get());
        s->add(e, std::move(comp));
        return *s->get(e);
    }
    
    template<typename T>
    T* get(Entity e) {
        auto it = storages_.find(typeid(T));
        if (it == storages_.end()) return nullptr;
        return static_cast<Storage<T>*>(it->second.get())->get(e);
    }
    
    template<typename T>
    bool has(Entity e) const {
        auto it = storages_.find(typeid(T));
        return it != storages_.end() && it->second->has(e);
    }
    
    template<typename T>
    void remove(Entity e) {
        auto it = storages_.find(typeid(T));
        if (it != storages_.end()) it->second->remove(e);
    }
    
    void each(std::function<void(Entity)> fn) {
        for (EntityId id : alive_) {
            fn(Entity(id, 0));
        }
    }
    
    template<typename T>
    void query(std::function<void(Entity, T*)> fn) {
        auto it = storages_.find(typeid(T));
        if (it == storages_.end()) return;
        auto& s = *static_cast<Storage<T>*>(it->second.get());
        for (size_t i = 0; i < s.entities.size(); i++) {
            fn(Entity(s.entities[i], 0), &s.data[i]);
        }
    }
    struct System {
        virtual ~System() = default;
        virtual void update(float dt) = 0;
    };
    void add_system(std::unique_ptr<System> sys) { systems_.push_back(std::move(sys)); }
    void update(float dt) { for (auto& s : systems_) s->update(dt); }
    
private:
    template<typename T>
    void ensure() {
        if (storages_.find(typeid(T)) == storages_.end()) {
            storages_[typeid(T)] = std::make_unique<Storage<T>>();
        }
    }
    
    EntityId next_;
    std::unordered_set<EntityId> alive_;
    std::unordered_map<std::type_index, std::unique_ptr<StorageBase>> storages_;
    std::vector<std::unique_ptr<System>> systems_;
};

// Common components
struct Transform {
    Vec3 position = Vec3::zero();
    Vec3 rotation = Vec3::zero();
    Vec3 scale = Vec3{1,1,1};
    Mat4 matrix = Mat4::identity();
    void update() {
        matrix = Mat4::translation(position) * 
                 Mat4::rot_y(rotation.y) * Mat4::rot_x(rotation.x) * Mat4::rot_z(rotation.z) *
                 Mat4::scale(scale);
    }
};

struct Collider {
    Aabb bounds;
    bool trigger = false;
};

struct RigidBody {
    Vec3 velocity = Vec3::zero();
    float mass = 1.0f;
    bool is_static = false;
};

struct Mesh {
    int id = 0;
};

struct Material {
    Vec3 color = Vec3::one();
    float roughness = 0.5f;
    float metalness = 0.0f;
};

struct Light {
    Vec3 color = Vec3::one();
    float intensity = 1.0f;
    float range = 10.0f;
};

struct Camera {
    float fov = 60.0f;
    float aspect = 16.0f/9.0f;
    float near_ = 0.1f;
    float far_ = 1000.0f;
    Mat4 view = Mat4::identity();
    Mat4 proj = Mat4::identity();
};

} // namespace litt
