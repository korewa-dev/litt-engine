// LittPhysics - GPU-accelerated rigid body physics for Litt Engine

#pragma once
#include "litt_math.h"
#include "litt_ecs.h"
#include <vector>
#include <queue>
#include <algorithm>

namespace litt {

// =============================================================================
// Physics Body
// =============================================================================
struct PhysicsBody {
    EntityId entity{};
    Aabb aabb;
    Vec3 centerOfMass;
    float inverseMass = 1.0f;
    bool isStatic = false;
    bool isTrigger = false;
    Vec3 velocity;
    Vec3 force;
    Vec3 previousPosition;
    
    void updateAabb() {
        Vec3 halfSize = aabb.size() * 0.5f;
        aabb.min = centerOfMass - halfSize;
        aabb.max = centerOfMass + halfSize;
    }
};

// =============================================================================
// Broad Phase - Broadphase collision detection
// =============================================================================
class BroadPhase {
public:
    std::vector<PhysicsBody*> bodies;
    
    // AABB Tree for broad phase
    struct Node {
        Aabb bounds;
        int left = -1, right = -1;
        PhysicsBody* body = nullptr;
        bool isLeaf = true;
    };
    
    std::vector<Node> nodes;
    int root = -1;
    
    bool addBody(PhysicsBody* body) {
        if (!body) return false;
        if (std::find(bodies.begin(), bodies.end(), body) != bodies.end()) {
            return false;
        }
        bodies.push_back(body);
        // NOTE: sweep-and-prune over `bodies` is the active broadphase. The
        // AABB-tree node array below is reserved for future work; we
        // deliberately do NOT append a node here any more, because removed
        // bodies left dangling pointers in it forever.
        return true;
    }
    
    void removeBody(PhysicsBody* body) {
        for (auto it = bodies.begin(); it != bodies.end(); ++it) {
            if (*it == body) {
                bodies.erase(it);
                return;
            }
        }
    }
    
    // Simple sweep and prune
    std::vector<std::pair<PhysicsBody*, PhysicsBody*>> collidePairs() {
        std::vector<std::pair<PhysicsBody*, PhysicsBody*>> pairs;
        
        // Sort by x
        std::vector<PhysicsBody*> sorted = bodies;
        std::sort(sorted.begin(), sorted.end(), [](const PhysicsBody* a, const PhysicsBody* b) {
            return a->aabb.min.x < b->aabb.min.x;
        });
        
        // Sweep and prune
        for (size_t i = 0; i < sorted.size(); ++i) {
            for (size_t j = i + 1; j < sorted.size(); ++j) {
                if (sorted[j]->aabb.min.x > sorted[i]->aabb.max.x) break;
                if (sorted[i]->aabb.intersects(sorted[j]->aabb)) {
                    pairs.emplace_back(sorted[i], sorted[j]);
                }
            }
        }
        
        return pairs;
    }
};

// =============================================================================
// Narrow Phase - SAT collision detection
// =============================================================================
class NarrowPhase {
public:
    struct Contact {
        Vec3 normal;
        float depth;
        Vec3 point;
        PhysicsBody* bodyA;
        PhysicsBody* bodyB;
    };
    
    std::vector<Contact> contacts;
    
    bool testAabb(const Aabb& a, const Aabb& b) {
        return a.intersects(b);
    }
    
    // SAT for AABB vs AABB
    bool testAabbAabb(const Aabb& a, const Aabb& b, Contact& contact) {
        if (!a.intersects(b)) return false;
        
        Vec3 overlap = Vec3{
            std::min(a.max.x - b.min.x, b.max.x - a.min.x),
            std::min(a.max.y - b.min.y, b.max.y - a.min.y),
            std::min(a.max.z - b.min.z, b.max.z - a.min.z)
        };
        
        float minOverlap = std::min({overlap.x, overlap.y, overlap.z});
        
        contact.depth = minOverlap;
        contact.bodyA = nullptr;
        contact.bodyB = nullptr;
        
        // Contact normals always point from body A toward body B.  The
        // resolver moves A opposite the normal and B along it.
        if (minOverlap == overlap.x) {
            contact.normal = (a.center().x < b.center().x) ? Vec3{1, 0, 0} : Vec3{-1, 0, 0};
        } else if (minOverlap == overlap.y) {
            contact.normal = (a.center().y < b.center().y) ? Vec3{0, 1, 0} : Vec3{0, -1, 0};
        } else {
            contact.normal = (a.center().z < b.center().z) ? Vec3{0, 0, 1} : Vec3{0, 0, -1};
        }
        
        contact.point = a.center();
        return true;
    }
};

// =============================================================================
// Physics Integrator
// =============================================================================
class PhysicsIntegrator {
public:
    float dt;
    Vec3 gravity;
    
