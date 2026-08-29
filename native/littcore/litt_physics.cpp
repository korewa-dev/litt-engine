// LittPhysics - Working Physics Implementation
// Rigid body physics with broadphase, narrowphase, and solver

#include "litt_physics.h"
#include <algorithm>
#include <cmath>
#include <queue>

namespace litt {

// =============================================================================
// PhysicsWorld Implementation
// =============================================================================

PhysicsWorld::PhysicsWorld() : gravity_(Vec3f(0, -9.81f, 0)), 
                               max_iterations_(10),
                               time_step_(1.0f / 60.0f) {}

PhysicsWorld::~PhysicsWorld() {
    clear();
}

void PhysicsWorld::set_gravity(const Vec3f& gravity) {
    gravity_ = gravity;
}

const Vec3f& PhysicsWorld::get_gravity() const {
    return gravity_;
}

void PhysicsWorld::add_body(PhysicsBody* body) {
    if (!body) return;
    
    bodies_.push_back(body);
    broadphase_.addBody(body);
}

void PhysicsWorld::remove_body(PhysicsBody* body) {
    if (!body) return;
    
    auto it = std::find(bodies_.begin(), bodies_.end(), body);
    if (it != bodies_.end()) {
        bodies_.erase(it);
    }
    broadphase_.removeBody(body);
}

void PhysicsWorld::clear() {
    bodies_.clear();
    broadphase_ = BroadPhase();
}

void PhysicsWorld::step(float dt) {
    if (bodies_.empty()) return;
    
    // 1. Apply forces
    for (auto* body : bodies_) {
        if (body->is_static) continue;
        
        // Apply gravity
        body->force += gravity_ * (1.0f / body->inverseMass);
        
        // Apply force to velocity
        body->velocity += body->force * body->inverseMass * dt;
        
        // Apply damping
        body->velocity *= 0.999f;
        
        // Clear forces
        body->force = Vec3f::zero();
    }
    
    // 2. Update positions
    for (auto* body : bodies_) {
        if (body->is_static) continue;
        
        // Store previous position for CCD
        body->previousPosition = body->centerOfMass;
        
        // Update position
        body->centerOfMass += body->velocity * dt;
        
        // Update AABB
        body->updateAabb();
    }
    
    // 3. Broad phase - find potential collisions
    std::vector<std::pair<PhysicsBody*, PhysicsBody*)) collision_pairs;
    broadphase_.findCollisions(collision_pairs);
    
    // 4. Narrow phase - precise collision detection
    std::vector<Collision> collisions;
    for (auto& [body1, body2] : collision_pairs) {
        Collision collision;
        if (castAABB(body1->aabb, body2->aabb, &collision)) {
            collisions.push_back(collision);
        }
    }
    
    // 5. Resolve collisions
    for (auto& collision : collisions) {
        resolve_collision(collision.body1, collision.body2, 
                         collision.normal, collision.depth);
    }
}

void PhysicsWorld::resolve_collision(PhysicsBody* body1, PhysicsBody* body2,
                                    const Vec3f& normal, float depth) {
    if (body1->is_static && body2->is_static) return;
    
    // Separate bodies
    Vec3f correction = normal * (depth / (body1->inverseMass + body2->inverseMass));
    
    if (!body1->is_static) {
        body1->centerOfMass += correction * body1->inverseMass;
    }
    if (!body2->is_static) {
        body2->centerOfMass -= correction * body2->inverseMass;
    }
    
    // Calculate relative velocity
    Vec3f rel_vel = body1->velocity - body2->velocity;
    float vel_along_normal = rel_vel.dot(normal);
    
    // Don't resolve if velocities are separating
    if (vel_along_normal > 0) return;
    
    // Calculate restitution
    float e = std::min(body1->restitution, body2->restitution);
    
    // Calculate impulse scalar
    float j = -(1.0f + e) * vel_along_normal;
    j /= (body1->inverseMass + body2->inverseMass);
    
    // Apply impulse
    Vec3f impulse = normal * j;
    
    if (!body1->is_static) {
        body1->velocity += impulse * body1->inverseMass;
    }
    if (!body2->is_static) {
        body2->velocity -= impulse * body2->inverseMass;
    }
}

bool PhysicsWorld::castAABB(const Aabb& a, const Aabb& b, Collision* out) {
    // AABB overlap test
    if (a.max.x < b.min.x || a.min.x > b.max.x) return false;
    if (a.max.y < b.min.y || a.min.y > b.max.y) return false;
    if (a.max.z < b.min.z || a.min.z > b.max.z) return false;
    
    if (out) {
        // Calculate collision normal and depth
        Vec3f center_diff = a.center() - b.center();
        Vec3f half_sizes = (a.size() + b.size()) * 0.5f;
        
        float overlap_x = half_sizes.x - std::abs(center_diff.x);
        float overlap_y = half_sizes.y - std::abs(center_diff.y);
        float overlap_z = half_sizes.z - std::abs(center_diff.z);
        
        // Find minimum overlap axis
        if (overlap_x <= overlap_y && overlap_x <= overlap_z) {
            out->normal = Vec3f(center_diff.x > 0 ? 1 : -1, 0, 0);
            out->depth = overlap_x;
        } else if (overlap_y <= overlap_x && overlap_y <= overlap_z) {
            out->normal = Vec3f(0, center_diff.y > 0 ? 1 : -1, 0);
            out->depth = overlap_y;
        } else {
            out->normal = Vec3f(0, 0, center_diff.z > 0 ? 1 : -1);
            out->depth = overlap_z;
        }
        
        out->body1 = nullptr;
        out->body2 = nullptr;
    }
    
    return true;
}

// =============================================================================
// BroadPhase Implementation
// =============================================================================

void BroadPhase::addBody(PhysicsBody* body) {
    // Sweep and prune - sort by AABB min.x
    bodies.push_back(body);
    sortBodies();
}

void BroadPhase::removeBody(PhysicsBody* body) {
    bodies.erase(
        std::remove(bodies.begin(), bodies.end(), body),
        bodies.end()
    );
}

void BroadPhase::findCollisions(std::vector<std::pair<PhysicsBody*, PhysicsBody*>>& pairs) {
    pairs.clear();
    
    // Sweep and prune
    for (size_t i = 0; i < bodies.size(); ++i) {
        for (size_t j = i + 1; j < bodies.size(); ++j) {
            if (AABB::intersects(bodies[i]->aabb, bodies[j]->aabb)) {
                pairs.push_back({bodies[i], bodies[j]});
            }
        }
    }
}

void BroadPhase::sortBodies() {
    std::sort(bodies.begin(), bodies.end(),
        [](const PhysicsBody* a, const PhysicsBody* b) {
            return a->aabb.min.x < b->aabb.min.x;
        });
}

} // namespace litt
