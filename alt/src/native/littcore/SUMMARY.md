# Litt Engine C++ Core - Summary

## Files Created

### Math Library (litt_math.h - 42.4 KB)
Complete production-grade C++17 math library with:
- **Vec2, Vec3, Vec4** - Full template-based vector types
  - All operators (+, -, *, /, ==, !=)
  - Dot product, cross product
  - Length, normalized, lerp, slerp
  - Clamp, reflect, refract
  - Rotation support
  
- **Mat4** - 4x4 column-major matrices
  - Identity, perspective, lookAt
  - Translation, rotation (X/Y/Z/axis-angle)
  - Scale, transpose, inverse
  - Vector multiplication
  
- **Quat** - Quaternions
  - Identity, from_axis_angle, from_euler
  - Multiplication, conjugate, inverse
  - Slerp, to_mat4, to_euler
  
- **Aabb** - Axis-aligned bounding boxes
  - Containment, intersection tests
  - Sphere intersection
  - Merge, expand, center, size
  
- **Plane** - Mathematical planes
  - Distance to point
  - Sphere intersection
  
- **Ray** - Ray structures
  - Origin, direction, t-min/max
  - Point at distance
  
- **HitInfo** - Intersection results
  - t value, point, normal, material
  
- **Math Utilities**
  - deg_to_rad, rad_to_deg
  - clamp, lerp, smoothstep
  - triangle_normal, barycentric
  - ray_aabb, ray_triangle
  
- **PCG Random**
  - 32/64-bit random generation
  - Float ranges
  - Point in sphere, on sphere

### ECS System (litt_ecs.h - 14.7 KB)
Entity Component System with:
- **Entity** - Unique IDs with generation for safe recycling
- **Component** - Template-based component base
- **ArchetypeStorage** - Cache-friendly storage
- **World** - Main ECS container
  - Entity creation/destruction
  - Component add/get/remove
  - System management
  - Serialization

### Input System (litt_input.h - 9.2 KB)
Complete input handling:
- **Key** enum (GLFW-compatible)
- **MouseButton** enum
- **InputState** class
  - Key down/up/just_pressed/just_released
  - Mouse position/delta/buttons
  - Scroll wheel
  - Action bindings
  - Default bindings

### World Simulation (litt_world.h - 11.7 KB)
World state management:
- **WorldConfig** - Game configuration
- **WorldEntity** - Entity data
- **WorldState** - Runtime state
- **WorldManager** - Main manager
  - Load/save scenes
  - Physics update
  - Entity management
  - Win/lose conditions

### Engine Core (litt_engine.h - 9.9 KB)
Main engine class:
- **EngineConfig** - Configuration
- **Engine** - Main class
  - Initialize/shutdown
  - Game loop (headless and windowed)
  - Scene management
  - Logging

### Renderer (litt_renderer.h - 10.9 KB)
Rendering abstraction:
- **RenderBackend** enum (Vulkan/DX12/OpenGL/Metal)
- **Mesh** - Mesh data
- **Material** - PBR material
- **Light** - Light types
- **Camera** - Camera with matrices
- **IRenderer** - Interface
- **Renderer** - Implementation
- **Scene** - Scene graph

### OBJ Loader (litt_obj_cpp.h - 13.9 KB)
Wavefront OBJ parser:
- **ObjVertex** - Vertex structure
- **ObjFace** - Face with indices
- **ObjMaterial** - Material parsing
- **ObjMesh** - Mesh data
- **ObjModel** - Complete model
- **ObjLoader** - Loader class
  - Load from file
  - Save to file
  - Compute normals
  - Compute bounds

### Build Scripts
- **build.sh** - Linux/Unix build script
- **build.bat** - Windows build script

## Usage Example

```cpp
#include "littcore/litt.h"

int main() {
    // Use math
    litt::Vec3f pos = {0, 5, -10};
    litt::Vec3f dir = {0, 0, -1}.normalized();
    
    // Create ECS world
    litt::World world;
    auto entity = world.create_entity();
    world.add_component<litt::Transform>(entity, pos);
    
    // Load model
    litt::ObjLoader loader;
    litt::ObjModel model;
    loader.load("assets/character.obj", model);
    
    // Run engine
    litt::Engine engine;
    engine.initialize({});
    engine.run();
    
    return 0;
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
