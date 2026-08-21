//! AMD AGS (Adaptive Graphics Selection) integration.
//! Provides GPU detection, selection, and optimization hints.
//!
//! AMD AGS allows applications to:
//! - Enumerate and select AMD GPUs
//! - Query GPU capabilities and features
//! - Set optimization hints for drivers
//! - Enable RDNA-specific optimizations
//! - Support multi-GPU configurations

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

/// AMD AGS GPU properties
#[derive(Clone, Copy, Debug, Pod, Zeroable)]
#[repr(C)]
pub struct GpuProperties {
    /// GPU vendor
    pub vendor: GpuVendor,
    /// GPU name (null-terminated)
    pub name: [u8; 128],
    /// Vendor ID
    pub vendor_id: u32,
    /// Device ID
    pub device_id: u32,
    /// Subsystem vendor ID
    pub subsys_vendor_id: u32,
    /// Subsystem ID
    pub subsys_id: u32,
    /// Revision ID
    pub revision_id: u32,
    /// Driver model
    pub driver_model: u32,
    /// Bus type
    pub bus_type: u32,
    /// Bus number
    pub bus_number: u32,
    /// Device number
    pub device_number: u32,
    /// Function number
    pub function_number: u32,
    /// Maximum pipeline count
    pub pipeline_max: u32,
    /// Shader core count
    pub shader_cores: u32,
    /// Clock frequency (MHz)
    pub clock_mhz: u32,
    /// VRAM size (MB)
    pub vram_mb: u32,
    /// RDNA generation (0 = unknown, 2 = RDNA2, 3 = RDNA3, 4 = RDNA4)
    pub rdna_gen: u32,
    /// NPU support
    pub npu_support: bool,
    /// FSR 4 support
    pub fsr4_support: bool,
    /// XDNA NPU TOPS
    pub npu_tops: f32,
    /// Padding for alignment
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
    /// Create from Vulkan physical device
    pub fn from_vulkan(instance: &Instance, physical_device: vk::PhysicalDevice) -> Self {
        let mut props = Self::default();
        
        unsafe {
            let device_props = instance.physical_device_properties(physical_device);
            let device_mem_props = instance.physical_device_memory_properties(physical_device);
            
            // Copy device name
            let name_bytes = device_props.device_name.as_slice();
            let name_len = name_bytes.iter().position(|&b| b == 0).unwrap_or(name_bytes.len());
            props.name[..name_len].copy_from_slice(&name_bytes[..name_len]);
            
            props.vendor_id = device_props.vendor_id;
            props.device_id = device_props.device_id;
            
            // Determine vendor
            props.vendor = match props.vendor_id {
                AMD_VENDOR_ID => GpuVendor::Amd,
                INTEL_VENDOR_ID => GpuVendor::Intel,
                SAMSUNG_VENDOR_ID => GpuVendor::Samsung,
                MOORE_THREADS_VENDOR_ID => GpuVendor::MooreThreads,
                QUALCOMM_VENDOR_ID => GpuVendor::Other("Qualcomm".to_string()),
                _ => GpuVendor::Unknown,
            };
            
            // Calculate VRAM
            let total_vram = device_mem_props.memory_heaps.iter()
                .map(|heap| heap.size as u64)
                .sum::<u64>();
            props.vram_mb = (total_vram / (1024 * 1024 * 1024)) as u32;
            
            // Detect RDNA generation from device name
            let name_str = std::ffi::CStr::from_bytes_with_nul(&props.name[..name_len + 1])
                .map(|s| s.to_string_lossy().to_lowercase())
                .unwrap_or_default();
            
            props.rdna_gen = Self::detect_rdna_generation(&name_str, &props.vendor);
            
            // Detect NPU support for AMD Ryzen AI
            if props.vendor == GpuVendor::Amd {
                props.npu_support = name_str.contains("npu") || 
                                   name_str.contains("ryzen ai") ||
                                   name_str.contains("rdna 3") ||
                                   name_str.contains("rdna3");
                props.fsr4_support = name_str.contains("rdna 4") ||
                                    name_str.contains("rdna4") ||
                                    name_str.contains("9000") ||
                                    name_str.contains("7000");
                
                // Set NPU TOPS for Ryzen AI
                if props.npu_support {
                    props.npu_tops = 25.0; // RDNA 2/3 NPU baseline
                    if props.rdna_gen >= 3 {
                        props.npu_tops = 50.0; // RDNA 3+ NPU
                    }
                }
            }
            
            // Intel Arc
            if props.vendor == GpuVendor::Intel {
                props.npu_support = name_str.contains("arc") || name_str.contains("xpu");
                if props.npu_support {
                    props.npu_tops = 48.0; // Intel AI Boost
                }
            }
        }
        
        props
    }
    
