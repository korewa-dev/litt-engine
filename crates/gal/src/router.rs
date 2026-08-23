//! The router -- where translation actually happens.
//!
//! Owns all live backend devices, designates a primary, and provides:
//! * `translate()` -- replay one neutral [`CommandList`] on every device
//!   (primary first). Game code records once; Vulkan, DX12 and AGS all
//!   receive identical work.
//! * `set_primary()` -- hot-swap the primary backend. Logical resource
//!   descriptors are re-materialized on the new primary (migration), so
//!   handles stay valid without game code knowing anything changed.

use crate::backend::BackendKind;
use crate::device::{CommandList, GraphicsDevice};
use crate::error::GalError;

/// One registered device plus the logical descriptors needed to rebuild its
/// resources on another backend.
struct Entry {
    device: Box<dyn GraphicsDevice>,
    /// Every buffer ever created through the router (desc + handle).
    buffers: Vec<(crate::id::BufferId, crate::desc::BufferDesc)>,
    images: Vec<(crate::id::ImageId, crate::desc::ImageDesc)>,
    /// Images materialized here via `set_primary` migration.
    migrated_images: Vec<crate::id::ImageId>,
    /// Buffers materialized here via `set_primary` migration.
    migrated_buffers: Vec<crate::id::BufferId>,
}

impl Entry {
    fn new(device: Box<dyn GraphicsDevice>) -> Self {
        Self {
            device,
            buffers: Vec::new(),
            images: Vec::new(),
            migrated_images: Vec::new(),
            migrated_buffers: Vec::new(),
        }
    }
}

/// Multi-backend router and translation engine.
pub struct BackendRouter {
    entries: Vec<Entry>,
    primary: usize,
}

impl BackendRouter {
    /// Empty router. Register at least one device before use.
    pub fn new() -> Self {
        Self { entries: Vec::new(), primary: 0 }
    }

    /// Register a device; returns its registration index.
    pub fn register(&mut self, device: Box<dyn GraphicsDevice>) -> usize {
        self.entries.push(Entry::new(device));
        self.entries.len() - 1
    }

    /// Number of registered devices.
    pub fn len(&self) -> usize {
        self.entries.len()
    }

    /// True when no devices are registered.
    pub fn is_empty(&self) -> bool {
        self.entries.is_empty()
    }

    /// Primary device (shared borrows).
    pub fn primary(&self) -> &dyn GraphicsDevice {
        &*self.entries[self.primary].device
    }

    /// Primary device (exclusive borrows).
    pub fn primary_mut(&mut self) -> &mut dyn GraphicsDevice {
        let i = self.primary;
        &mut *self.entries[i].device
    }

    /// Device by registration index.
    pub fn device(&self, index: usize) -> Option<&dyn GraphicsDevice> {
        self.entries.get(index).map(|e| &*e.device)
    }

    /// All devices, in registration order (for iteration in tests/tools).
    pub fn devices(&self) -> impl Iterator<Item = &dyn GraphicsDevice> {
        self.entries.iter().map(|e| &*e.device)
    }

    /// Find the registration index of a backend kind.
    pub fn find(&self, kind: BackendKind) -> Option<usize> {
        self.entries.iter().position(|e| e.device.backend() == kind)
    }

    // -- resource creation through the router --------------------------------

    /// Create a buffer on the PRIMARY device and remember the descriptor so
    /// migrations can recreate it elsewhere.
    pub fn create_buffer(&mut self, desc: crate::desc::BufferDesc) -> Result<crate::id::BufferId, GalError> {
        let id = self.primary_mut().create_buffer(desc)?;
        if let Some(e) = self.entries.get_mut(self.primary) {
            e.buffers.push((id, desc));
        }
        Ok(id)
    }

    /// Create an image on the PRIMARY device and track it for migration.
    pub fn create_image(&mut self, desc: crate::desc::ImageDesc) -> Result<crate::id::ImageId, GalError> {
        let id = self.primary_mut().create_image(desc)?;
        if let Some(e) = self.entries.get_mut(self.primary) {
            e.images.push((id, desc));
        }
        Ok(id)
    }

    /// Create a pipeline on the primary only (pipelines are cheap to rebuild;
    /// they are NOT migrated automatically).
    pub fn create_pipeline(&mut self, desc: crate::desc::PipelineDesc) -> Result<crate::id::PipelineId, GalError> {
        self.primary_mut().create_pipeline(desc)
    }

    // -- translation ----------------------------------------------------------

    /// Replay `list` on EVERY registered device, primary first.
    ///
    /// A failure on any secondary is returned but does not stop other
    /// backends from executing -- mirrors real multi-API debugging where one
    /// API may reject an optional feature.
    pub fn translate(&mut self, list: &CommandList) -> Result<(), GalError> {
        if self.entries.is_empty() {
            return Err(GalError::BackendUnavailable(BackendKind::Null));
        }
        let mut first_err: Option<GalError> = None;
        for (i, entry) in self.entries.iter_mut().enumerate() {
            let res = entry.device.execute(list);
            if let Err(err) = res {
                let wrapped = match err {
                    GalError::TranslateFailed { detail, .. } => GalError::TranslateFailed {
                        backend: entry.device.backend(),
                        detail,
                    },
                    other => GalError::TranslateFailed {
                        backend: entry.device.backend(),
                        detail: other.to_string(),
                    },
                };
                if i == self.primary {
                    return Err(wrapped); // primary failure is fatal for the frame
                }
                first_err.get_or_insert(wrapped);
            }
        }
        match first_err {
            Some(e) => Err(e),
            None => Ok(()),
        }
    }

    // -- migration --------------------------------------------------------------

    /// Make registration index `index` the new primary, re-creating every
    /// tracked logical resource there so existing handles keep working.
    pub fn set_primary(&mut self, index: usize) -> Result<(), GalError> {
        if index >= self.entries.len() {
            return Err(GalError::MigrationFailed(format!("no such device slot {index}")));
        }

        // Snapshot logical state from the OLD primary's tracking tables,
        // then materialize on the new target.
        let snapshot_buffers: Vec<(crate::id::BufferId, crate::desc::BufferDesc)> =
            self.entries[self.primary].buffers.clone();
        let snapshot_images: Vec<(crate::id::ImageId, crate::desc::ImageDesc)> =
            self.entries[self.primary].images.clone();

        for (id, desc) in &snapshot_buffers {
            self.entries[index]
                .device
                .create_buffer(*desc)
                .map_err(|e| GalError::MigrationFailed(format!("{id}: {e}")))?;
            self.entries[index].buffers.push((*id, *desc));
            self.entries[index].migrated_buffers.push(*id);
        }
        for (id, desc) in &snapshot_images {
            self.entries[index]
                .device
                .create_image(*desc)
                .map_err(|e| GalError::MigrationFailed(format!("{id}: {e}")))?;
            self.entries[index].images.push((*id, *desc));
            self.entries[index].migrated_images.push(*id);
        }

        self.primary = index;
        Ok(())
    }

    /// True when registration slot `index` has materialized a copy of `id`
    /// through migration. Handles are PER-BACKEND in real APIs; the logical
    /// identity is what the router tracks.
    pub fn is_migrated_on(&self, index: usize, id: crate::id::ImageId) -> bool {
        self.entries
            .get(index)
            .map(|e| e.migrated_images.iter().any(|m| m.bits() == id.bits()))
            .unwrap_or(false)
    }

    /// Current primary registration index.
    pub fn primary_index(&self) -> usize {
        self.primary
    }
}

impl Default for BackendRouter {
    fn default() -> Self {
        Self::new()
    }
}
