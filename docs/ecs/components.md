# ECS Components Reference

> Component definitions from `template/src/components/` and planned subsystems.

## Implemented Components

### Transform
```rust
// template/src/components/transform.rs
#[derive(Clone, Debug, Default)]
pub struct Transform {
    pub position: Vec3,
    pub rotation: Quat,
    pub scale: Vec3,
}

impl Transform {
    pub fn new() -> Self { Self::default() }
    pub fn matrix(&self) -> Mat4 {
        Mat4::translate(self.position.0, self.position.1, self.position.2)
            * self.rotation.to_mat4()
            * Mat4::scale(self.scale.0, self.scale.1, self.scale.2)
    }
}
```

### Camera
```rust
// template/src/components/camera.rs
#[derive(Clone, Debug)]
pub struct Camera {
    pub position: Vec3,
    pub rotation: Vec2,
    pub fov: f32,
    pub near_plane: f32,
    pub far_plane: f32,
    pub aspect: f32,
    pub exposure: f32,
}
```

### Player
```rust
// template/src/components/player.rs
#[derive(Clone, Debug)]
pub struct Player {
    pub position: Vec3,
    pub rotation: Vec2,
    pub velocity: Vec3,
    pub speed: f32,
    pub look_speed: f32,
    pub is_ground: bool,
}
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

