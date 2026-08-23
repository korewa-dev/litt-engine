//! Device trait and the backend-neutral command list.
//!
//! `CommandList` is the translation currency: plain data with zero Vulkan
//! or D3D types. Every registered device receives the SAME recorded list,
//! which is what makes "develop on Vulkan, run on DX12/AGS" free.

use crate::backend::BackendKind;
use crate::caps::Capabilities;
use crate::desc::{BufferDesc, ImageDesc, PipelineDesc, SwapchainDesc};
use crate::error::GalError;
use crate::id::{BufferId, ImageId, PipelineId, SwapchainId};

/// One backend-neutral GPU command.
#[derive(Clone, Debug)]
pub enum Command {
    /// Bind a pipeline (graphics or compute) by handle.
    BindPipeline(PipelineId),
    /// Bind a buffer into slot N (vertex/index/storage/uniform per pipeline).
    BindBuffer(BufferId, u32),
    /// Bind an image into slot N.
    BindImage(ImageId, u32),
    /// Upload push constants / root constants at offset.
    PushConstants { data: Vec<u8>, offset: u32 },
    /// Compute dispatch in workgroups.
    Dispatch(u32, u32, u32),
    /// Draw triangles from currently bound buffers.
    Draw { vertex_count: u32, instance_count: u32 },
    /// Draw indexed.
    DrawIndexed { index_count: u32, instance_count: u32 },
    /// Full memory barrier between passes.
    Barrier,
    /// Transition the swapchain image for present (backends that care).
    PresentBarrier(SwapchainId),
}

/// A recorded, replayable sequence of commands. Cheap to clone.
#[derive(Clone, Debug)]
pub struct CommandList {
    /// Debug label ("shadow-pass", "pathtrace", ...).
    pub label: String,
    commands: Vec<Command>,
}

impl CommandList {
    /// Start recording an empty list.
    pub fn new(label: &str) -> Self {
        Self { label: label.to_string(), commands: Vec::new() }
    }

    /// All recorded commands, in order.
    pub fn commands(&self) -> &[Command] {
        &self.commands
    }

    /// Number of recorded commands.
    pub fn len(&self) -> usize {
        self.commands.len()
    }

    /// True when nothing has been recorded yet.
    pub fn is_empty(&self) -> bool {
        self.commands.is_empty()
    }

    /// Bind a compute or graphics pipeline.
    pub fn bind_pipeline(&mut self, pipeline: PipelineId) -> &mut Self {
        self.commands.push(Command::BindPipeline(pipeline));
        self
    }

    /// Bind a buffer to slot N.
    pub fn bind_buffer(&mut self, buffer: BufferId, slot: u32) -> &mut Self {
        self.commands.push(Command::BindBuffer(buffer, slot));
        self
    }

    /// Bind an image to slot N.
    pub fn bind_image(&mut self, image: ImageId, slot: u32) -> &mut Self {
        self.commands.push(Command::BindImage(image, slot));
        self
    }

    /// Overload matching the common single-slot case used in tests/examples.
    pub fn bind_buffer_0(&mut self, buffer: BufferId) -> &mut Self {
        self.bind_buffer(buffer, 0)
    }

    /// Write push/root constants.
    pub fn push_constants(&mut self, data: &[u8]) -> &mut Self {
        self.push_constants_at(0, data)
    }

    /// Write push/root constants at a byte offset.
    pub fn push_constants_at(&mut self, offset: u32, data: &[u8]) -> &mut Self {
        self.commands.push(Command::PushConstants { data: data.to_vec(), offset });
        self
    }

    /// Compute dispatch (workgroups).
    pub fn dispatch(&mut self, x: u32, y: u32, z: u32) -> &mut Self {
        self.commands.push(Command::Dispatch(x, y, z));
        self
    }

    /// Non-indexed draw.
    pub fn draw(&mut self, vertices: u32, instances: u32) -> &mut Self {
        self.commands.push(Command::Draw { vertex_count: vertices, instance_count: instances });
        self
    }

    /// Indexed draw.
    pub fn draw_indexed(&mut self, indices: u32, instances: u32) -> &mut Self {
        self.commands.push(Command::DrawIndexed { index_count: indices, instance_count: instances });
        self
    }

    /// Insert a full barrier (end of pass).
    pub fn barrier(&mut self) -> &mut Self {
        self.commands.push(Command::Barrier);
        self
    }
}

/// Execution statistics exposed by every device (debug HUD feeds on this).
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct DeviceStats {
    pub buffers_created: u64,
    pub images_created: u64,
    pub pipelines_created: u64,
    pub dispatches: u64,
    pub draws: u64,
    pub barriers: u64,
    pub push_constant_writes: u64,
}

/// The contract every graphics backend fulfills for the engine.
///
/// Implemented by NullDevice (headless), and behind features by the
/// Vulkan, DX12, and AGS adapters.
pub trait GraphicsDevice: Send {
    /// Which backend this device speaks natively.
    fn backend(&self) -> BackendKind;

    /// Human name for logs/HUD ("vulkan#0 (RX 6700 XT)").
    fn name(&self) -> &str;

    /// Capability matrix (RT/compute/bindless/...).
    fn caps(&self) -> Capabilities;

    /// Lifetime counters -- consumed by litt-profiler.
    fn stats(&self) -> DeviceStats;

    // -- resource creation ------------------------------------------------

    /// Materialize a logical buffer; returns an opaque handle.
    ///
    /// # Errors
    /// `InvalidDescriptor` for zero size / empty usage, backend-specific
    /// failures map to `CreateFailed`.
    fn create_buffer(&mut self, desc: BufferDesc) -> Result<BufferId, GalError>;

    /// Materialize a logical image.
    fn create_image(&mut self, desc: ImageDesc) -> Result<ImageId, GalError>;

    /// Compile/link a logical pipeline.
    fn create_pipeline(&mut self, desc: PipelineDesc) -> Result<PipelineId, GalError>;

    /// Create a presentation target (only meaningful for presenting backends).
    fn create_swapchain(&mut self, _desc: SwapchainDesc) -> Result<SwapchainId, GalError> {
        Err(GalError::CreateFailed(format!("{} cannot present", self.backend().name())))
    }

    // -- destruction -------------------------------------------------------

    /// Free a buffer. Stale handles are rejected with `InvalidHandle`.
    fn destroy_buffer(&mut self, id: BufferId) -> Result<(), GalError>;

    /// Free an image.
    fn destroy_image(&mut self, id: ImageId) -> Result<(), GalError>;

    /// Free a pipeline.
    fn destroy_pipeline(&mut self, id: PipelineId) -> Result<(), GalError>;

    // -- introspection used by migration -----------------------------------

    /// True when this device materialized the given image (post-migration check).
    fn has_image(&self, id: ImageId) -> bool;

    /// True when this device materialized the given buffer.
    fn has_buffer(&self, id: BufferId) -> bool;

    // -- execution ----------------------------------------------------------

    /// Translate + execute one recorded command list.
    ///
    /// This is where "translation" happens: each backend walks the neutral
    /// `Command` stream and lowers it to native API calls (or counts them,
    /// in the null backend).
    fn execute(&mut self, list: &CommandList) -> Result<(), GalError>;
}
