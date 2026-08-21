//! AMD AGS (Adaptive Graphics Selection) - Custom Implementation
//!
//! NOTE: This is a custom GPU detection/selection system, NOT the official
//! AMD AGS (AMDGPU Services) library. The official AMD AGS library provides
//! power management, fan control, and performance profiling - none of which
//! are implemented here.
//!
//! What IS implemented:
//! - GPU vendor detection (AMD, Intel, Samsung, Moore Threads)
//! - RDNA generation detection (RDNA 2/3/4)
//! - NPU support detection
//! - FSR 4 capability detection
//! - GPU scoring and selection
//! - Optimization hint generation
//!
//! To use the REAL AMD AGS library, add this to Cargo.toml:
//!
//! ```toml
//! [dependencies]
//! ags = { git = "https://github.com/GPUOpen-LibrariesAndSDKs/AGS" }
//! ```
//!
//! And use it like:
//! ```rust
//! use ags::AgsContext;
//!
//! let mut context = AgsContext::new();
//! context.init();
//!
//! // Get GPU count
//! let gpu_count = context.get_adapter_count();
//!
//! // Get GPU info
//! let mut adapter_info = AgsAdapterInfo::new();
//! context.get_adapter_info(0, &mut adapter_info);
//!
//! // Get driver info
//! let mut driver_info = AgsDriverInfo::new();
//! context.get_driver_info(&mut driver_info);
//!
//! // Power management (requires admin privileges)
//! // context.set_gpu_power_profile(...);
//! ```

use ash::{vk, Device, Instance};
use bytemuck::{Pod, Zeroable};
use super::GpuVendor;

/// AMD GPU vendor ID
pub const AMD_VENDOR_ID: u32 = 0x1002;
/// Intel GPU vendor ID
pub const INTEL_VENDOR_ID: u32 = 0x8086;
/// Samsung GPU vendor ID
pub const SAMSUNG_VENDOR_ID: u32 = 0x1AE;
/// Moore Threads GPU vendor ID
pub const MOORE_THREADS_VENDOR_ID: u32 = 0x1DD;
/// Qualcomm GPU vendor ID
pub const QUALCOMM_VENDOR_ID: u32 = 0x5143;

/// GPU properties detected from Vulkan
#[derive(Clone, Copy, Debug, Pod, Zeroable)]
#[repr(C)]
pub struct GpuProperties {
    pub vendor: GpuVendor,
    pub name: [u8; 128],
    pub vendor_id: u32,
    pub device_id: u32,
    pub subsys_vendor_id: u32,
    pub subsys_id: u32,
    pub revision_id: u32,
    pub driver_model: u32,
    pub bus_type: u32,
    pub bus_number: u32,
    pub device_number: u32,
    pub function_number: u32,
    pub pipeline_max: u32,
    pub shader_cores: u32,
    pub clock_mhz: u32,
    pub vram_mb: u32,
    pub rdna_gen: u32,
    pub npu_support: bool,
    pub fsr4_support: bool,
    pub npu_tops: f32,
    pub _pad: [u32; 4],
}

impl Default for GpuProperties {
    fn default() -> Self {
        Self {
            vendor: GpuVendor::Unknown,
            name: [0u8; 128],
            vendor_id: 0,
            device_id: 0,
            subsys_vendor_id: 0,
            subsys_id: 0,
            revision_id: 0,
            driver_model: 0,
            bus_type: 0,
            bus_number: 0,
            device_number: 0,
            function_number: 0,
            pipeline_max: 0,
            shader_cores: 0,
            clock_mhz: 0,
            vram_mb: 0,
            rdna_gen: 0,
            npu_support: false,
            fsr4_support: false,
            npu_tops: 0.0,
            _pad: [0; 4],
        }
    }
}

