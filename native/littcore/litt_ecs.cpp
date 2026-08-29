// LittECS - Entity Component System Implementation
// Production ECS with archetype-based storage and system execution

#include "litt_ecs.h"
#include <algorithm>
#include <cstring>

namespace litt {

// =============================================================================
// World Implementation
// =============================================================================

World::World() : next_entity_id_(0), next_gen_(1) {}

World::~World() {
    // Cleanup all component storages
    for (auto& [type_idx, storage] : component_storages_) {
        // Storage is managed via unique_ptr, auto-cleanup
    }
}

Entity World::create_entity() {
    if (next_entity_id_ >= MAX_ENTITIES) {
        // Entity pool exhaustion - in production, implement recycling
        throw std::runtime_error("Entity pool exhausted");
    }
    
    EntityId id = next_entity_id_++;
    uint16_t gen = next_gen_++;
    
    // Track entity
    entities_.push_back(Entity{id, gen});
    
    // Initialize component masks
    entity_masks_[id] = 0;
    
    // Reset archetype data for this entity
    archetype_data_[id].resize(MAX_COMPONENTS, nullptr);
    
    return Entity{id, gen};
}

void World::destroy_entity(Entity entity) {
    if (!entity.valid()) return;
    
    EntityId id = entity.id;
    
    // Remove all components
    for (auto& [type_idx, storage] : component_storages_) {
        if (storage->has(entity)) {
            storage->remove(entity);
            entity_masks_[id] &= ~(1ULL << type_idx);
        }
    }
    
    // Clear archetype data
    archetype_data_[id].clear();
    
    // Mark entity as invalid
    entity.gen = INVALID_GEN;
}

bool World::has_entity(Entity entity) const {
    if (!entity.valid()) return false;
    
    auto it = entities_.begin();
    return std::find_if(it, entities_.end(), 
        [entity](const Entity& e) { 
            return e.id == entity.id && e.gen == entity.gen; 
        }) != entities_.end();
}

// =============================================================================
// Component Management
// =============================================================================

void World::add_component(Entity entity, std::unique_ptr<ComponentBase> component) {
    if (!entity.valid()) return;
    
    // Get component type index
    auto type_idx = get_component_type_index(*component);
    
    // Get or create storage
    auto& storage = component_storages_[type_idx];
    if (!storage) {
        storage = create_storage(type_idx, component->size());
    }
    
    // Add to storage
    storage->add(entity, std::move(component));
    
    // Update archetype mask
    entity_masks_[entity.id] |= (1ULL << type_idx);
}

void World::remove_component(Entity entity, const std::type_info& type) {
    if (!entity.valid()) return;
    
    auto it = component_storages_.find(type.index());
    if (it == component_storages_.end()) return;
    
    it->second->remove(entity);
    entity_masks_[entity.id] &= ~(1ULL << type.index());
}

ComponentBase* World::get_component(Entity entity, const std::type_info& type) {
    auto it = component_storages_.find(type.index());
    if (it == component_storages_.end()) return nullptr;
    
    return it->second->get(entity);
}

const ComponentBase* World::get_component(const Entity& entity, const std::type_info& type) const {
    auto it = component_storages_.find(type.index());
    if (it == component_storages_.end()) return nullptr;
    
    return it->second->get(entity);
}

// =============================================================================
// System Management
// =============================================================================

void World::add_system(std::unique_ptr<System> system) {
    systems_.push_back(std::move(system));
}

void World::remove_system(const std::string& name) {
    systems_.erase(
        std::remove_if(systems_.begin(), systems_.end(),
            [&name](const std::unique_ptr<System>& sys) {
                return sys->name() == name;
            }),
        systems_.end()
    );
}

void World::update(float dt) {
    // Update all systems
    for (auto& system : systems_) {
        system->on_update(dt);
    }
}

void World::render() {
    // Render all render systems
    for (auto& system : systems_) {
        if (auto* render_sys = dynamic_cast<RenderSystem*>(system.get())) {
            render_sys->on_render();
        }
    }
}

// =============================================================================
// Query System
// =============================================================================

Query World::query() {
    return Query(*this);
}

Query::Query(World& world) : world_(world) {}

Query& Query::with_component(const std::type_info& type) {
    required_types_.insert(type.index());
    return *this;
}

Query& Query::without_component(const std::type_info& type) {
    excluded_types_.insert(type.index());
    return *this;
}

std::vector<Entity> Query::execute() const {
    std::vector<Entity> results;
    
    for (const auto& entity : world_.entities_) {
        if (!entity.valid()) continue;
        
        uint64_t mask = world_.entity_masks_[entity.id];
        
        // Check required components
        bool has_all = true;
        for (auto type_idx : required_types_) {
            if (!(mask & (1ULL << type_idx))) {
                has_all = false;
                break;
            }
        }
        
        if (!has_all) continue;
        
        // Check excluded components
        bool has_none = true;
        for (auto type_idx : excluded_types_) {
            if (mask & (1ULL << type_idx)) {
                has_none = false;
                break;
            }
        }
        
        if (has_none) {
            results.push_back(entity);
        }
    }
    
    return results;
}

// =============================================================================
// System Base Implementations
// =============================================================================

void System::on_update(float dt) {}
void System::on_render() {}
std::string System::name() const { return "System"; }

// TransformSystem
TransformSystem::TransformSystem() : System() {}

void TransformSystem::on_update(float dt) {
    // Update all entities with Transform and Velocity
    auto query = world_->query()
        .with_component(typeid(Transform))
        .with_component(typeid(Velocity));
    
    for (auto entity : query.execute()) {
        auto* transform = world_->get_component<Transform>(entity);
        auto* velocity = world_->get_component<Velocity>(entity);
        
        if (transform && velocity) {
            transform->position += velocity->velocity * dt;
        }
    }
}

// GravitySystem
GravitySystem::GravitySystem(float gravity) : System(), gravity_(gravity) {}

void GravitySystem::on_update(float dt) {
    auto query = world_->query()
        .with_component(typeid(Transform))
        .with_component(typeid(Velocity))
        .with_component(typeid(PhysicsBody));
    
    for (auto entity : query.execute()) {
        auto* velocity = world_->get_component<Velocity>(entity);
        
        if (velocity) {
            velocity->velocity.y -= gravity_ * dt;
        }
    }
}

// PhysicsSystem
PhysicsSystem::PhysicsSystem() : System() {}

void PhysicsSystem::on_update(float dt) {
    // Simple AABB collision detection
    auto bodies = world_->query()
        .with_component(typeid(Transform))
        .with_component(typeid(PhysicsBody));
    
    std::vector<std::pair<Entity, Entity>> pairs;
    
    // Find potential collisions
    for (size_t i = 0; i < bodies.size(); ++i) {
        for (size_t j = i + 1; j < bodies.size(); ++j) {
            auto* t1 = world_->get_component<Transform>(bodies[i]);
            auto* t2 = world_->get_component<Transform>(bodies[j]);
            auto* p1 = world_->get_component<PhysicsBody>(bodies[i]);
            auto* p2 = world_->get_component<PhysicsBody>(bodies[j]);
            
            if (t1 && t2 && p1 && p2) {
                if (AABB::intersects(t1->position, p1->halfExtents,
                                   t2->position, p2->halfExtents)) {
                    pairs.push_back({bodies[i], bodies[j]});
                }
            }
        }
    }
    
    // Resolve collisions
    for (auto& [e1, e2] : pairs) {
        resolve_collision(e1, e2);
    }
}

void PhysicsSystem::resolve_collision(Entity a, Entity b) {
    auto* t1 = world_->get_component<Transform>(a);
    auto* t2 = world_->get_component<Transform>(b);
    auto* v1 = world_->get_component<Velocity>(a);
    auto* v2 = world_->get_component<Velocity>(b);
    
    if (!t1 || !t2 || !v1 || !v2) return;
    
    // Simple separation
    Vec3f diff = t1->position - t2->position;
    float dist = diff.length();
    
    if (dist > 0.0f) {
        Vec3f normal = diff.normalized();
        float overlap = 0.1f; // Simple overlap
        
        t1->position += normal * overlap;
        t2->position -= normal * overlap;
        
        // Simple bounce
        float restitution = 0.5f;
        Vec3f rel_vel = v1->velocity - v2->velocity;
        float vel_along_normal = rel_vel.dot(normal);
        
        if (vel_along_normal < 0) {
            Vec3f impulse = normal * vel_along_normal * restitution;
            v1->velocity -= impulse;
            v2->velocity += impulse;
        }
    }
}

} // namespace litt
