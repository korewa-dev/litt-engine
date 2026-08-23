//! GAL errors.

use crate::id::{BufferId, ImageId};
use crate::BackendKind;

/// Everything that can go wrong through the abstraction layer.
#[derive(Debug)]
pub enum GalError {
    /// Requested backend is not compiled in or unknown.
    UnknownBackend(String),
    /// Requested backend is known but no device was registered.
    BackendUnavailable(BackendKind),
    /// Stale or foreign handle passed to a device.
    InvalidHandle(String),
    /// Descriptor rejected by validation.
    InvalidDescriptor(String),
    /// Backend failed to materialize a logical resource.
    CreateFailed(String),
    /// Command list failed to translate on some backend.
    TranslateFailed { backend: BackendKind, detail: String },
    /// Migration target could not host the logical resources.
    MigrationFailed(String),
}

impl std::fmt::Display for GalError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            GalError::UnknownBackend(s) => write!(f, "unknown graphics backend: {s}"),
            GalError::BackendUnavailable(b) => write!(f, "backend not registered: {}", b.name()),
            GalError::InvalidHandle(s) => write!(f, "invalid resource handle: {s}"),
            GalError::InvalidDescriptor(s) => write!(f, "invalid descriptor: {s}"),
            GalError::CreateFailed(s) => write!(f, "resource creation failed: {s}"),
            GalError::TranslateFailed { backend, detail } => {
                write!(f, "command translation failed on {}: {}", backend.name(), detail)
            }
            GalError::MigrationFailed(s) => write!(f, "backend migration failed: {s}"),
        }
    }
}

impl std::error::Error for GalError {}

impl GalError {
    /// Convenience: invalid buffer handle error with both ids for context.
    pub fn stale_buffer(got: BufferId) -> Self {
        GalError::InvalidHandle(format!("buffer #{} gen{}", got.index(), got.generation()))
    }

    /// Convenience: invalid image handle error with both ids for context.
    pub fn stale_image(got: ImageId) -> Self {
        GalError::InvalidHandle(format!("image #{} gen{}", got.index(), got.generation()))
    }
}