impl GpuProperties {
    pub fn from_vulkan(instance: &Instance, physical_device: vk::PhysicalDevice) -> Self {
        let mut props = Self::default();
        
        unsafe {
            let device_props = instance.physical_device_properties(physical_device);
            let device_mem_props = instance.physical_device_memory_properties(physical_device);
            
            let name_bytes = device_props.device_name.as_slice();
            let name_len = name_bytes.iter().position(|&b| b == 0).unwrap_or(name_bytes.len());
            props.name[..name_len].copy_from_slice(&name_bytes[..name_len]);
            
            props.vendor_id = device_props.vendor_id;
            props.device_id = device_props.device_id;
            
            props.vendor = match props.vendor_id {
                AMD_VENDOR_ID => GpuVendor::Amd,
                INTEL_VENDOR_ID => GpuVendor::Intel,
                SAMSUNG_VENDOR_ID => GpuVendor::Samsung,
                MOORE_THREADS_VENDOR_ID => GpuVendor::MooreThreads,
                QUALCOMM_VENDOR_ID => GpuVendor::Other("Qualcomm".to_string()),
                _ => GpuVendor::Unknown,
            };
            
            let total_vram = device_mem_props.memory_heaps.iter()
                .map(|heap| heap.size as u64)
                .sum::<u64>();
            props.vram_mb = (total_vram / (1024 * 1024 * 1024)) as u32;
            
            let name_str = std::ffi::CStr::from_bytes_with_nul(&props.name[..name_len + 1])
                .map(|s| s.to_string_lossy().to_lowercase())
                .unwrap_or_default();
            
            props.rdna_gen = Self::detect_rdna_generation(&name_str, &props.vendor);
            
            if props.vendor == GpuVendor::Amd {
                props.npu_support = name_str.contains("npu") || 
                                   name_str.contains("ryzen ai") ||
                                   name_str.contains("rdna 3") ||
                                   name_str.contains("rdna3");
                props.fsr4_support = name_str.contains("rdna 4") ||
                                    name_str.contains("rdna4") ||
                                    name_str.contains("9000") ||
                                    name_str.contains("7000");
                
                if props.npu_support {
                    props.npu_tops = 25.0;
                    if props.rdna_gen >= 3 {
                        props.npu_tops = 50.0;
                    }
                }
            }
            
            if props.vendor == GpuVendor::Intel {
                props.npu_support = name_str.contains("arc") || name_str.contains("xpu");
                if props.npu_support {
                    props.npu_tops = 48.0;
                }
            }
        }
        
        props
    }
    
    fn detect_rdna_generation(name: &str, vendor: &GpuVendor) -> u32 {
        if *vendor != GpuVendor::Amd {
            return 0;
        }
        
        if name.contains("rdna 4") || name.contains("rdna4") ||
           name.contains("7000") || name.contains("9000") {
            return 4;
        }
        if name.contains("rdna 3") || name.contains("rdna3") ||
           name.contains("6000") {
            return 3;
        }
        if name.contains("rdna 2") || name.contains("rdna2") ||
           name.contains("5000") {
            return 2;
        }
        0
    }
    
    pub fn supports_ray_tracing(&self) -> bool {
        matches!(self.vendor, GpuVendor::Amd | GpuVendor::Intel | GpuVendor::Samsung)
    }
    
    pub fn supports_fsr4(&self) -> bool {
        self.fsr4_support
    }
    
    pub fn has_npu(&self) -> bool {
        self.npu_support && self.npu_tops > 0.0
    }
}

#[derive(Clone, Copy, Debug, Default)]
pub struct GpuSelectionCriteria {
    pub prefer_amd: bool,
    pub min_vram_mb: u32,
    pub min_shader_cores: u32,
    pub prefer_discrete: bool,
    pub require_ray_tracing: bool,
    pub require_fsr4: bool,
    pub require_npu: bool,
}

impl GpuSelectionCriteria {
    pub fn score(&self, gpu: &GpuProperties) -> f32 {
        let mut score = 0.0f32;
        
        if self.prefer_amd && gpu.vendor == GpuVendor::Amd {
            score += 100.0;
        }
        
        score += gpu.vram_mb as f32 * 0.1;
        score += gpu.rdna_gen as f32 * 50.0;
        
        if gpu.supports_ray_tracing() {
            score += 50.0;
        }
        
        if gpu.supports_fsr4() {
            score += 30.0;
        }
        
        if gpu.has_npu() {
            score += gpu.npu_tops * 2.0;
        }
        
        if self.require_ray_tracing && !gpu.supports_ray_tracing() {
            score -= 1000.0;
        }
        if self.require_fsr4 && !gpu.supports_fsr4() {
            score -= 1000.0;
        }
        if self.require_npu && !gpu.has_npu() {
            score -= 1000.0;
        }
        if gpu.vram_mb < self.min_vram_mb {
            score -= 1000.0;
        }
        
        score
    }
}

#[derive(Debug)]
pub struct GpuManager {
    pub selected: Option<GpuProperties>,
    pub gpus: Vec<GpuProperties>,
    pub criteria: GpuSelectionCriteria,
}

