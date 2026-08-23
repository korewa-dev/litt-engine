//! Shader compilation and management.
//! GLSL -> SPIR-V (Vulkan), HLSL -> DXIL (DX12).

use std::path::{Path, PathBuf};
use super::handle::{AssetHandle, AssetType, AssetState};

/// Shader stage
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum ShaderStage {
    Vertex,
    Fragment,
    Compute,
    RayGen,
    ClosestHit,
    Miss,
    Intersection,
    Callable,
    Task,
    Mesh,
}

impl std::fmt::Display for ShaderStage {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Vertex => write!(f, "Vertex"),
            Self::Fragment => write!(f, "Fragment"),
            Self::Compute => write!(f, "Compute"),
            Self::RayGen => write!(f, "RayGen"),
            Self::ClosestHit => write!(f, "ClosestHit"),
            Self::Miss => write!(f, "Miss"),
            Self::Intersection => write!(f, "Intersection"),
            Self::Callable => write!(f, "Callable"),
            Self::Task => write!(f, "Task"),
            Self::Mesh => write!(f, "Mesh"),
        }
    }
}

/// Shader source type
#[derive(Clone, Debug)]
pub enum ShaderSource {
    /// GLSL source code
    Glsl(String),
    /// HLSL source code
    Hlsl(String),
    /// Pre-compiled SPIR-V
    SpirV(Vec<u32>),
    /// Pre-compiled DXIL
    Dxil(Vec<u8>),
    /// WGSL source code
    Wgsl(String),
}

/// Compiled shader
#[derive(Debug)]
pub struct Shader {
    pub handle: AssetHandle,
    pub name: String,
    pub stage: ShaderStage,
    pub source: ShaderSource,
    pub entry_point: String,
    pub push_constant_size: u32,
    pub descriptor_sets: Vec<DescriptorSetLayout>,
    pub state: AssetState,
}

/// Descriptor set layout
#[derive(Debug, Clone)]
pub struct DescriptorSetLayout {
    pub binding: u32,
    pub ty: DescriptorType,
    pub count: u32,
    pub stage_flags: ShaderStageFlags,
}

/// Descriptor type
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum DescriptorType {
    UniformBuffer,
    SampledImage,
    StorageBuffer,
    StorageImage,
    CombinedImageSampler,
    UniformTexelBuffer,
    StorageTexelBuffer,
    InputAttachment,
    AccelerationStructure,
    Sampler,
}

/// Shader stage flags
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct ShaderStageFlags(pub u32);

impl ShaderStageFlags {
    pub const VERTEX: Self = Self(1 << 0);
    pub const FRAGMENT: Self = Self(1 << 1);
    pub const COMPUTE: Self = Self(1 << 2);
    pub const RAY_GEN: Self = Self(1 << 3);
    pub const CLOSEST_HIT: Self = Self(1 << 4);
    pub const MISS: Self = Self(1 << 5);
    pub const ALL: Self = Self(0x3F);
}

impl Shader {
    pub fn new(handle: AssetHandle, name: &str, stage: ShaderStage, source: ShaderSource) -> Self {
        Self {
            handle,
            name: name.to_string(),
            stage,
            source,
            entry_point: "main".to_string(),
            push_constant_size: 0,
            descriptor_sets: Vec::new(),
            state: AssetState::Pending,
        }
    }

    /// Compile GLSL to SPIR-V
    pub fn compile_glsl_to_spirv(glsl: &str, entry_point: &str) -> Result<Vec<u32>, String> {
        // Try to find glslc or glslangValidator
        let compiler = Self::find_glsl_compiler()?;

        let out_dir = std::env::var("OUT_DIR").unwrap_or_else(|_| "/tmp".to_string());
        let spv_path = Path::new(&out_dir).join("compiled.spv");

        std::fs::write("shader.glsl", glsl)
            .map_err(|e| format!("Failed to write shader: {}", e))?;

        let status = std::process::Command::new(&compiler)
            .args(&["shader.glsl", "-o", spv_path.to_string_lossy().as_ref(), "-O3"])
            .status()
            .map_err(|e| format!("Failed to run compiler: {}", e))?;

        if !status.success() {
            return Err(format!("GLSL compilation failed for '{}'", entry_point));
        }

        let spv_bytes = std::fs::read(&spv_path)
            .map_err(|e| format!("Failed to read SPIR-V: {}", e))?;

        // Convert bytes to u32 words
        if spv_bytes.len() % 4 != 0 {
            return Err("SPIR-V data has invalid size".to_string());
        }

        let words: Vec<u32> = spv_bytes
            .chunks_exact(4)
            .map(|chunk| u32::from_le_bytes(chunk.try_into().unwrap()))
            .collect();

        Ok(words)
    }

