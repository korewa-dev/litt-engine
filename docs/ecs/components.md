# ECS Components Reference

> Component definitions from `template/src/components/` and planned subsystems.

## Implemented Components

### Transform
```cpp
struct Transform {
    Vec3 position;
    Quat rotation;
    Vec3 scale;
};
```

### Camera
```cpp
struct Camera {
    Vec3 position;
    Vec2 rotation;
    float fov;
    float near_plane;
    float far_plane;
    float aspect;
    float exposure;
};
```

### Player
```cpp
struct Player {
    Vec3 position;
    Vec2 rotation;
    Vec3 velocity;
    float speed;
    float look_speed;
    bool is_ground;
};
```

### Mesh
```cpp
struct Vertex {
    Vec3 position;
    Vec3 normal;
    Vec2 texcoord;
};

struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    Bbox* bounding_box;
};
```

### Material
```cpp
struct Material {
    Vec3 albedo;
    float roughness;
    float metallic;
    float ior;
    Vec3 emissive;
    float light_intensity;
    float _pad[3];
};
```

### Light
```cpp
struct Light {
    Vec3 position;
    Vec3 direction;
    Vec3 color;
    float intensity;
    float radius;
    float _pad[2];
};
```

## Planned Components

See [reference.md](./reference.md) for the complete component table including AI, physics, input, UI, and network components.

