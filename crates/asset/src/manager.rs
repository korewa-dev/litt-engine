//! Asset manager -- central hub for loading and managing assets.
//! Provides type-safe asset loading with caching and referencing.

use std::collections::HashMap;
use std::path::{Path, PathBuf};
use super::handle::{AssetHandle, AssetType, AssetState};
use super::cache::AssetCache;
use super::model::{Model, GltfLoader, ObjLoader};
use super::texture::{Texture, ImageLoader};
use super::shader::{Shader, ShaderCompiler, ShaderStage, ShaderSource};
use super::material::Material;

/// The central asset manager
#[derive(Debug)]
pub struct AssetManager {
    /// All loaded assets
    pub assets: HashMap<AssetHandle, Asset>,
    /// Asset cache
    pub cache: AssetCache,
    /// Shader compiler
    pub shader_compiler: ShaderCompiler,
    /// Base path for asset loading
    pub base_path: PathBuf,
    /// Load statistics
    pub load_count: u32,
    pub error_count: u32,
}

/// Wrapper for any asset type
#[derive(Debug)]
pub enum Asset {
    Model(Box<Model>),
    Texture(Box<Texture>),
    Shader(Box<Shader>),
    Material(Box<Material>),
    Font(Box<super::font::Font>),
}

impl Asset {
    pub fn handle(&self) -> AssetHandle {
        match self {
            Self::Model(m) => m.handle,
            Self::Texture(t) => t.handle,
            Self::Shader(s) => s.handle,
            Self::Material(m) => m.handle,
            Self::Font(f) => f.handle,
        }
    }

    pub fn name(&self) -> &str {
        match self {
            Self::Model(m) => &m.name,
            Self::Texture(t) => &t.name,
            Self::Shader(s) => &s.name,
            Self::Material(m) => &m.name,
            Self::Font(f) => &f.name,
        }
    }

    pub fn state(&self) -> &AssetState {
        match self {
            Self::Model(m) => &m.meshes.first().map(|_| AssetState::Loaded).unwrap_or(AssetState::Pending),
            Self::Texture(t) => &t.state,
            Self::Shader(s) => &s.state,
            Self::Material(m) => &AssetState::Loaded,
            Self::Font(f) => &AssetState::Loaded,
        }
    }
}

impl Default for AssetManager {
    fn default() -> Self {
        Self::new()
    }
}

impl AssetManager {
    /// Create a new asset manager
    pub fn new() -> Self {
        Self {
            assets: HashMap::new(),
            cache: AssetCache::new(),
            shader_compiler: ShaderCompiler::new(),
            base_path: PathBuf::from("assets"),
            load_count: 0,
            error_count: 0,
        }
    }

    /// Set the base asset path
    pub fn with_base_path(mut self, path: &str) -> Self {
        self.base_path = PathBuf::from(path);
        self
    }

    /// Set the cache max size
    pub fn with_cache_size(mut self, max_bytes: usize) -> Self {
        self.cache = self.cache.with_max_size(max_bytes);
        self
    }

    /// Get the full path for an asset
    pub fn resolve_path(&self, path: &str) -> PathBuf {
        let p = Path::new(path);
        if p.is_absolute() {
            p.to_path_buf()
        } else {
            self.base_path.join(p)
        }
    }

    /// Load a model by path
    pub fn load_model(&mut self, path: &str) -> Result<AssetHandle, String> {
        let full_path = self.resolve_path(path);
        let handle = AssetHandle::from_path(path, AssetType::Model);

        if self.assets.contains_key(&handle) {
            return Ok(handle); // Already loaded
        }

        let ext = full_path.extension()
            .and_then(|e| e.to_str())
            .map(|s| s.to_lowercase())
            .unwrap_or_default();

        let model = match ext.as_str() {
            "gltf" | "glb" => GltfLoader::load_from_file(full_path.to_str().unwrap_or(path)),
            "obj" => ObjLoader::load_from_file(full_path.to_str().unwrap_or(path)),
            _ => Err(format!("Unknown model format: {}", ext)),
        }?;

        let size = model.total_vertices() * std::mem::size_of::<super::model::Vertex>()
            + model.total_indices() * std::mem::size_of::<u32>();

        self.assets.insert(handle, Asset::Model(Box::new(model)));
        self.cache.record(handle, size);
        self.load_count += 1;

        Ok(handle)
    }