    /// Compile HLSL to DXIL
    pub fn compile_hlsl_to_dxil(hlsl: &str, entry_point: &str, target: &str) -> Result<Vec<u8>, String> {
        // Try to find dxcompiler.dll
        let dxc_path = std::env::var("DXC_PATH")
            .unwrap_or_else(|_| "dxcompiler.dll".to_string());

        let status = std::process::Command::new("dxc")
            .args(&[
                "-T", target,
                "-E", entry_point,
                "-O3",
                "-",
            ])
            .stdin(std::process::Stdio::piped())
            .stdout(std::process::Stdio::piped())
            .stderr(std::process::Stdio::piped())
            .spawn()
            .map_err(|e| format!("Failed to run dxcompiler: {}", e))?;

        // In a real implementation, we'd pipe the HLSL source to stdin
        // and read DXIL from stdout
        let output = status.wait_with_output()
            .map_err(|e| format!("dxcompiler failed: {}", e))?;

        if !output.status.success() {
            return Err(format!("HLSL compilation failed: {}", String::from_utf8_lossy(&output.stderr)));
        }

        Ok(output.stdout)
    }

    /// Find GLSL compiler on the system
    fn find_glsl_compiler() -> Result<PathBuf, String> {
        // Check common paths
        let candidates = ["glslc", "glslangValidator"];

        for candidate in &candidates {
            if let Some(path) = find_in_path(candidate) {
                return Ok(path);
            }
        }

        // Check VULKAN_SDK
        if let Ok(sdk) = std::env::var("VULKAN_SDK") {
            for candidate in &candidates {
                let path = Path::new(&sdk).join("bin").join(format!("{}.exe", candidate));
                if path.exists() {
                    return Ok(path);
                }
            }
        }

        Err(format!("GLSL compiler not found (tried: {:?})", candidates))
    }
}

/// Shader compiler -- manages shader compilation and caching
#[derive(Debug)]
pub struct ShaderCompiler {
    pub cache_dir: PathBuf,
    pub glsl_compiler: Option<PathBuf>,
    pub dxc_compiler: Option<PathBuf>,
}

impl Default for ShaderCompiler {
    fn default() -> Self {
        Self::new()
    }
}

impl ShaderCompiler {
    /// Create a new shader compiler
    pub fn new() -> Self {
        Self {
            cache_dir: PathBuf::from(".shader_cache"),
            glsl_compiler: Self::find_compiler("glslc").or_else(|| Self::find_compiler("glslangValidator")),
            dxc_compiler: Self::find_compiler("dxc"),
        }
    }

    /// Find a compiler on the system
    fn find_compiler(name: &str) -> Option<PathBuf> {
        find_in_path(name)
    }

    /// Compile a shader source
    pub fn compile(&self, shader: &mut Shader) -> Result<(), String> {
        shader.state = AssetState::Loading;

        match &shader.source {
            ShaderSource::Glsl(source) => {
                let spirv = Self::compile_glsl_to_spirv_inner(source, &shader.entry_point, &self.glsl_compiler)?;
                shader.source = ShaderSource::SpirV(spirv);
            }
            ShaderSource::Hlsl(source) => {
                let dxil = Self::compile_hlsl_to_dxil_inner(source, &shader.entry_point, &self.dxc_compiler)?;
                shader.source = ShaderSource::Dxil(dxil);
            }
            ShaderSource::SpirV(_) | ShaderSource::Dxil(_) => {
                // Already compiled
            }
            ShaderSource::Wgsl(_) => {
                return Err("WGSL compilation requires wgpu renderer".to_string());
            }
        }

        shader.state = AssetState::Loaded;
        Ok(())
    }

