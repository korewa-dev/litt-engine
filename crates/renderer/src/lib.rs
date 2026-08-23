//! Main renderer -- orchestrates vulkan, pathtracer, and fidelityfx.
//! Complete pipeline with BLAS/TLAS, FSR 3.1.5 upscaler, and CAS sharpening.

pub mod renderer;
pub mod command_pool;
pub mod render_pass;
pub mod descriptor;
pub mod particle;
pub mod spatial;
pub mod screenshot;

pub use renderer::*;
pub use command_pool::*;
pub use render_pass::*;
pub use descriptor::*;
pub use particle::*;
pub use spatial::*;
pub use screenshot::*;

use ash::{vk, Device};
use litt_vulkan::{GpuAllocator, RtLoader};
use litt_fidelityfx::{
    CasConstants, CasPipeline, DisplayPipeline, Fsr3Pipeline, Fsr3Quality, PathTracerPipeline,
    PathTracePushConstants,
};
use litt_pathtracer::{upload_scene as pt_upload_scene, Camera, PathTracerBuffers, PathTracerConstants, Scene};

/// Complete rendering pipeline state
pub struct RenderPipeline {
    /// Path tracer buffers (GPU)
    pub path_tracer_buffers: PathTracerBuffers,
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

impl std::fmt::Debug for RenderPipeline {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("RenderPipeline")
            .field("frame_count", &self.frame_count)
            .field("path_trace_enabled", &self.path_trace_enabled)
            .field("fsr_enabled", &self.fsr_enabled)
            .field("is_initialized", &self.is_initialized)
            .finish()
    }
}

impl RenderPipeline {
    /// Create new render pipeline with FSR 3.1.5 and CAS
    #[allow(clippy::too_many_arguments)]
    pub fn new(
        device: &Device,
        rt_loader: &RtLoader,
        allocator: &mut GpuAllocator,
        scene: &Scene,
        camera: &Camera,
        width: u32,
        height: u32,
    ) -> Result<Self, String> {
        // Upload scene data to GPU
        let path_tracer_buffers = pt_upload_scene(device, scene, allocator)?;

        // Build acceleration structures (BLAS + TLAS) for ray tracing
        let _acceleration = litt_pathtracer::build_scene_acceleration(
            device, rt_loader, allocator, scene,
        )?;

        // Initialize FSR 3.1.5 at half resolution (common FSR quality setting)
        let fsr_enabled = true;
        let fsr_input_w = (width / 2).max(1);
        let fsr_input_h = (height / 2).max(1);
        let mut fsr_pipeline = Fsr3Pipeline::new();
        unsafe {
            fsr_pipeline.initialize(
                device,
                fsr_input_w,
                fsr_input_h,
                width.max(1),
                height.max(1),
                Fsr3Quality::Quality,
            )?;
        }

        // Initialize CAS sharpening at full resolution
        let mut cas_pipeline = CasPipeline::new();
        unsafe {
            cas_pipeline.initialize(device, width.max(1), height.max(1))?;
        }

        let cas_constants = CasConstants::default();
        let tracer_constants = PathTracerConstants::new(width.max(1), height.max(1), camera, scene);

        // Initialize GPU path tracer
        let path_trace_enabled = true;
        let mut path_tracer = PathTracerPipeline::new();
        unsafe {
            path_tracer.initialize(device, fsr_input_w, fsr_input_h)?;
        }

        // Initialize display/tone-map pipeline
        let mut display_pipeline = DisplayPipeline::new();
        unsafe {
            display_pipeline.initialize(
                device,
                fsr_input_w,
                fsr_input_h,
                width.max(1),
                height.max(1),
            )?;
        }

        Ok(Self {
            path_tracer_buffers,
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
        self.tracer_constants =
            PathTracerConstants::new(width.max(1), height.max(1), camera, scene);
        self.frame_count += 1;
    }

    /// Allocate the path tracer descriptor set for the current frame
    pub unsafe fn allocate_path_tracer_desc_set(&self) -> Result<vk::DescriptorSet, String> {
        litt_fidelityfx::allocate_path_tracer_descriptor_set(&self.path_tracer)
    }

    /// Reset temporal state
    pub fn reset_temporal(&mut self) {
        self.frame_count = 0;
    }
}

