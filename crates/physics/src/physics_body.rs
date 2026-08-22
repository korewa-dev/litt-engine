//! PhysicsBody component -- the core data type for GPU/CPU physics simulation
//!
//! Matches the spec in docs/physics/physics-system.md exactly.

use litt_math::Vec3;
use bytemuck::{Pod, Zeroable};

// =============================================================================
// ColliderShape -- matches the spec from docs/physics/physics-system.md
// =============================================================================

/// Collider shape types supported by the physics system.
#[derive(Clone, Copy, Debug, PartialEq, Default)]
pub enum ColliderShape {
    /// Axis-aligned bounding box
    #[default]
    AABB {
        /// Half-extents along each axis
        half_extent: Vec3,
    },
    /// Sphere
    Sphere {
        /// Radius in world units
        radius: f32,
    },
    /// Capsule (cylinder + two hemispheres)
    Capsule {
        /// Radius of the capsule
        radius: f32,
        /// Half-height of the cylindrical section
        half_height: f32,
    },
}

impl ColliderShape {
    /// Create an AABB collider
    pub fn aabb(half_extent: Vec3) -> Self {
        Self::AABB { half_extent }
    }

    /// Create a sphere collider
    pub fn sphere(radius: f32) -> Self {
        Self::Sphere { radius }
    }

    /// Create a capsule collider
    pub fn capsule(radius: f32, half_height: f32) -> Self {
        Self::Capsule { radius, half_height }
    }

    /// Get the bounding sphere radius for broadphase culling
    pub fn bounding_radius(&self) -> f32 {
        match self {
            Self::AABB { half_extent } => half_extent.length(),
            Self::Sphere { radius } => *radius,
            Self::Capsule { radius, half_height } => {
                ((radius * radius) + (half_height * half_height)).sqrt()
            }
        }
    }

    /// Get the AABB for this shape (axis-aligned bounding box of the shape)
    pub fn compute_aabb(&self, center: Vec3) -> (Vec3, Vec3) {
        match self {
            Self::AABB { half_extent } => (center - *half_extent, center + *half_extent),
            Self::Sphere { radius } => {
                let h = Vec3::new(*radius, *radius, *radius);
                (center - h, center + h)
            }
            Self::Capsule { radius, half_height } => {
                let h = Vec3::new(*radius, *half_height, *radius);
                (center - h, center + h)
            }
        }
    }
}

// =============================================================================
// GPU-ready PhysicsBody layout (matches the GLSL struct in compute shaders)
// =============================================================================

/// Physics body component attached to simulating entities.
///
/// All fields are laid out for GPU compute shader access via #[repr(C)].
/// Total size: 128 bytes (matches the GLSL struct in shaders/compute/physics_*.comp.glsl)
#[derive(Clone, Copy, Debug, Pod, Zeroable)]
#[repr(C, align(16))]
pub struct PhysicsBody {
    /// Collider shape type index (0=AABB, 1=Sphere, 2=Capsule)
    pub shape_type: u32,
    /// Mass in kg (0.0 = static/kinematic)
    pub mass: f32,
    /// Inverse mass (precomputed, 0.0 for static)
    pub inv_mass: f32,
    /// Linear velocity (m/s)
    pub linear_velocity: Vec3,
    /// Angular velocity (rad/s)
    pub angular_velocity: Vec3,
    /// Linear damping (0.0-1.0)
    pub linear_damping: f32,
    /// Angular damping (0.0-1.0)
    pub angular_damping: f32,
    /// Friction coefficient (0.0-1.0)
    pub friction: f32,
    /// Restitution / bounciness (0.0-1.0)
    pub restitution: f32,
    /// Collision layer bitmask
    pub layer: u32,
    /// Trigger flag (no response, only notify)
    pub is_trigger: u32,
    /// Gravity scale (1.0 = full gravity)
    pub gravity_scale: f32,
    /// Shape-specific data (union-like):
    ///   AABB: half_extent (x,y,z)
    ///   Sphere: radius + 3 pad
    ///   Capsule: radius + half_height + 2 pad
    pub shape_data: [f32; 4],
}

impl PhysicsBody {
    /// Create a new dynamic physics body
    pub fn new(shape: ColliderShape, mass: f32) -> Self {
        let inv_mass = if mass > 0.0 { 1.0 / mass } else { 0.0 };
        let (shape_type, shape_data) = Self::encode_shape(&shape);
        Self {
            shape_type,
            mass,
            inv_mass,
            linear_velocity: Vec3::ZERO,
            angular_velocity: Vec3::ZERO,
            linear_damping: 0.1,
            angular_damping: 0.1,
            friction: 0.6,
            restitution: 0.2,
            layer: 0xFFFFFFFF,
            is_trigger: 0,
            gravity_scale: 1.0,
            shape_data,
        }
    }

    /// Create a static (immovable) physics body
    pub fn static_body(shape: ColliderShape) -> Self {
        let mut body = Self::new(shape, 0.0);
        body
    }

    /// Create a kinematic body (moves but not affected by forces)
    pub fn kinematic_body(shape: ColliderShape) -> Self {
        let mut body = Self::new(shape, 0.0);
        body.is_trigger = 1;
        body
    }

