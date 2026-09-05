# Litt Engine C++ Core
# Production-grade C++17 math and engine libraries

## Overview

This directory contains the core C++ libraries for the Litt Engine. All code is pure C++17 with no external dependencies beyond standard libraries.

## Libraries

| Library | Header | Description |
|---------|--------|-------------|
| Math | `litt_math.h` | Vectors, matrices, quaternions, AABB, ray/triangle intersection |
| ECS | `litt_ecs.h` | Entity Component System with archetype storage |
| Input | `litt_input.h` | Keyboard, mouse, controller input handling |
| World | `litt_world.h` | World simulation, game state, physics |
| Scene | `litt_scene.h` | Scene graph, node management |
| Physics | `litt_physics.h` | Rigid body physics, collision detection |
| Audio | `litt_audio.h` | Audio playback, 3D sound |
| UI | `litt_ui.h` | User interface elements |
| Config | `litt_config.h` | Settings and presets |
| Profiler | `litt_profiler.h` | Performance profiling |

## Quick Start

```cpp
#include "littcore/litt.h"

int main() {
    // Use math types
    litt::Vec3f position = {0, 5, -10};
    litt::Vec3f direction = {0, 0, -1}.normalized();
    
    // Create ECS world
    litt::World world;
    auto entity = world.create_entity();
    world.add_component<litt::Transform>(entity, position);
    
    return 0;
}
```

## Math Types

### Vec2, Vec3, Vec4

```cpp
litt::Vec3f v1(1, 2, 3);
litt::Vec3f v2 = v1 * 2.0f;        // (2, 4, 6)
litt::Vec3f v3 = v1 + v2;          // (3, 6, 9)
litt::Vec3f v4 = v1.cross(v2);     // Cross product
float d = v1.dot(v2);              // Dot product
litt::Vec3f n = v1.normalized();   // Normalized
litt::Vec3f l = v1.lerp(v2, 0.5f); // Linear interpolation
```

### Mat4

```cpp
litt::Mat4f identity = litt::Mat4f::identity();
litt::Mat4f trans = litt::Mat4f::translation(litt::Vec3f(1, 2, 3));
litt::Mat4f rot = litt::Mat4f::rotation_y(3.14f / 4);
litt::Mat4f scale = litt::Mat4f::scale(litt::Vec3f(2, 2, 2));
litt::Mat4f persp = litt::Mat4f::perspective(60.0f, 16.0f/9.0f, 0.1f, 1000.0f);
litt::Mat4f lookat = litt::Mat4f::look_at(camera_pos, target, up);

// Multiply
litt::Mat4f mvp = projection * view * model;
```

### Quat

```cpp
litt::Quatf q1 = litt::Quatf::identity();
litt::Quatf q2 = litt::Quatf::from_axis_angle(litt::Vec3f::unit_y(), 0.5f);
litt::Quatf q3 = litt::Quatf::from_euler(litt::Vec3f{0, 0.5f, 0});

// Multiply
litt::Quatf q4 = q1 * q2;

// Slerp
litt::Quatf q5 = q1.slerp(q2, 0.5f);

// Convert to matrix
litt::Mat4f m = q1.to_mat4();
```

### AABB

```cpp
litt::Aabbf aabb({-1, -1, -1}, {1, 1, 1});
bool inside = aabb.contains(point);
bool intersect = aabb.intersects(other_aabb);
litt::Vec3f center = aabb.center();
litt::Vec3f size = aabb.size();
```

### Ray Intersection

```cpp
litt::Rayf ray(origin, direction);
litt::HitInfof hit = litt::math::ray_aabb(ray, aabb);
litt::HitInfof tri_hit = litt::math::ray_triangle(ray, v0, v1, v2);
```

## ECS Usage

```cpp
litt::World world;

// Create entity
auto player = world.create_entity();

// Add components
world.add_component<litt::Transform>(player, {0, 0, 0});
world.add_component<litt::RigidBody>(player);
world.add_component<litt::Mesh>(player, 1);

// Get components
auto* transform = world.get_component<litt::Transform>(player);
transform->data = {1, 0, 0};  // Move

// Check components
if (world.has_component<litt::RigidBody>(player)) {
    // Has physics
}
```

## Build

```bash
cd native
chmod +x build.sh
./build.sh linux release
```

## License

See [LICENSE](../../LICENSE)
