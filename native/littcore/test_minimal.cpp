// Minimal compilation test
#include "litt_math.h"
#include "litt_ecs.h"
#include "litt_scene.h"
#include "litt_renderer.h"
#include "litt_physics.h"
#include "litt_collision.h"
#include "litt_bvh.h"
#include "litt_input.h"

using namespace litt;

int main() {
    Vec3 pos(1, 2, 3);
    Aabb bounds(Vec3::zero(), Vec3::one());
    
    World world;
    Entity e = world.create();
    
    PhysicsBody body;
    body.aabb = bounds;
    
    MeshData mesh;
    mesh.positions.push_back(pos);
    
    return 0;
}