    fn compile_glsl_to_spirv_inner(source: &str, entry: &str, compiler: &Option<PathBuf>) -> Result<Vec<u32>, String> {
        // If compiler not found, return placeholder
        let compiler = compiler.as_ref()
            .ok_or("No GLSL compiler found")?;

        let out_dir = std::env::var("OUT_DIR").unwrap_or_else(|_| "/tmp".to_string());
        let spv_path = Path::new(&out_dir).join(format!("{}_{}.spv", entry, hash_str(source)));

        if spv_path.exists() {
            return Ok(Self::load_spirv(&spv_path)?);
        }

        std::fs::write("shader.glsl", source)
            .map_err(|e| format!("Failed to write shader: {}", e))?;

        let output = std::process::Command::new(compiler)
            .args(&["shader.glsl", "-o", spv_path.to_string_lossy().as_ref(), "-O3"])
            .output()
            .map_err(|e| format!("Failed to run compiler: {}", e))?;

        if !output.status.success() {
            return Err(format!("GLSL compilation failed: {}", String::from_utf8_lossy(&output.stderr)));
        }

        Self::load_spirv(&spv_path)
    }

    fn compile_hlsl_to_dxil_inner(source: &str, entry: &str, compiler: &Option<PathBuf>) -> Result<Vec<u8>, String> {
        let compiler = compiler.as_ref()
            .ok_or("No DXC compiler found. Set DXC_PATH or install DirectX Shader Compiler.")?;

        let out_dir = std::env::var("OUT_DIR").unwrap_or_else(|_| "/tmp".to_string());
        let dxil_path = Path::new(&out_dir).join(format!("{}_{}.dxil", entry, hash_str(source)));

        if dxil_path.exists() {
            return Ok(std::fs::read(&dxil_path).map_err(|e| e.to_string())?);
        }

        let mut output = std::process::Command::new(compiler)
            .args(&["-T", "ds_6_5", "-E", entry, "-O3", "-fdxil", "-"])
            .stdin(std::process::Stdio::piped())
            .stdout(std::process::Stdio::piped())
            .stderr(std::process::Stdio::piped())
            .spawn()
            .map_err(|e| format!("Failed to run dxcompiler: {}", e))?;

        let mut child = output
            .stdin
            .take()
            .ok_or("Failed to open stdin")?;
        use std::io::Write;
        child.write_all(source.as_bytes())
            .map_err(|e| format!("Failed to write shader: {}", e))?;
        drop(child);

        let output = output.wait_with_output()
            .map_err(|e| format!("dxcompiler failed: {}", e))?;

        if !output.status.success() {
            return Err(format!("HLSL compilation failed: {}", String::from_utf8_lossy(&output.stderr)));
        }

        std::fs::write(&dxil_path, &output.stdout)
            .map_err(|e| e.to_string())?;

        Ok(output.stdout)
    }

    fn load_spirv(path: &Path) -> Result<Vec<u32>, String> {
        let bytes = std::fs::read(path)
            .map_err(|e| format!("Failed to read SPIR-V: {}", e))?;

        if bytes.len() % 4 != 0 {
            return Err("SPIR-V data has invalid size".to_string());
        }

        Ok(bytes
            .chunks_exact(4)
            .map(|chunk| u32::from_le_bytes(chunk.try_into().unwrap()))
            .collect())
    }
}

fn hash_str(s: &str) -> u64 {
    use std::hash::{Hash, Hasher};
    let mut hasher = std::collections::hash_map::DefaultHasher::new();
    s.hash(&mut hasher);
    hasher.finish()
}

/// Locate an executable on PATH without external crates.
fn find_in_path(name: &str) -> Option<std::path::PathBuf> {
    let exts: &[&str] = if cfg!(windows) { &[".exe", ".bat", ".cmd", ""] } else { &[""] };
    let path_var = std::env::var_os("PATH")?;
    for dir in std::env::split_paths(&path_var) {
        for ext in exts {
            let candidate = dir.join(format!("{}{}", name, ext));
            if candidate.is_file() {
                return Some(candidate);
            }
        }
    }
    None
}