    PhysicsIntegrator(float dt, const Vec3& gravity = Vec3{0, -9.81f, 0})
        : dt(dt), gravity(gravity) {}
    
    void integrate(PhysicsBody* body) {
        if (body->isStatic) return;
        
        float invMass = body->inverseMass;
        
        // Apply forces
        body->velocity += body->force * invMass * dt;
        body->velocity += gravity * dt;
        
        // Apply damping
        body->velocity *= 0.999f;
        
        // Integrate position
        body->centerOfMass += body->velocity * dt;
        
        // Update AABB
        body->updateAabb();
        
        // Reset forces
        body->force = Vec3::zero();
    }
    
    void applyForce(PhysicsBody* body, const Vec3& force) {
        body->force += force;
    }
    
    void applyImpulse(PhysicsBody* body, const Vec3& impulse) {
        if (body->isStatic) return;
        body->velocity += impulse * body->inverseMass;
    }
};

// =============================================================================
// Physics System
// =============================================================================
class PhysicsSystem {
public:
    BroadPhase broadPhase;
    NarrowPhase narrowPhase;
    PhysicsIntegrator integrator;
    
    std::vector<PhysicsBody*> bodies;
    
    PhysicsSystem(float dt = 1.0f / 60.0f, const Vec3& gravity = Vec3{0, -9.81f, 0})
        : integrator(dt, gravity) {}
    
    bool addBody(PhysicsBody* body) {
        if (!body) return false;
        if (std::find(bodies.begin(), bodies.end(), body) != bodies.end()) {
            return false;
        }
        body->updateAabb();
        if (!broadPhase.addBody(body)) {
            return false;
        }
        bodies.push_back(body);
        return true;
    }
    
    void removeBody(PhysicsBody* body) {
        broadPhase.removeBody(body);
        for (auto it = bodies.begin(); it != bodies.end(); ++it) {
            if (*it == body) {
                bodies.erase(it);
                return;
            }
        }
    }
    
    void update() {
        // Integrate
        for (auto* body : bodies) {
            integrator.integrate(body);
        }
        
        // Broad phase
        auto pairs = broadPhase.collidePairs();
        
        // Narrow phase
        narrowPhase.contacts.clear();
        for (auto& [a, b] : pairs) {
            NarrowPhase::Contact contact;
            if (narrowPhase.testAabbAabb(a->aabb, b->aabb, contact)) {
                contact.bodyA = a;
                contact.bodyB = b;
                narrowPhase.contacts.push_back(contact);
            }
        }
        
        // Resolve collisions
        resolveContacts();
    }
    
private:
    void resolveContacts() {
        for (auto& contact : narrowPhase.contacts) {
            // Simple impulse resolution
            PhysicsBody* a = contact.bodyA;
            PhysicsBody* b = contact.bodyB;
            
            // Triggers report contacts but never apply physical correction or
            // impulses.
            if (a->isTrigger || b->isTrigger) continue;

            const float invA = a->isStatic ? 0.0f : a->inverseMass;
            const float invB = b->isStatic ? 0.0f : b->inverseMass;
            const float totalInvMass = invA + invB;
            if (totalInvMass <= 0.0f) continue;
            
            // Separate using effective inverse masses.  Static bodies have an
            // effective inverse mass of zero regardless of their stored value.
            const Vec3 separation = contact.normal * contact.depth;
            if (invA > 0.0f) {
                a->centerOfMass -= separation * (invA / totalInvMass);
                a->updateAabb();
            }
            if (invB > 0.0f) {
                b->centerOfMass += separation * (invB / totalInvMass);
                b->updateAabb();
            }
            
            // Impulse
            const Vec3 relativeVel = b->velocity - a->velocity;
            const float velAlongNormal = relativeVel.dot(contact.normal);
            
            if (velAlongNormal > 0.0f) continue; // Moving apart
            
            const float restitution = 0.3f;
            const float j = -(1.0f + restitution) * velAlongNormal / totalInvMass;
            const Vec3 impulse = contact.normal * j;
            
            if (invA > 0.0f) a->velocity -= impulse * invA;
            if (invB > 0.0f) b->velocity += impulse * invB;
        }
    }
};

} // namespace litt