    /// Load a texture by path
    pub fn load_texture(&mut self, path: &str) -> Result<AssetHandle, String> {
        let full_path = self.resolve_path(path);
        let handle = AssetHandle::from_path(path, AssetType::Texture);

        if self.assets.contains_key(&handle) {
            return Ok(handle); // Already loaded
        }

        let ext = full_path.extension()
            .and_then(|e| e.to_str())
            .map(|s| s.to_lowercase())
            .unwrap_or_default();

        let texture = match ext.as_str() {
            "ktx" | "ktx2" => {
                let data = std::fs::read(&full_path)
                    .map_err(|e| format!("Failed to read '{}': {}", path, e))?;
                ImageLoader::load_ktx2(handle, path, &data)
            }
            "png" | "jpg" | "jpeg" | "bmp" | "tga" | "webp" => {
                let data = std::fs::read(&full_path)
                    .map_err(|e| format!("Failed to read '{}': {}", path, e))?;
                ImageLoader::load_from_bytes(handle, path, &data)
            }
            _ => Err(format!("Unknown texture format: {}", ext)),
        }?;

        let size = texture.data.len();

        self.assets.insert(handle, Asset::Texture(Box::new(texture)));
        self.cache.record(handle, size);
        self.load_count += 1;

        Ok(handle)
    }

    /// Load a shader by path
    pub fn load_shader(&mut self, path: &str, stage: ShaderStage) -> Result<AssetHandle, String> {
        let full_path = self.resolve_path(path);
        let handle = AssetHandle::from_path(path, AssetType::Shader);

        if self.assets.contains_key(&handle) {
            return Ok(handle);
        }

        let ext = full_path.extension()
            .and_then(|e| e.to_str())
            .map(|s| s.to_lowercase())
            .unwrap_or_default();

        let source = match ext.as_str() {
            "glsl" | "vert" | "frag" | "comp" => {
                let content = std::fs::read_to_string(&full_path)
                    .map_err(|e| format!("Failed to read '{}': {}", path, e))?;
                ShaderSource::Glsl(content)
            }
            "hlsl" | "vert" | "frag" | "comp" => {
                let content = std::fs::read_to_string(&full_path)
                    .map_err(|e| format!("Failed to read '{}': {}", path, e))?;
                ShaderSource::Hlsl(content)
            }
            "spv" => {
                let data = std::fs::read(&full_path)
                    .map_err(|e| format!("Failed to read '{}': {}", path, e))?;
                if data.len() % 4 != 0 {
                    return Err("Invalid SPIR-V file".to_string());
                }
                let words: Vec<u32> = data.chunks_exact(4)
                    .map(|chunk| u32::from_le_bytes(chunk.try_into().unwrap()))
                    .collect();
                ShaderSource::SpirV(words)
            }
            "dxil" => {
                let data = std::fs::read(&full_path)
                    .map_err(|e| format!("Failed to read '{}': {}", path, e))?;
                ShaderSource::Dxil(data)
            }
            _ => return Err(format!("Unknown shader format: {}", ext)),
        };

        let mut shader = Shader::new(handle, path, stage, source);
        self.shader_compiler.compile(&mut shader)?;

        let size = match &shader.source {
            ShaderSource::SpirV(words) => words.len() * 4,
            ShaderSource::Dxil(bytes) => bytes.len(),
            ShaderSource::Glsl(s) => s.len(),
            ShaderSource::Hlsl(s) => s.len(),
            ShaderSource::Wgsl(s) => s.len(),
        };

        self.assets.insert(handle, Asset::Shader(Box::new(shader)));
        self.cache.record(handle, size);
        self.load_count += 1;

        Ok(handle)
    }

