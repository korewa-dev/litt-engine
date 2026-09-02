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
```rust
// template/src/components/mesh.rs
#[derive(Clone, Debug, Pod, Zeroable)]
#[repr(C)]
pub struct Vertex {
    pub position: Vec3,
    pub normal: Vec3,
    pub texcoord: Vec2,
}

#[derive(Clone, Debug, Default)]
pub struct Mesh {
    pub vertices: Vec<Vertex>,
    pub indices: Vec<u32>,
    pub bounding_box: Option<Bbox>,
}
```

### Material
```rust
// template/src/components/material.rs
#[derive(Clone, Copy, Debug, Pod, Zeroable)]
#[repr(C)]
pub struct Material {
    pub albedo: Vec3,
    pub roughness: f32,
    pub metallic: f32,
    pub ior: f32,
    pub emissive: Vec3,
    pub light_intensity: f32,
    pub _pad: [f32; 3],
}
```

### Light
```rust
// template/src/components/light.rs
#[derive(Clone, Copy, Debug, Pod, Zeroable)]
#[repr(C)]
pub struct Light {
    pub position: Vec3,
    pub direction: Vec3,
    pub color: Vec3,
    pub intensity: f32,
    pub radius: f32,
    pub _pad: [f32; 2],
}
```

## Planned Components

See [reference.md](./reference.md) for the complete component table including AI, physics, input, UI, and network components.

