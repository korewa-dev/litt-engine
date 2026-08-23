//! Headless reference backend.
//!
//! Materializes logical resources in tables and counts executed commands.
//! Two uses:
//! 1. AI-agent/headless runs -- the engine ethos: simulate without a GPU.
//! 2. Golden reference for adapter conformance tests.

use crate::backend::BackendKind;
use crate::caps::Capabilities;
use crate::desc::{BufferDesc, ImageDesc, PipelineDesc};
use crate::device::{DeviceStats, GraphicsDevice};
use crate::error::GalError;
use crate::id::{BufferId, ImageId, PipelineId};
use std::collections::HashMap;

#[derive(Default)]
struct SlotTable {
    /// slot -> generation currently live (generations start at 1)
    generations: HashMap<u32, u32>,
    next_slot: u32,
    /// Freed slots ready for reuse; generation already bumped on free.
    free_list: Vec<u32>,
}

impl SlotTable {
    fn alloc(&mut self) -> (u32, u32) {
        // Reuse a freed slot (with its bumped generation) before minting new.
        if let Some(slot) = self.free_list.pop() {
            let gen = self.generations[&slot];
            return (slot, gen);
        }
        let slot = self.next_slot;
        self.next_slot += 1;
        // Fresh slots start at generation 1 so gen 0 is never a valid handle.
        let gen = *self.generations.entry(slot).or_insert(1);
        (slot, gen)
    }

    /// Validate a handle; on success bump generation so the stale copy dies.
    fn free(&mut self, index: u32, generation: u32) -> Result<(), GalError> {
        if generation == 0 {
            return Err(GalError::InvalidHandle(format!("slot {index} gen 0 is never valid")));
        }
        match self.generations.get(&index) {
            Some(g) if *g == generation => {
                self.generations.insert(index, generation.wrapping_add(1));
                self.free_list.push(index);
                Ok(())
            }
            Some(_) | None => Err(GalError::InvalidHandle(format!("slot {index} gen {generation}")))
        }
    }

    fn is_live(&self, index: u32, generation: u32) -> bool {
        generation != 0
            && matches!(self.generations.get(&index), Some(g) if *g == generation)
    }
}

/// Null backend: validates everything, renders nothing, counts all.
pub struct NullDevice {
    display_name: String,
    buffers: SlotTable,
    images: SlotTable,
    pipelines: SlotTable,
    stats: DeviceStats,
}

impl NullDevice {
    /// New null device with the default name "null".
    pub fn new() -> Self {
        Self::with_name("null")
    }

    /// New null device with a custom display name (multi-device tests).
    pub fn with_name(name: &str) -> Self {
        Self {
            display_name: format!("{name} (headless)"),
            buffers: SlotTable::default(),
            images: SlotTable::default(),
            pipelines: SlotTable::default(),
            stats: DeviceStats::default(),
        }
    }

    fn validate_buffer_desc(desc: BufferDesc) -> Result<(), GalError> {
        if desc.size == 0 {
            return Err(GalError::InvalidDescriptor("buffer size must be > 0".into()));
        }
        if desc.usage.is_empty() {
            return Err(GalError::InvalidDescriptor("buffer usage empty".into()));
        }
        Ok(())
    }

    fn validate_image_desc(desc: ImageDesc) -> Result<(), GalError> {
        if desc.width == 0 || desc.height == 0 || desc.depth == 0 {
            return Err(GalError::InvalidDescriptor("image extent must be non-zero".into()));
        }
        if desc.usage.is_empty() {
            return Err(GalError::InvalidDescriptor("image usage empty".into()));
        }
        Ok(())
    }
}

impl Default for NullDevice {
    fn default() -> Self {
        Self::new()
    }
}

impl GraphicsDevice for NullDevice {
    fn backend(&self) -> BackendKind {
        BackendKind::Null
    }

    fn name(&self) -> &str {
        &self.display_name
    }

    fn caps(&self) -> Capabilities {
        Capabilities::NULL
    }

    fn stats(&self) -> DeviceStats {
        self.stats
    }

    fn create_buffer(&mut self, desc: BufferDesc) -> Result<BufferId, GalError> {
        Self::validate_buffer_desc(desc)?;
        let (idx, gen) = self.buffers.alloc();
        self.stats.buffers_created += 1;
        Ok(BufferId::pack(idx, gen))
    }

    fn create_image(&mut self, desc: ImageDesc) -> Result<ImageId, GalError> {
        Self::validate_image_desc(desc)?;
        let (idx, gen) = self.images.alloc();
        self.stats.images_created += 1;
        Ok(ImageId::pack(idx, gen))
    }

    fn create_pipeline(&mut self, _desc: PipelineDesc) -> Result<PipelineId, GalError> {
        let (idx, gen) = self.pipelines.alloc();
        self.stats.pipelines_created += 1;
        Ok(PipelineId::pack(idx, gen))
    }

    fn destroy_buffer(&mut self, id: BufferId) -> Result<(), GalError> {
        self.buffers.free(id.index(), id.generation())
    }

    fn destroy_image(&mut self, id: ImageId) -> Result<(), GalError> {
        self.images.free(id.index(), id.generation())
    }

    fn destroy_pipeline(&mut self, id: PipelineId) -> Result<(), GalError> {
        self.pipelines.free(id.index(), id.generation())
    }

    fn has_image(&self, id: ImageId) -> bool {
        self.images.is_live(id.index(), id.generation())
    }

    fn has_buffer(&self, id: BufferId) -> bool {
        self.buffers.is_live(id.index(), id.generation())
    }

    fn execute(&mut self, list: &crate::device::CommandList) -> Result<(), GalError> {
        for cmd in list.commands() {
            match cmd {
                crate::device::Command::Dispatch(..) => self.stats.dispatches += 1,
                crate::device::Command::Draw { .. } | crate::device::Command::DrawIndexed { .. } => {
                    self.stats.draws += 1
                }
                crate::device::Command::Barrier | crate::device::Command::PresentBarrier(_) => {
                    self.stats.barriers += 1
                }
                crate::device::Command::PushConstants { .. } => self.stats.push_constant_writes += 1,
                _ => {}
            }
        }
        Ok(())
    }
}