    /// Load a material by name
    pub fn load_material(&mut self, name: &str) -> AssetHandle {
        let handle = AssetHandle::from_path(name, AssetType::Material);
        if !self.assets.contains_key(&handle) {
            let material = Material::new(name);
            self.assets.insert(handle, Asset::Material(Box::new(material)));
            self.cache.record(handle, std::mem::size_of::<Material>());
            self.load_count += 1;
        }
        handle
    }

    /// Get a reference to a loaded asset
    pub fn get<T: 'static>(&self, handle: &AssetHandle) -> Option<&T> {
        self.assets.get(handle).and_then(|asset| {
            match asset {
                Asset::Model(m) => AnyCast::<T>::downcast_ref(m.as_ref()),
                Asset::Texture(t) => AnyCast::<T>::downcast_ref(t.as_ref()),
                Asset::Shader(s) => AnyCast::<T>::downcast_ref(s.as_ref()),
                Asset::Material(m) => AnyCast::<T>::downcast_ref(m.as_ref()),
                Asset::Font(f) => AnyCast::<T>::downcast_ref(f.as_ref()),
            }
        })
    }

    /// Get a mutable reference to a loaded asset
    pub fn get_mut<T: 'static>(&mut self, handle: &AssetHandle) -> Option<&mut T> {
        self.assets.get_mut(handle).and_then(|asset| {
            match asset {
                Asset::Model(m) => AnyCast::<T>::downcast_mut(m.as_mut()),
                Asset::Texture(t) => AnyCast::<T>::downcast_mut(t.as_mut()),
                Asset::Shader(s) => AnyCast::<T>::downcast_mut(s.as_mut()),
                Asset::Material(m) => AnyCast::<T>::downcast_mut(m.as_mut()),
                Asset::Font(f) => AnyCast::<T>::downcast_mut(f.as_mut()),
            }
        })
    }

    /// Unload an asset
    pub fn unload(&mut self, handle: &AssetHandle) -> bool {
        if let Some(asset) = self.assets.remove(handle) {
            let size = match asset {
                Asset::Model(m) => m.total_vertices() * std::mem::size_of::<super::model::Vertex>()
                    + m.total_indices() * std::mem::size_of::<u32>(),
                Asset::Texture(t) => t.data.len(),
                Asset::Shader(s) => match &s.source {
                    ShaderSource::SpirV(w) => w.len() * 4,
                    ShaderSource::Dxil(b) => b.len(),
                    _ => 0,
                },
                Asset::Material(_) => std::mem::size_of::<Material>(),
                Asset::Font(f) => f.data.len(),
            };
            self.cache.current_size_bytes = self.cache.current_size_bytes.saturating_sub(size);
            true
        } else {
            false
        }
    }

    /// Get cache statistics
    pub fn cache_stats(&self) -> super::cache::CacheStats {
        self.cache.stats()
    }

    /// Get load statistics
    pub fn stats(&self) -> ManagerStats {
        ManagerStats {
            total_assets: self.assets.len(),
            load_count: self.load_count,
            error_count: self.error_count,
            cache: self.cache.stats(),
        }
    }
}

/// Load statistics
#[derive(Debug)]
pub struct ManagerStats {
    pub total_assets: usize,
    pub load_count: u32,
    pub error_count: u32,
    pub cache: super::cache::CacheStats,
}

/// Type casting helper
trait AnyCast<T> {
    fn downcast_ref(&self) -> Option<&T>;
    fn downcast_mut(&mut self) -> Option<&mut T>;
}

impl<T: 'static> AnyCast<T> for T {
    fn downcast_ref(&self) -> Option<&T> { Some(self) }
    fn downcast_mut(&mut self) -> Option<&mut T> { Some(self) }
}
