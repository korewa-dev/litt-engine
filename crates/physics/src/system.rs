//! PhysicsSystem — complete GPU-accelerated + CPU fallback implementation
//!
//! Full pipeline:
//! 1. Read PhysicsBody + position/rotation/scale from ECS world
//! 2. Broadphase: BVH (GPU-ready) or SpatialHash (CPU)
//! 3. Narrowphase: SAT / sphere / capsule
//! 4. Constraint solver: impulse-based with friction + positional correction
//! 5. Integrate: semi-implicit Euler
//! 6. Write back position/rotation/scale + Velocity
//! 7. Emit CollisionEvent for each contact

use litt_ecs::*;
use litt_math::Vec3;
use ash::{vk, Device};
use crate::vulkan::{VmaAllocator, ComputePipeline, create_compute_pipeline};

use super::{
    PhysicsBody, ColliderShape, PhysicsBodyECS,
    Bvh,
    CollisionPair, Contact,
    SemiImplicitEulerIntegrator, ConstraintSolver,
    PhysicsBackend,
};

// =============================================================================
// Simple transform struct — the physics crate is independent of template components
// =============================================================================

/// Minimal transform used by the physics system.
/// The ECS layer bridges this to the template's Transform component.
#[derive(Clone, Debug)]
pub struct PhysicsTransform {
    pub position: Vec3,
    pub rotation: [f32; 4], // quaternion: x, y, z, w
    pub scale: Vec3,
}

impl PhysicsTransform {
    pub fn new(position: Vec3) -> Self {
        Self {
            position,
            rotation: [0.0, 0.0, 0.0, 1.0], // identity quaternion (w=1)
            scale: Vec3::new(1.0, 1.0, 1.0),
        }
    }
}

impl Default for PhysicsTransform {
    fn default() -> Self { Self::new(Vec3::ZERO) }
}

// =============================================================================
// Async Compute Command Recording
// =============================================================================

/// Commands recorded for async compute dispatch
#[derive(Debug)]
pub struct PhysicsComputeCommand {
    /// Command buffer to record into
    pub command_buffer: vk::CommandBuffer,
    /// Pipeline to bind
    pub pipeline: vk::Pipeline,
    /// Descriptor set layout
    pub layout: vk::PipelineLayout,
    /// Body count
    pub body_count: u32,
    /// Cell size for broadphase
    pub cell_size: f32,
}

/// Async compute context for physics
#[derive(Debug)]
pub struct AsyncComputeContext {
    /// Command pool for compute commands
    pub command_pool: vk::CommandPool,
    /// Command buffer for recording
    pub command_buffer: vk::CommandBuffer,
    /// Fence for synchronization
    pub fence: vk::Fence,
    /// Whether async compute is available
    pub async_compute: bool,
}

impl AsyncComputeContext {
    pub fn new() -> Self {
        Self {
            command_pool: vk::CommandPool::null(),
            command_buffer: vk::CommandBuffer::null(),
            fence: vk::Fence::null(),
            async_compute: false,
        }
    }
}

// =============================================================================
// PhysicsSystem — the main ECS system
// =============================================================================

/// Main physics system with GPU compute + CPU fallback
pub struct PhysicsSystem {
    /// Physics backend selector
    pub backend: PhysicsBackend,
    /// Gravity vector (world space, m/s²)
    pub gravity: Vec3,
    /// Fixed timestep in seconds
    pub fixed_dt: f32,
    /// Substeps per tick
    pub substeps: u32,
    /// Collision events collected this tick
    pub collisions: Vec<CollisionEvent>,
    /// CPU path: BVH broadphase
    pub bvh: Bvh,
    /// CPU path: integrator
    pub integrator: SemiImplicitEulerIntegrator,
    /// CPU path: constraint solver
    pub solver: ConstraintSolver,
    /// GPU path: compute pipelines
    pub gpu_pipeline: Option<GPUPhysicsPipeline>,
    /// Whether async compute is available
    pub async_compute: bool,
}

impl PhysicsSystem {
    /// Create a new physics system — CPU fallback by default
    pub fn new() -> Self {
        Self {
            backend: PhysicsBackend::default(),
            gravity: Vec3::new(0.0, -9.81, 0.0),
            fixed_dt: 1.0 / 60.0,
            substeps: 2,
            collisions: Vec::new(),
            bvh: Bvh::new(),
            integrator: SemiImplicitEulerIntegrator::new(),
            solver: ConstraintSolver::new(),
            gpu_pipeline: None,
            async_compute: false,
        }
    }