impl GpuManager {
    pub fn new(criteria: GpuSelectionCriteria) -> Self {
        Self {
            selected: None,
            gpus: Vec::new(),
            criteria,
        }
    }
    
    pub fn add_gpu(&mut self, instance: &Instance, physical_device: vk::PhysicalDevice) {
        let gpu = GpuProperties::from_vulkan(instance, physical_device);
        self.gpus.push(gpu);
    }
    
    pub fn select_best(&mut self) -> Result<&GpuProperties, String> {
        if self.gpus.is_empty() {
            return Err("No GPUs available".to_string());
        }
        
        let best_idx = self.gpus.iter()
            .enumerate()
            .max_by(|(_, a), (_, b)| {
                self.criteria.score(a).partial_cmp(&self.criteria.score(b)).unwrap()
            })
            .map(|(i, _)| i)
            .unwrap();
        
        self.selected = Some(self.gpus[best_idx]);
        Ok(&self.gpus[best_idx])
    }
    
    pub fn get_selected(&self) -> Option<&GpuProperties> {
        self.selected.as_ref()
    }
    
    pub fn get_amd_gpus(&self) -> Vec<&GpuProperties> {
        self.gpus.iter()
            .filter(|gpu| gpu.vendor == GpuVendor::Amd)
            .collect()
    }
    
    pub fn has_npu_support(&self) -> bool {
        self.gpus.iter().any(|gpu| gpu.has_npu())
    }
    
    pub fn has_fsr4_support(&self) -> bool {
        self.gpus.iter().any(|gpu| gpu.supports_fsr4())
    }
}

#[derive(Clone, Copy, Debug, Default)]
pub struct AgsHints {
    pub wave32_enabled: bool,
    pub sustained_encoding: bool,
    pub pipeline_cache: bool,
    pub shader_core_hints: bool,
}

impl AgsHints {
    pub fn from_gpu(gpu: &GpuProperties) -> Self {
        let mut hints = Self::default();
        
        if gpu.rdna_gen >= 2 {
            hints.wave32_enabled = true;
        }
        
        if gpu.rdna_gen >= 3 {
            hints.sustained_encoding = true;
        }
        
        if matches!(gpu.vendor, GpuVendor::Amd | GpuVendor::Intel | GpuVendor::Samsung) {
            hints.optimized_queues = true;
            hints.pipeline_cache = true;
            hints.shader_core_hints = true;
        }
        
        hints
    }
    
    pub fn get_extensions(&self) -> Vec<&str> {
        let mut exts = Vec::new();
        
        if self.optimized_queues {
            exts.push("VK_EXT_queue_family_foreign");
        }
        
        if self.sustained_encoding {
            exts.push("VK_KHR_sustained_fast_encoding");
        }
        
        if self.pipeline_cache {
            exts.push("VK_EXT_pipeline_creation_cache_control");
        }
        
        if self.shader_core_hints {
            exts.push("VK_AMD_shader_core_properties");
            exts.push("VK_AMD_shader_info");
        }
        
        exts
    }
}

#[derive(Debug)]
pub struct AmgInfo {
    pub gpu: GpuProperties,
    pub hints: AgsHints,
    pub vendor_name: String,
    pub gpu_name: String,
}

impl AmgInfo {
    pub fn from_gpu(gpu: &GpuProperties) -> Self {
        let hints = AgsHints::from_gpu(gpu);
        let vendor_name = match gpu.vendor {
            GpuVendor::Amd => "AMD".to_string(),
            GpuVendor::Intel => "Intel".to_string(),
            GpuVendor::Samsung => "Samsung".to_string(),
            GpuVendor::MooreThreads => "Moore Threads".to_string(),
            GpuVendor::Other(name) => name,
            _ => "Unknown".to_string(),
        };
        
        let gpu_name = std::ffi::CStr::from_bytes_with_nul(&gpu.name)
            .map(|s| s.to_string_lossy().to_string())
            .unwrap_or_default();
        
        Self {
            gpu: *gpu,
            hints,
            vendor_name,
            gpu_name,
        }
    }
    
    pub fn is_amd(&self) -> bool {
        matches!(self.gpu.vendor, GpuVendor::Amd)
    }
    
    pub fn is_rdna3_or_newer(&self) -> bool {
        self.gpu.rdna_gen >= 3
    }
    
    pub fn is_rdna4_or_newer(&self) -> bool {
        self.gpu.rdna_gen >= 4
    }
}