    /// Check if this body is static
    pub fn is_static(&self) -> bool { self.mass <= 0.0 }

    /// Check if this body is a trigger
    pub fn is_trigger(&self) -> bool { self.is_trigger != 0 }

    /// Encode shape into type index + data
    fn encode_shape(shape: &ColliderShape) -> (u32, [f32; 4]) {
        match shape {
            ColliderShape::AABB { half_extent } => (0, [half_extent.0, half_extent.1, half_extent.2, 0.0]),
            ColliderShape::Sphere { radius } => (1, [*radius, 0.0, 0.0, 0.0]),
            ColliderShape::Capsule { radius, half_height } => (2, [*radius, *half_height, 0.0, 0.0]),
        }
    }

    /// Get the shape from this body
    pub fn shape(&self) -> ColliderShape {
        match self.shape_type {
            0 => ColliderShape::AABB {
                half_extent: Vec3::new(self.shape_data[0], self.shape_data[1], self.shape_data[2]),
            },
            1 => ColliderShape::Sphere { radius: self.shape_data[0] },
            2 => ColliderShape::Capsule {
                radius: self.shape_data[0],
                half_height: self.shape_data[1],
            },
            _ => ColliderShape::default(),
        }
    }
}

impl Default for PhysicsBody {
    fn default() -> Self {
        Self::new(ColliderShape::default(), 1.0)
    }
}

// =============================================================================
// CPU-only PhysicsBody (for ECS storage, cloneable)
// =============================================================================

/// ECS-compatible PhysicsBody wrapper (cloneable, not GPU-direct)
#[derive(Clone, Debug)]
pub struct PhysicsBodyECS {
    pub inner: PhysicsBody,
}

impl PhysicsBodyECS {
    pub fn new(shape: ColliderShape, mass: f32) -> Self {
        Self { inner: PhysicsBody::new(shape, mass) }
    }

    pub fn static_body(shape: ColliderShape) -> Self {
        Self { inner: PhysicsBody::static_body(shape) }
    }
}

impl Default for PhysicsBodyECS {
    fn default() -> Self { Self::new(ColliderShape::default(), 1.0) }
}

impl From<PhysicsBody> for PhysicsBodyECS {
    fn from(inner: PhysicsBody) -> Self { Self { inner } }
}

impl From<PhysicsBodyECS> for PhysicsBody {
    fn from(wrapper: PhysicsBodyECS) -> Self { wrapper.inner }
}

// =============================================================================
// Tests
// =============================================================================

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_physics_body_creation() {
        let body = PhysicsBody::new(ColliderShape::sphere(0.5), 1.0);
        assert!((body.mass - 1.0).abs() < 1e-6);
        assert!((body.inv_mass - 1.0).abs() < 1e-6);
        assert!(!body.is_static());
        assert_eq!(body.shape_type, 1); // Sphere
        assert!((body.shape_data[0] - 0.5).abs() < 1e-6);
    }

    #[test]
    fn test_static_body() {
        let body = PhysicsBody::static_body(ColliderShape::aabb(Vec3::new(1.0, 1.0, 1.0)));
        assert!(body.is_static());
        assert!((body.mass - 0.0).abs() < 1e-6);
        assert!((body.inv_mass - 0.0).abs() < 1e-6);
        assert_eq!(body.shape_type, 0); // AABB
    }

    #[test]
    fn test_sphere_aabb() {
        let body = PhysicsBody::new(ColliderShape::sphere(2.0), 1.0);
        let (min, max) = body.shape().compute_aabb(Vec3::new(5.0, 3.0, 1.0));
        assert!((min.0 - 3.0).abs() < 1e-6);
        assert!((min.1 - 1.0).abs() < 1e-6);
        assert!((min.2 - (-1.0)).abs() < 1e-6);
        assert!((max.0 - 7.0).abs() < 1e-6);
        assert!((max.1 - 5.0).abs() < 1e-6);
        assert!((max.2 - 3.0).abs() < 1e-6);
    }

    #[test]
    fn test_aabb_aabb() {
        let body = PhysicsBody::new(ColliderShape::aabb(Vec3::new(1.0, 2.0, 1.0)), 1.0);
        let (min, max) = body.shape().compute_aabb(Vec3::new(0.0, 0.0, 0.0));
        assert!((min.0 - (-1.0)).abs() < 1e-6);
        assert!((min.1 - (-2.0)).abs() < 1e-6);
        assert!((min.2 - (-1.0)).abs() < 1e-6);
        assert!((max.0 - 1.0).abs() < 1e-6);
        assert!((max.1 - 2.0).abs() < 1e-6);
        assert!((max.2 - 1.0).abs() < 1e-6);
    }

    #[test]
    fn test_capsule_bounding_radius() {
        let shape = ColliderShape::capsule(0.5, 1.0);
        let radius = shape.bounding_radius();
        let expected = (0.25 + 1.0).sqrt();
        assert!((radius - expected).abs() < 1e-6);
    }

    #[test]
    fn test_physics_body_gpu_layout() {
        // Verify the struct is 128 bytes and properly aligned
        assert_eq!(std::mem::size_of::<PhysicsBody>(), 128);
        assert_eq!(std::mem::align_of::<PhysicsBody>(), 16);
    }

}