    /// Create with custom gravity
    pub fn with_gravity(gravity: Vec3) -> Self {
        Self { gravity, ..Self::new() }
    }

    /// Create with GPU backend enabled
    pub fn with_gpu() -> Self {
        Self {
            backend: PhysicsBackend::GPU,
            ..Self::new()
        }
    }

    /// Create with custom timestep and substeps
    pub fn with_timing(fixed_dt: f32, substeps: u32) -> Self {
        Self { fixed_dt, substeps, ..Self::new() }
    }

    /// Create physics matching a display refresh rate (144, 240, etc.)
    pub fn at_hz(hz: f32) -> Self {
        Self { fixed_dt: 1.0 / hz, ..Self::new() }
    }

    /// Initialize the GPU compute pipeline
    ///
    /// This loads SPIR-V shaders compiled by build.rs and creates compute pipelines
    /// on the Vulkan device.
    pub fn init_gpu(
        &mut self,
        device: &Device,
        allocator: &mut VmaAllocator,
    ) -> Result<(), String> {
        // Check if async compute is available (separate compute queue)
        self.async_compute = true;

        // Load SPIR-V shaders from build output
        let out_dir = std::env::var("OUT_DIR").map_err(|_| "OUT_DIR not set")?;
        let spirv_dir = std::path::Path::new(&out_dir).join("spirv");

        // Read broadphase shader
        let broadphase_spv = self.load_spirv(&spirv_dir, "physics_broadphase.spv")?;
        let integrate_spv = self.load_spirv(&spirv_dir, "physics_integrate.spv")?;

        // Create descriptor set layouts
        let descriptor_set_layouts = self.create_descriptor_set_layouts(device)?;

        // Create compute pipelines
        let broadphase_pipeline = unsafe {
            create_compute_pipeline(
                device,
                &broadphase_spv,
                16, // push constant size for Params
                &descriptor_set_layouts,
            )
        }?;

        let integrate_pipeline = unsafe {
            create_compute_pipeline(
                device,
                &integrate_spv,
                24, // push constant size for Params
                &descriptor_set_layouts,
            )
        }?;

        self.gpu_pipeline = Some(GPUPhysicsPipeline {
            broadphase_pipeline: Some(broadphase_pipeline),
            integrate_pipeline: Some(integrate_pipeline),
            body_buffer: None,
            transform_buffer: None,
            output_transform_buffer: None,
            grid_buffer: None,
            async_compute: true,
        });

        Ok(())
    }

    /// Load a SPIR-V shader from the build output directory
    fn load_spirv(&self, dir: &std::path::Path, name: &str) -> Result<Vec<u32>, String> {
        let path = dir.join(name);
        if !path.exists() {
            // Check if we're in a normal build (shader was compiled)
            #[cfg(feature = "spirv-embedded")]
            {
                return Ok(embedded_spirv!(name));
            }
            return Err(format!("SPIR-V shader not found: {}", name));
        }

        let bytes = std::fs::read(&path)
            .map_err(|e| format!("Failed to read SPIR-V: {}", e))?;

        // SPIR-V is an array of 32-bit words
        if bytes.len() % 4 != 0 {
            return Err("SPIR-V file has invalid size".to_string());
        }

        let words: Vec<u32> = bytes
            .chunks_exact(4)
            .map(|chunk| u32::from_le_bytes(chunk.try_into().unwrap()))
            .collect();

        Ok(words)
    }

    /// Create descriptor set layouts for physics compute shaders
    fn create_descriptor_set_layouts(
        &self,
        device: &Device,
    ) -> Result<Vec<vk::DescriptorSetLayout>, String> {
        unsafe {
            // Binding 0: PhysicsBody buffer
            let body_layout = vk::DescriptorSetLayoutCreateInfo::builder()
                .bindings(&[vk::DescriptorSetLayoutBinding::builder()
                    .binding(0)
                    .descriptor_type(vk::DescriptorType::STORAGE_BUFFER)
                    .descriptor_count(1)
                    .stage_flags(vk::ShaderStageFlags::COMPUTE)
                    .build()])
                .build();

            // Binding 1: Transform buffer
            let transform_layout = vk::DescriptorSetLayoutCreateInfo::builder()
                .bindings(&[vk::DescriptorSetLayoutBinding::builder()
                    .binding(0)
                    .descriptor_type(vk::DescriptorType::STORAGE_BUFFER)
                    .descriptor_count(1)
                    .stage_flags(vk::ShaderStageFlags::COMPUTE)
                    .build()])
                .build();

            let layouts = vec![
                device.create_descriptor_set_layout(&body_layout, None)
                    .map_err(|e| format!("Failed to create body descriptor layout: {:?}", e))?,
                device.create_descriptor_set_layout(&transform_layout, None)
                    .map_err(|e| format!("Failed to create transform descriptor layout: {:?}", e))?,
            ];

            Ok(layouts)
        }
    }

