//! Main renderer — orchestrates vulkan, pathtracer, and fidelityfx.
//! Complete pipeline with VMA, BLAS/TLAS, FSR 3.1.5 upscaler, and CAS sharpening.

pub mod renderer;
pub mod command_pool;
pub mod render_pass;
pub mod descriptor;

pub use renderer::*;
pub use command_pool::*;
pub use render_pass::*;
pub use descriptor::*;

use ash::{vk, Device};
use crate::vulkan::{VmaAllocator, AccelerationStructures};
use crate::fidelityfx::{Fsr3Pipeline, CasPipeline, PathTracerPipeline, PathTracePushConstants, CasConstants, allocate_path_tracer_descriptor_set, DisplayPipeline};
use crate::pathtracer::{PathTracerBuffers, PathTracerConstants, Scene, Camera};
use litt_math::*;

/// Complete rendering pipeline state
#[derive(Debug)]
pub struct RenderPipeline {
    /// Path tracer buffers (GPU)
    pub path_tracer_buffers: PathTracerBuffers,
    /// Acceleration structures (BLAS + TLAS)
    pub acceleration_structures: AccelerationStructures,
    /// FSR 3.1.5 upscaler pipeline
    pub fsr_pipeline: Fsr3Pipeline,
    /// CAS sharpening pipeline
    pub cas_pipeline: CasPipeline,
    /// CAS constants
    pub cas_constants: CasConstants,
    /// Path tracer constants
    pub tracer_constants: PathTracerConstants,
    /// Frame count
    pub frame_count: u32,
    /// Whether path tracing is enabled
    pub path_trace_enabled: bool,
    /// Path tracer compute pipeline
    pub path_tracer: PathTracerPipeline,
    /// Display / tone-map compute pipeline
    pub display_pipeline: DisplayPipeline,
    /// Whether FSR is enabled
    pub fsr_enabled: bool,
    /// Is initialized
    pub is_initialized: bool,
}

impl RenderPipeline {
    /// Create new render pipeline with FSR 3.1.5 and CAS
    pub fn new(
        device: &Device,
        rt_loader: &ash::extensions::khr::RayTracingPipeline,
        allocator: &mut VmaAllocator,
        scene: &Scene,
        camera: &Camera,
        width: u32,
        height: u32,
    ) -> Result<Self, String> {
        // Upload scene data to GPU
        let path_tracer_buffers = crate::pathtracer::upload_scene(device, scene, allocator)?;

        // Build acceleration structures
        let acceleration_structures = crate::pathtracer::build_scene_acceleration(
            device,
            rt_loader,
            allocator,
            scene,
        )?;

        // Initialize FSR 3.1.5 at half resolution (common FSR quality setting)
        let fsr_enabled = true;
        let fsr_input_w = width / 2;
        let fsr_input_h = height / 2;
        let mut fsr_pipeline = Fsr3Pipeline::new();
        unsafe {
            fsr_pipeline.initialize(
                device,
                fsr_input_w, fsr_input_h,
                width, height,
                crate::fidelityfx::Fsr3Quality::Quality,
            )?;
        }

        // Initialize CAS sharpening at full resolution
        let mut cas_pipeline = CasPipeline::new();
        unsafe {
            cas_pipeline.initialize(device, width, height)?;
        }

        let cas_constants = CasConstants::default();
        let tracer_constants = PathTracerConstants::new(width, height, camera, scene);

        // Initialize GPU path tracer
        let path_trace_enabled = true;
        let mut path_tracer = PathTracerPipeline::new();
        unsafe {
            path_tracer.initialize(device, fsr_input_w, fsr_input_h)?;
        }

        // Initialize display/tone-map pipeline
        let mut display_pipeline = DisplayPipeline::new();
        unsafe {
            display_pipeline.initialize(device, fsr_input_w, fsr_input_h, width, height)?;
        }

        Ok(Self {
            path_tracer_buffers,
            acceleration_structures,
            fsr_pipeline,
            cas_pipeline,
            cas_constants,
            tracer_constants,
            frame_count: 0,
            path_trace_enabled,
            path_tracer,
            display_pipeline,
            fsr_enabled,
            is_initialized: true,
        })
    }

    /// Update pipeline for new frame
    pub fn update(&mut self, camera: &Camera, scene: &Scene, width: u32, height: u32) {
        self.tracer_constants = PathTracerConstants::new(width, height, camera, scene);
        self.frame_count += 1;
    }

    /// Allocate the path tracer descriptor set for the current frame
    pub unsafe fn allocate_path_tracer_desc_set(&self) -> Result<vk::DescriptorSet, String> {
        allocate_path_tracer_descriptor_set(&self.path_tracer)
    }

    /// Reset temporal state
    pub fn reset_temporal(&mut self) {
        self.frame_count = 0;
    }
}

/// Complete application state
#[derive(Debug)]
pub struct AppState {
    pub pipeline: RenderPipeline,
    pub window_size: (u32, u32),
}

impl AppState {
    /// Create new application state
    pub fn new(
        device: &Device,
        rt_loader: &ash::extensions::khr::RayTracingPipeline,
        allocator: &mut VmaAllocator,
        scene: Scene,
        camera: Camera,
        width: u32,
        height: u32,
    ) -> Result<Self, String> {
        let pipeline = RenderPipeline::new(
            device,
            rt_loader,
            allocator,
            &scene,
            &camera,
            width,
            height,
        )?;

        Ok(Self {
            pipeline,
            window_size: (width, height),
        })
    }

    /// Resize the application
    pub fn resize(&mut self, width: u32, height: u32) -> Result<(), String> {
        self.window_size = (width, height);
        // Note: Full resize would need to recreate swapchain and buffers
        Ok(())
    }
}