    /// Detect RDNA generation from device name
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
    
    /// Check if this GPU supports ray tracing
    pub fn supports_ray_tracing(&self) -> bool {
        matches!(self.vendor, GpuVendor::Amd | GpuVendor::Intel | GpuVendor::Samsung)
    }
    
    /// Check if this GPU supports FSR 4
    pub fn supports_fsr4(&self) -> bool {
        self.fsr4_support
    }
    
    /// Check if this GPU has NPU acceleration
    pub fn has_npu(&self) -> bool {
        self.npu_support && self.npu_tops > 0.0
    }
}

/// AMD GPU selection criteria
#[derive(Clone, Copy, Debug, Default)]
pub struct GpuSelectionCriteria {
    /// Prefer AMD GPUs
    pub prefer_amd: bool,
    /// Minimum VRAM in MB
    pub min_vram_mb: u32,
    /// Minimum shader cores
    pub min_shader_cores: u32,
    /// Prefer discrete GPUs
    pub prefer_discrete: bool,
    /// Require ray tracing
    pub require_ray_tracing: bool,
    /// Require FSR 4 support
    pub require_fsr4: bool,
    /// Require NPU support
    pub require_npu: bool,
}

impl GpuSelectionCriteria {
    /// Score a GPU for selection (higher is better)
    pub fn score(&self, gpu: &GpuProperties) -> f32 {
        let mut score = 0.0f32;
        
        // Vendor preference
        if self.prefer_amd && gpu.vendor == GpuVendor::Amd {
            score += 100.0;
        }
        
        // VRAM
        score += gpu.vram_mb as f32 * 0.1;
        
        // RDNA generation bonus
        score += gpu.rdna_gen as f32 * 50.0;
        
        // Ray tracing support
        if gpu.supports_ray_tracing() {
            score += 50.0;
        }
        
        // FSR 4 support
        if gpu.supports_fsr4() {
            score += 30.0;
        }
        
        // NPU support
        if gpu.has_npu() {
            score += gpu.npu_tops * 2.0;
        }
        
        // Discrete GPU bonus
        if self.prefer_discrete && gpu.vendor != GpuVendor::Other(_) {
            score += 20.0;
        }
        
        // Penalty for failing requirements
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

/// AMD AGS GPU manager
#[derive(Debug)]
pub struct GpuManager {
    /// Selected GPU properties
    pub selected: Option<GpuProperties>,
    /// All available GPUs
    pub gpus: Vec<GpuProperties>,
    /// Selection criteria
    pub criteria: GpuSelectionCriteria,
}

impl GpuManager {
    /// Create new GPU manager and enumerate devices
    pub fn new(criteria: GpuSelectionCriteria) -> Result<Self, String> {
        // Note: Full implementation would use platform-specific GPU enumeration
        // This is a simplified version for now
        Ok(Self {
            selected: None,
            gpus: Vec::new(),
            criteria,
        })
    }
    
    /// Add a GPU to the manager
    pub fn add_gpu(&mut self, instance: &Instance, physical_device: vk::PhysicalDevice) {
        let gpu = GpuProperties::from_vulkan(instance, physical_device);
        self.gpus.push(gpu);
    }
    
    /// Select the best GPU based on criteria
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
    
    /// Get the selected GPU
    pub fn get_selected(&self) -> Option<&GpuProperties> {
        self.selected.as_ref()
    }
    
    /// Get all AMD GPUs
    pub fn get_amd_gpus(&self) -> Vec<&GpuProperties> {
        self.gpus.iter()
            .filter(|gpu| gpu.vendor == GpuVendor::Amd)
            .collect()
    }
    
    /// Check if any GPU supports NPU
    pub fn has_npu_support(&self) -> bool {
        self.gpus.iter().any(|gpu| gpu.has_npu())
    }
    
    /// Check if any GPU supports FSR 4
    pub fn has_fsr4_support(&self) -> bool {
        self.gpus.iter().any(|gpu| gpu.supports_fsr4())
    }
}

/// AMD AGS optimization hints
#[derive(Clone, Copy, Debug, Default)]
pub struct AgsHints {
    /// Enable wave32 mode (RDNA2/3)
    pub wave32_enabled: bool,
    /// Enable optimized queue family selection
    pub optimized_queues: bool,
    /// Enable sustained fast encoding
    pub sustained_encoding: bool,
    /// Enable pipeline cache control
    pub pipeline_cache: bool,
    /// Shader core optimization hint
    pub shader_core_hints: bool,
}

impl AgsHints {
    /// Create hints based on GPU properties
    pub fn from_gpu(gpu: &GpuProperties) -> Self {
        let mut hints = Self::default();
        
        // RDNA 2/3 - enable wave32
        if gpu.rdna_gen >= 2 {
            hints.wave32_enabled = true;
        }
        
        // RDNA 3/4 - enable sustained fast encoding
        if gpu.rdna_gen >= 3 {
            hints.sustained_encoding = true;
        }
        
        // Enable optimized settings for supported GPUs
        if matches!(gpu.vendor, GpuVendor::Amd | GpuVendor::Intel | GpuVendor::Samsung) {
            hints.optimized_queues = true;
            hints.pipeline_cache = true;
            hints.shader_core_hints = true;
        }
        
        hints
    }
    
    /// Get Vulkan device extension names to enable
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

/// AMD GPU information for the engine
#[derive(Debug)]
pub struct AmgInfo {
    /// GPU properties
    pub gpu: GpuProperties,
    /// AGS optimization hints
    pub hints: AgsHints,
    /// Vendor name
    pub vendor_name: String,
    /// GPU name
    pub gpu_name: String,
}

impl AmgInfo {
    /// Create from GPU properties
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
    
    /// Check if we're running on AMD hardware
    pub fn is_amd(&self) -> bool {
        matches!(self.gpu.vendor, GpuVendor::Amd)
    }
    
    /// Check if we're running on RDNA 3 or newer
    pub fn is_rdna3_or_newer(&self) -> bool {
        self.gpu.rdna_gen >= 3
    }
    
    /// Check if we're running on RDNA 4 or newer
    pub fn is_rdna4_or_newer(&self) -> bool {
        self.gpu.rdna_gen >= 4
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn test_gpu_properties_detection() {
        // Test RDNA generation detection
        assert_eq!(GpuProperties::detect_rdna_generation("radeo rx 7800 xt", &GpuVendor::Amd), 4);
        assert_eq!(GpuProperties::detect_rdna_generation("radv radeon rx 6700 xt", &GpuVendor::Amd), 3);
        assert_eq!(GpuProperties::detect_rdna_generation("radv radeon rx 5700 xt", &GpuVendor::Amd), 2);
    }
    
    #[test]
    fn test_gpu_scoring() {
        let criteria = GpuSelectionCriteria {
            prefer_amd: true,
            ..Default::default()
        };
        
        let amd_gpu = GpuProperties {
            vendor: GpuVendor::Amd,
            vendor_id: AMD_VENDOR_ID,
            rdna_gen: 3,
            vram_mb: 12288,
            ..Default::default()
        };
        
        let intel_gpu = GpuProperties {
            vendor: GpuVendor::Intel,
            vendor_id: INTEL_VENDOR_ID,
            rdna_gen: 0,
            vram_mb: 8192,
            ..Default::default()
        };
        
        assert!(criteria.score(&amd_gpu) > criteria.score(&intel_gpu));
    }
}