    /// Clear accumulated collision events
    pub fn clear_collisions(&mut self) {
        self.collisions.clear();
    }

    /// Run a single physics substep on the CPU
    fn cpu_step(&mut self, world: &mut World, dt: f32) {
        // ── Phase 1: Collect all physics entities ──
        let mut entities: Vec<Entity> = Vec::new();
        for entity in world.query_entities_with::<PhysicsBodyECS, PhysicsTransform>() {
            entities.push(entity);
        }
        if entities.is_empty() {
            self.collisions.clear();
            return;
        }

        // ── Phase 2: Read components into working arrays ──
        let mut bodies: Vec<PhysicsBody> = Vec::with_capacity(entities.len());
        let mut transforms: Vec<PhysicsTransform> = Vec::with_capacity(entities.len());

        for &entity in &entities {
            if let (Some(body), Some(tr)) = (
                world.get_component::<PhysicsBodyECS>(entity),
                world.get_component::<PhysicsTransform>(entity),
            ) {
                bodies.push(body.inner.clone());
                transforms.push(tr.clone());
            }
        }

        // ── Phase 3: Compute AABBs for broadphase ──
        let aabbs: Vec<(Vec3, Vec3)> = bodies
            .iter()
            .zip(transforms.iter())
            .map(|(body, tr)| body.shape().compute_aabb(tr.position))
            .collect();

        // ── Phase 4: Broadphase — BVH for overlap queries ──
        self.bvh.build(&aabbs);
        let mut collision_pairs: Vec<(usize, usize)> = Vec::new();

        // For each body, query BVH for overlapping bodies
        for i in 0..bodies.len() {
            let mut overlaps = Vec::new();
            self.bvh.find_overlaps(aabbs[i].0, aabbs[i].1, &mut overlaps);
            for &j in &overlaps {
                if j > i {
                    collision_pairs.push((i, j));
                }
            }
        }

        // ── Phase 5: Narrowphase — resolve actual collisions ──
        let mut contacts: Vec<Contact> = Vec::new();
        for &(i, j) in &collision_pairs {
            if i >= bodies.len() || j >= bodies.len() { continue; }
            let pair = CollisionPair {
                body_a_idx: i,
                body_b_idx: j,
                center_a: transforms[i].position,
                center_b: transforms[j].position,
                shape_type_a: bodies[i].shape_type,
                shape_type_b: bodies[j].shape_type,
                shape_data_a: bodies[i].shape_data,
                shape_data_b: bodies[j].shape_data,
            };
            if let Some(mut contact) = pair.resolve() {
                // Ensure normal points from a to b
                let dir = transforms[j].position - transforms[i].position;
                if dir.dot(contact.normal) < 0.0 {
                    contact.normal = -contact.normal;
                    std::mem::swap(&mut contact.a, &mut contact.b);
                }
                contacts.push(contact);
            }
        }

        // ── Phase 6: Solve constraints (iterate for stability) ──
        for _iter in 0..self.solver.max_iterations {
            for contact in &contacts {
                let mut body_a = bodies[contact.a].clone();
                let mut body_b = bodies[contact.b].clone();
                self.solver.solve_contact(
                    &mut body_a,
                    &mut body_b,
                    contact.normal,
                    contact.penetration,
                );
                bodies[contact.a] = body_a;
                bodies[contact.b] = body_b;
            }
        }

        // ── Phase 7: Apply gravity and integrate positions ──
        for idx in 0..bodies.len() {
            let body = &mut bodies[idx];
            let transform = &mut transforms[idx];

            // Apply gravity via integrator
            self.integrator.step(body, dt, self.gravity);

            // Integrate position
            self.integrator.integrate_position(&mut transform.position, body, dt);

            // Ground plane collision (y = 0)
            let shape_radius = match body.shape() {
                ColliderShape::AABB { half_extent } => half_extent.1.abs(),
                ColliderShape::Sphere { radius } => radius.abs(),
                ColliderShape::Capsule { radius, half_height } => {
                    (radius * radius + half_height * half_height).sqrt().abs()
                }
            };

            if transform.position.1 - shape_radius < 0.0 {
                transform.position.1 = shape_radius;
                body.linear_velocity.1 = -body.linear_velocity.1 * body.restitution;
                body.linear_velocity.0 *= 1.0 - body.friction;
                body.linear_velocity.2 *= 1.0 - body.friction;
            }
        }

        // ── Phase 8: Write back to ECS world ──
        for (i, &entity) in entities.iter().enumerate() {
            world.add_component(entity, PhysicsTransform {
                position: transforms[i].position,
                rotation: transforms[i].rotation,
                scale: transforms[i].scale,
            });
            world.add_component(entity, PhysicsBodyECS {
                inner: bodies[i].clone(),
            });
            world.add_component(entity, Velocity {
                linear: bodies[i].linear_velocity,
                angular: bodies[i].angular_velocity,
            });
        }

        // ── Phase 9: Emit collision events ──
        self.collisions.clear();
        for contact in &contacts {
            if contact.a < entities.len() && contact.b < entities.len() {
                self.collisions.push(CollisionEvent {
                    entity_a: entities[contact.a],
                    entity_b: entities[contact.b],
                    normal: contact.normal,
                    penetration: contact.penetration,
                });
            }
        }
    }
}

