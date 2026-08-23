//! Asset handle -- unique identifier for loaded assets.
//! Provides type-safe referencing across the engine.

use std::hash::{Hash, Hasher};
use std::collections::hash_map::DefaultHasher;

/// Unique asset handle
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub struct AssetHandle {
    pub id: u64,
    pub type_: AssetType,
}

impl AssetHandle {
    pub fn new(id: u64, type_: AssetType) -> Self {
        Self { id, type_ }
    }

    /// Generate a handle from a path string
    pub fn from_path(path: &str, type_: AssetType) -> Self {
        let mut hasher = DefaultHasher::new();
        path.hash(&mut hasher);
        Self {
            id: hasher.finish(),
            type_,
        }
    }
}

/// Type of asset
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub enum AssetType {
    Model,
    Texture,
    Shader,
    Material,
    Font,
    Animation,
    Audio,
    Scene,
}

impl std::fmt::Display for AssetType {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Model => write!(f, "Model"),
            Self::Texture => write!(f, "Texture"),
            Self::Shader => write!(f, "Shader"),
            Self::Material => write!(f, "Material"),
            Self::Font => write!(f, "Font"),
            Self::Animation => write!(f, "Animation"),
            Self::Audio => write!(f, "Audio"),
            Self::Scene => write!(f, "Scene"),
        }
    }
}

/// Asset loading state
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum AssetState {
    /// Not yet loaded
    Pending,
    /// Currently loading
    Loading,
    /// Successfully loaded
    Loaded,
    /// Failed to load
    Error(String),
}

impl std::fmt::Display for AssetState {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Pending => write!(f, "Pending"),
            Self::Loading => write!(f, "Loading"),
            Self::Loaded => write!(f, "Loaded"),
            Self::Error(s) => write!(f, "Error: {}", s),
        }
    }
}
