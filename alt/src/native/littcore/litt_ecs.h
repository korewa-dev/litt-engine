// LittECS - High-performance Entity Component System
// Archetype-based storage for cache-friendly iteration

#pragma once
#include <utility>
#include "litt_math.h"
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <typeindex>
#include <memory>
#include <functional>
#include <algorithm>
#include <stdexcept>
#include <limits>

namespace litt {

using EntityId = uint32_t;
constexpr EntityId INVALID = 0xFFFFFFFFu;

struct Entity {
    EntityId id;
    uint32_t gen;
    Entity() : id(INVALID), gen(0) {}
    Entity(EntityId i, uint32_t g) : id(i), gen(g) {}
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
        size_t last_pos = data.size() - 1;   // position of the swapped-in slot
        if ((size_t)idx != last_pos) {
            data[idx] = std::move(data[last_pos]);
            entities[idx] = entities[last_pos];
            index[entities[idx]] = idx;
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
        EntityId id;
        if (!free_ids_.empty()) {
            id = free_ids_.back();
            free_ids_.pop_back();
        } else {
            if (next_ == INVALID) {
                throw std::overflow_error("entity id space exhausted");
            }
            id = next_++;
            generations_.push_back(0);
        }
        alive_.insert(id);
        return Entity(id, generations_[id]);
    }

    void destroy(Entity e) {
        if (!is_alive(e)) return;

        // Remove components before making the ID reusable.
        for (auto& [type, storage] : storages_) {
            storage->remove(e);
        }
        alive_.erase(e.id);
        // A wrapped generation could make a very old stale handle valid
        // again. Retire the ID instead of recycling it at generation max.
        if (generations_[e.id] == std::numeric_limits<uint32_t>::max()) {
            generations_[e.id] = 0;
        } else {
            ++generations_[e.id];
            free_ids_.push_back(e.id);
        }
    }

    bool is_alive(Entity e) const {
        return e.id < generations_.size() &&
               alive_.count(e.id) > 0 &&
               generations_[e.id] == e.gen;
    }
    
    template<typename T>
    T& add(Entity e, T comp) {
        if (!is_alive(e)) throw std::invalid_argument("cannot add component to dead or stale entity");
        ensure<T>();
        auto* s = static_cast<Storage<T>*>(storages_[typeid(T)].get());
        // Reject duplicate components
        if (s->has(e)) {
            *s->get(e) = std::move(comp);
            return *s->get(e);
        }
        s->add(e, std::move(comp));
        return *s->get(e);
    }
    
    template<typename T>
    T* get(Entity e) {
        if (!is_alive(e)) return nullptr;
        auto it = storages_.find(typeid(T));
        if (it == storages_.end()) return nullptr;
        return static_cast<Storage<T>*>(it->second.get())->get(e);
    }
    
    template<typename T>
    bool has(Entity e) const {
        if (!is_alive(e)) return false;
        auto it = storages_.find(typeid(T));
        return it != storages_.end() && it->second->has(e);
    }
    
    template<typename T>
    void remove(Entity e) {
        if (!is_alive(e)) return;
        auto it = storages_.find(typeid(T));
        if (it != storages_.end()) it->second->remove(e);
    }
    
    void each(std::function<void(Entity)> fn) {
        std::vector<Entity> snapshot;
        snapshot.reserve(alive_.size());
        for (EntityId id : alive_) snapshot.emplace_back(id, generations_[id]);
        for (Entity e : snapshot) if (is_alive(e)) fn(e);
    }
    
    template<typename T>
    void query(std::function<void(Entity, T*)> fn) {
        auto it = storages_.find(typeid(T));
        if (it == storages_.end()) return;
        auto& s = *static_cast<Storage<T>*>(it->second.get());
        std::vector<Entity> snapshot;
        snapshot.reserve(s.entities.size());
        for (EntityId id : s.entities) {
            if (id < generations_.size()) snapshot.emplace_back(id, generations_[id]);
        }
        for (Entity entity : snapshot) {
            if (!is_alive(entity)) continue;
            T* component = s.get(entity);
            if (component) fn(entity, component);
        }
    }
    struct System {
        virtual ~System() = default;
        virtual void update(float dt) = 0;
    };
    void add_system(std::unique_ptr<System> sys) {
        if (sys) systems_.push_back(std::move(sys));
    }
    void update(float dt) { for (auto& s : systems_) s->update(dt); }
    
private:
    template<typename T>
    void ensure() {
        if (storages_.find(typeid(T)) == storages_.end()) {
            storages_[typeid(T)] = std::make_unique<Storage<T>>();
        }
    }
    
    EntityId next_;
    std::vector<uint32_t> generations_;
    std::vector<EntityId> free_ids_;
    std::unordered_set<EntityId> alive_;
    std::unordered_map<std::type_index, std::unique_ptr<StorageBase>> storages_;
    std::vector<std::unique_ptr<System>> systems_;
};

// Common components
struct Transform {
    Vec3 position = Vec3::zero();
    Quat rotation = Quat::identity();
    Vec3 scale = Vec3{1,1,1};
    Mat4 matrix = Mat4::identity();
    void update() {
        matrix = Mat4::translation(position) *
                 rotation.to_mat4() *
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

// Mesh, Material, Light, Camera are defined in litt_renderer.h
// to avoid type conflicts between ECS stubs and the full renderer implementations.

} // namespace litt