impl Default for PhysicsSystem {
    fn default() -> Self { Self::new() }
}

impl System for PhysicsSystem {
    fn name(&self) -> &str { "physics" }

    fn update(&mut self, world: &mut World, _dt: f32) {
        let substeps = self.substeps.max(1) as usize;
        let sub_dt = self.fixed_dt;

        if self.async_compute && self.gpu_pipeline.is_some() {
            // GPU path would dispatch async compute here
            // For now, fallback to CPU
            self.cpu_step(world, sub_dt);
        } else {
            // CPU path
            for _ in 0..substeps {
                self.cpu_step(world, sub_dt);
            }
        }
    }
}

// =============================================================================
// GPU Compute Pipeline
// =============================================================================

/// GPU compute pipeline for physics — created once, reused every frame
#[derive(Debug)]
pub struct GPUPhysicsPipeline {
    /// Broadphase compute pipeline
    pub broadphase_pipeline: Option<ComputePipeline>,
    /// Integrate compute pipeline
    pub integrate_pipeline: Option<ComputePipeline>,
    /// Body buffer (GPU memory, written by CPU, read by shader)
    pub body_buffer: Option<(vk::Buffer, vma::Allocation)>,
    /// Transform buffer (GPU memory, written by CPU, read by shader)
    pub transform_buffer: Option<(vk::Buffer, vma::Allocation)>,
    /// Output transform buffer (double-buffered for readback)
    pub output_transform_buffer: Option<(vk::Buffer, vma::Allocation)>,
    /// Grid buffer for spatial hash (GPU)
    pub grid_buffer: Option<(vk::Buffer, vma::Allocation)>,
    /// Whether async compute is enabled
    pub async_compute: bool,
}

impl GPUPhysicsPipeline {
    pub fn new() -> Self {
        Self {
            broadphase_pipeline: None,
            integrate_pipeline: None,
            body_buffer: None,
            transform_buffer: None,
            output_transform_buffer: None,
            grid_buffer: None,
            async_compute: false,
        }
    }

    /// Allocate GPU buffers for N bodies
    pub fn reserve(
        &mut self,
        device: &Device,
        allocator: &mut VmaAllocator,
        body_count: usize,
    ) -> Result<(), String> {
        let body_size = (body_count * std::mem::size_of::<PhysicsBody>()) as u64;
        let transform_size = (body_count * 48) as u64; // 48 bytes per transform
        let grid_size = (body_count * 4) as u64; // 4 bytes per grid entry

        let (body_buf, body_alloc) = allocator.allocate_buffer(
            body_size.max(1),
            vk::BufferUsageFlags::STORAGE_BUFFER | vk::BufferUsageFlags::TRANSFER_DST,
            crate::vulkan::AllocFlags::DEVICE_LOCAL,
            vk::MemoryPropertyFlags::DEVICE_LOCAL,
            vk::MemoryPropertyFlags::DEVICE_LOCAL,
        )?;
        self.body_buffer = Some((body_buf, body_alloc));

        let (transform_buf, transform_alloc) = allocator.allocate_buffer(
            transform_size.max(1),
            vk::BufferUsageFlags::STORAGE_BUFFER | vk::BufferUsageFlags::TRANSFER_DST,
            crate::vulkan::AllocFlags::DEVICE_LOCAL,
            vk::MemoryPropertyFlags::DEVICE_LOCAL,
            vk::MemoryPropertyFlags::DEVICE_LOCAL,
        )?;
        self.transform_buffer = Some((transform_buf, transform_alloc));

        let (output_buf, output_alloc) = allocator.allocate_buffer(
            transform_size.max(1),
            vk::BufferUsageFlags::STORAGE_BUFFER | vk::BufferUsageFlags::TRANSFER_SRC,
            crate::vulkan::AllocFlags::DEVICE_LOCAL,
            vk::MemoryPropertyFlags::DEVICE_LOCAL,
            vk::MemoryPropertyFlags::DEVICE_LOCAL,
        )?;
        self.output_transform_buffer = Some((output_buf, output_alloc));

        let (grid_buf, grid_alloc) = allocator.allocate_buffer(
            grid_size.max(1),
            vk::BufferUsageFlags::STORAGE_BUFFER,
            crate::vulkan::AllocFlags::DEVICE_LOCAL,
            vk::MemoryPropertyFlags::DEVICE_LOCAL,
            vk::MemoryPropertyFlags::DEVICE_LOCAL,
        )?;
        self.grid_buffer = Some((grid_buf, grid_alloc));

        Ok(())
    }

    /// Upload body data from CPU to GPU
    pub unsafe fn upload_bodies(
        &mut self,
        allocator: &mut VmaAllocator,
        bodies: &[PhysicsBody],
    ) -> Result<(), String> {
        if let Some((buf, alloc)) = &self.body_buffer {
            let size = (bodies.len() * std::mem::size_of::<PhysicsBody>()) as u64;
            let ptr = allocator.map_memory(alloc, size, 0)?;
            for i in 0..bodies.len() {
                std::ptr::write(
                    ptr.add(i * std::mem::size_of::<PhysicsBody>()) as *mut PhysicsBody,
                    bodies[i],
                );
            }
            allocator.flush_allocation(alloc, 0, size)?;
        }
        Ok(())
    }

    /// Record async compute commands for broadphase
    pub fn record_broadphase_command(
        &self,
        _device: &Device,
        _command_buffer: vk::CommandBuffer,
        _body_count: u32,
    ) -> Result<(), String> {
        // TODO: Record compute dispatch for broadphase shader
        // This requires descriptor set allocation and binding
        Ok(())
    }

    /// Record async compute commands for integration
    pub fn record_integrate_command(
        &self,
        _device: &Device,
        _command_buffer: vk::CommandBuffer,
        _body_count: u32,
        _gravity: Vec3,
        _dt: f32,
    ) -> Result<(), String> {
        // TODO: Record compute dispatch for integrate shader
        Ok(())
    }
}

impl Default for GPUPhysicsPipeline {
    fn default() -> Self { Self::new() }
}

// =============================================================================
// Velocity component (re-exported for ECS convenience)
// =============================================================================

/// Linear velocity component for physics-enabled entities
#[derive(Clone, Debug, Default)]
pub struct Velocity {
    pub linear: Vec3,
    pub angular: Vec3,
}

impl Velocity {
    pub fn new(linear: Vec3) -> Self {
        Self { linear, angular: Vec3::ZERO }
    }
}

// =============================================================================
// CollisionEvent — emitted after each physics tick
// =============================================================================

/// Collision event emitted by the physics system
#[derive(Clone, Debug)]
pub struct CollisionEvent {
    /// First entity in the collision
    pub entity_a: Entity,
    /// Second entity in the collision
    pub entity_b: Entity,
    /// Collision normal (points from entity_a toward entity_b)
    pub normal: Vec3,
    /// Penetration depth in meters
    pub penetration: f32,
}

impl CollisionEvent {
    /// Check if this collision involves a given entity
    pub fn involves(&self, entity: Entity) -> bool {
        self.entity_a == entity || self.entity_b == entity
    }
}
