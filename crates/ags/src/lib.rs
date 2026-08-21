//! AMD AGS (AMDGPU Services) Rust Bindings
//!
//! Provides access to AMD GPU power management, performance profiling,
//! fan control, and optimization features.
//!
//! Requires: amd_ags_x64.dll (Windows) or libamd_ags.so (Linux)

#![allow(non_camel_case_types)]
#![allow(non_snake_case)]

use libloading::{Library, Symbol};
use std::ffi::{CStr, CString};
use std::os::raw::c_void;

// =============================================================================
// C Types (matching AMD AGS C API)
// =============================================================================

pub type AGSBoolean = i32;
pub type AGSInt = i32;
pub type AGSUInt = u32;
pub type AGSFloat = f32;
pub type AGSDouble = f64;

// =============================================================================
// AGS Result Codes
// =============================================================================

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
#[repr(i32)]
pub enum AGSResult {
    AGS_SUCCESS = 0,
    AGS_ERROR_FAILED = 1,
    AGS_ERROR_INVALID_POINTER = 2,
    AGS_ERROR_INVALID_VERSION = 3,
    AGS_ERROR_SIZE_MISMATCH = 4,
    AGS_ERROR_UNSUPPORTED = 5,
    AGS_ERROR_UNAUTHORIZED = 6,
}

impl std::fmt::Display for AGSResult {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::AGS_SUCCESS => write!(f, "Success"),
            Self::AGS_ERROR_FAILED => write!(f, "Failed"),
            Self::AGS_ERROR_INVALID_POINTER => write!(f, "Invalid pointer"),
            Self::AGS_ERROR_INVALID_VERSION => write!(f, "Invalid version"),
            Self::AGS_ERROR_SIZE_MISMATCH => write!(f, "Size mismatch"),
            Self::AGS_ERROR_UNSUPPORTED => write!(f, "Unsupported"),
            Self::AGS_ERROR_UNAUTHORIZED => write!(f, "Unauthorized (admin required)"),
        }
    }
}

impl AGSResult {
    pub fn is_success(&self) -> bool {
        *self == Self::AGS_SUCCESS
    }
}

// =============================================================================
// AGS Adapter Info
// =============================================================================

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AGSAdapterInfo {
    pub AdapterName: [u8; 64],
    pub VendorName: [u8; 64],
    pub DeviceID: AGSUInt,
    pub SubSysID: AGSUInt,
    pub Revision: AGSUInt,
    pub LUID: u64,
    pub Domain: AGSInt,
    pub Bus: AGSInt,
    pub Device: AGSInt,
    pub Function: AGSInt,
    pub DRMNode: AGSInt,
    pub PMMethod: AGSInt,
    pub Flags: AGSUInt,
    pub VBIOSVersion: [u8; 64],
    pub DriverVersion: u64,
    pub VendorDriverVersion: u64,
}

impl Default for AGSAdapterInfo {
    fn default() -> Self {
        Self {
            AdapterName: [0u8; 64],
            VendorName: [0u8; 64],
            DeviceID: 0,
            SubSysID: 0,
            Revision: 0,
            LUID: 0,
            Domain: -1,
            Bus: -1,
            Device: -1,
            Function: -1,
            DRMNode: -1,
            PMMethod: -1,
            Flags: 0,
            VBIOSVersion: [0u8; 64],
            DriverVersion: 0,
            VendorDriverVersion: 0,
        }
    }
}

impl AGSAdapterInfo {
    pub fn new() -> Self { Self::default() }

    pub fn adapter_name(&self) -> &str {
        CStr::from_bytes_until_nul(&self.AdapterName)
            .map(|s| s.to_string_lossy().as_ref())
            .unwrap_or("")
    }

    pub fn vendor_name(&self) -> &str {
        CStr::from_bytes_until_nul(&self.VendorName)
            .map(|s| s.to_string_lossy().as_ref())
            .unwrap_or("")
    }

    pub fn vbios_version(&self) -> &str {
        CStr::from_bytes_until_nul(&self.VBIOSVersion)
            .map(|s| s.to_string_lossy().as_ref())
            .unwrap_or("")
    }
}

// =============================================================================
// AGS Driver Info
// =============================================================================

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AGSDriverInfo {
    pub DriverVersion: u64,
    pub VendorDriverVersion: u64,
    pub WDMVersion: u64,
    pub UMDFVersion: u64,
    pub DCHUPVersion: u64,
    pub INFVersion: u64,
    pub InstallerVersion: u64,
    pub INFDate: u64,
    pub CatalogFile: [u8; 64],
    pub INFPath: [u8; 256],
    pub INFSection: [u8; 128],
}

impl Default for AGSDriverInfo {
    fn default() -> Self {
        Self {
            DriverVersion: 0,
            VendorDriverVersion: 0,
            WDMVersion: 0,
            UMDFVersion: 0,
            DCHUPVersion: 0,
            INFVersion: 0,
            InstallerVersion: 0,
            INFDate: 0,
            CatalogFile: [0u8; 64],
            INFPath: [0u8; 256],
            INFSection: [0u8; 128],
        }
    }
}

// =============================================================================
// AGS Power State Types
// =============================================================================

#[repr(i32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum AGSPowerStateType {
    AGS_POWER_STATE_TYPE_DEFAULT = 0,
    AGS_POWER_STATE_TYPE_FULLSCREEN_3D = 1,
    AGS_POWER_STATE_TYPE_VIDEO = 2,
    AGS_POWER_STATE_TYPE_VR = 3,
    AGS_POWER_STATE_TYPE_POWER_SAVING = 4,
    AGS_POWER_STATE_TYPE_UTILIZATION_3D = 5,
    AGS_POWER_STATE_TYPE_UTILIZATION_VIDEO_DECODE = 6,
    AGS_POWER_STATE_TYPE_UTILIZATION_COMPUTE = 7,
    AGS_POWER_STATE_TYPE_UTILIZATION_OPENGL = 8,
}

// =============================================================================
// AGS Power Profile
// =============================================================================

#[repr(i32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum AGSPowerProfile {
    AGS_POWER_PROFILE_DEFAULT = 0,
    AGS_POWER_PROFILE_FORCE_HIGH = 1,
    AGS_POWER_PROFILE_LOW = 2,
    AGS_POWER_PROFILE_AUTO_MIN_LOW = 3,
}

// =============================================================================
// AGS Performance Level
// =============================================================================

#[repr(i32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum AGSPerformanceLevel {
    AGS_PERFORMANCE_LEVEL_DEFAULT = 0,
    AGS_PERFORMANCE_LEVEL_HIGH = 1,
    AGS_PERFORMANCE_LEVEL_LOW = 2,
}

// =============================================================================
// AGS Fan Info
// =============================================================================

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AGSFanInfo {
    pub Flags: AGSUInt,
    pub CurrentSpeed: AGSInt,
    pub TargetTemperature: AGSInt,
    pub CurrentFrequency: AGSInt,
}

// =============================================================================
// AGS Power Info
// =============================================================================

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AGSPowerInfo {
    pub AveragePower: AGSFloat,
    pub LastSamplePower: AGSFloat,
    pub CurrentPowerLimit: AGSFloat,
    pub MinPowerLimit: AGSFloat,
    pub MaxPowerLimit: AGSFloat,
    pub StepSize: AGSFloat,
    pub Threshold: [AGSFloat; 2],
}

// =============================================================================
// AGS Thermals
// =============================================================================

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AGSThermals {
    pub Flags: AGSUInt,
    pub CurrentTemperature: AGSFloat,
    pub TargetTemperature: AGSFloat,
    pub ThrottlingTemperature: AGSFloat,
    pub CriticalTemperature: AGSFloat,
    pub SensorCount: AGSInt,
}

// =============================================================================
// AGS Utilization
// =============================================================================

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AGSUtilization {
    pub GPU: AGSFloat,
    pub Memory: AGSFloat,
    pub PCIe_TX: AGSFloat,
    pub PCIe_RX: AGSFloat,
    pub VisualProcessor_0: AGSFloat,
    pub VisualProcessor_1: AGSFloat,
}

// =============================================================================
// AGS Context (main interface)
// =============================================================================

pub struct AGSContext {
    lib: Library,
    pub version: (u32, u32, u32, u32),
}

type AGSInitFn = unsafe extern "C" fn(*mut AGSContext) -> AGSResult;
type AGSGetAdapterCountFn = unsafe extern "C" fn(*const AGSContext) -> AGSInt;
type AGSGetAdapterInfoFn = unsafe extern "C" fn(*const AGSContext, AGSInt, *mut AGSAdapterInfo) -> AGSResult;
type AGSGetDriverInfoFn = unsafe extern "C" fn(*const AGSContext, *mut AGSDriverInfo) -> AGSResult;
type AGSGetPowerStateTypeFn = unsafe extern "C" fn(*const AGSContext, AGSInt, *mut AGSPowerStateType) -> AGSResult;
type AGSSetPowerProfileFn = unsafe extern "C" fn(*const AGSContext, AGSInt, AGSPowerProfile) -> AGSResult;
type AGSGetPowerProfileFn = unsafe extern "C" fn(*const AGSContext, AGSInt, *mut AGSPowerProfile) -> AGSResult;
type AGSGetFanInfoFn = unsafe extern "C" fn(*const AGSContext, AGSInt, *mut AGSFanInfo) -> AGSResult;
type AGSSetFanSpeedFn = unsafe extern "C" fn(*const AGSContext, AGSInt, AGSInt) -> AGSResult;
type AGSGetPowerInfoFn = unsafe extern "C" fn(*const AGSContext, AGSInt, *mut AGSPowerInfo) -> AGSResult;
type AGSGetThermalsFn = unsafe extern "C" fn(*const AGSContext, AGSInt, *mut AGSThermals) -> AGSResult;
type AGSGetUtilizationFn = unsafe extern "C" fn(*const AGSContext, AGSInt, *mut AGSUtilization) -> AGSResult;

impl AGSContext {
    /// Create a new AMD AGS context
    ///
    /// Loads the AMD AGS library (amd_ags_x64.dll on Windows, libamd_ags.so on Linux)
    pub fn new() -> Result<Self, String> {
        unsafe {
            #[cfg(target_os = "windows")]
            {
                // Try to load from common AMD driver locations
                let paths = [
                    "amd_ags_x64.dll",
                    "C:\\Windows\\System32\\amd_ags_x64.dll",
                    "C:\\Windows\\SysWOW64\\amd_ags_x64.dll",
                ];

                let mut lib: Option<Library> = None;
                for path in &paths {
                    if let Ok(l) = Library::new(path) {
                        lib = Some(l);
                        break;
                    }
                }

                let lib = lib.ok_or_else(|| "Failed to load AMD AGS library".to_string())?;

                let init: Symbol<AGSInitFn> = lib.get(b"AGSInit")
                    .map_err(|e| format!("Failed to get AGSInit: {}", e))?;

                let mut context: AGSContext = AGSContext { lib, version: (0, 0, 0, 0) };

                let result = init(&mut context as *mut _);
                if result != AGSResult::AGS_SUCCESS {
                    return Err(format!("AGSInit failed: {}", result));
                }

                Ok(context)
            }

            #[cfg(target_os = "linux")]
            {
                let lib = Library::new("libamd_ags.so")
                    .or_else(|_| Library::new("libamd_ags.so.1"))
                    .map_err(|e| format!("Failed to load libamd_ags: {}", e))?;

                let init: Symbol<AGSInitFn> = lib.get(b"AGSInit")
                    .map_err(|e| format!("Failed to get AGSInit: {}", e))?;

                let mut context: AGSContext = AGSContext { lib, version: (0, 0, 0, 0) };

                let result = init(&mut context as *mut _);
                if result != AGSResult::AGS_SUCCESS {
                    return Err(format!("AGSInit failed: {}", result));
                }

                Ok(context)
            }

            #[cfg(not(any(target_os = "windows", target_os = "linux")))]
            {
                Err("AMD AGS not supported on this platform".to_string())
            }
        }
    }

    /// Get the number of AMD GPUs
    pub fn adapter_count(&self) -> i32 {
        unsafe {
            type Fn = unsafe extern "C" fn(*const AGSContext) -> AGSInt;
            let get_count: Symbol<Fn> = self.lib.get(b"AGSGetAdapterCount")
                .unwrap_or_else(|_| std::mem::transmute(0usize));
            get_count(self)
        }
    }

    /// Get adapter info for a GPU
    pub fn get_adapter_info(&self, index: i32) -> Result<AGSAdapterInfo, AGSResult> {
        let mut info = AGSAdapterInfo::new();
        unsafe {
            type Fn = unsafe extern "C" fn(*const AGSContext, AGSInt, *mut AGSAdapterInfo) -> AGSResult;
            let get_info: Symbol<Fn> = self.lib.get(b"AGSGetAdapterInfo")
                .unwrap_or_else(|_| std::mem::transmute(0usize));
            let result = get_info(self, index, &mut info);
            if result.is_success() { Ok(info) } else { Err(result) }
        }
    }

    /// Get driver information
    pub fn get_driver_info(&self) -> Result<AGSDriverInfo, AGSResult> {
        let mut info = AGSDriverInfo::default();
        unsafe {
            type Fn = unsafe extern "C" fn(*const AGSContext, *mut AGSDriverInfo) -> AGSResult;
            let get_info: Symbol<Fn> = self.lib.get(b"AGSGetDriverInfo")
                .unwrap_or_else(|_| std::mem::transmute(0usize));
            let result = get_info(self, &mut info);
            if result.is_success() { Ok(info) } else { Err(result) }
        }
    }

    /// Get current power state type
    pub fn get_power_state(&self, adapter_index: i32) -> Result<AGSPowerStateType, AGSResult> {
        let mut state = AGSPowerStateType::AGS_POWER_STATE_TYPE_DEFAULT;
        unsafe {
            type Fn = unsafe extern "C" fn(*const AGSContext, AGSInt, *mut AGSPowerStateType) -> AGSResult;
            let get_state: Symbol<Fn> = self.lib.get(b"AGSGetPowerStateType")
                .unwrap_or_else(|_| std::mem::transmute(0usize));
            let result = get_state(self, adapter_index, &mut state);
            if result.is_success() { Ok(state) } else { Err(result) }
        }
    }

    /// Set power profile for a GPU
    ///
    /// Requires administrator privileges on Windows
    pub fn set_power_profile(&self, adapter_index: i32, profile: AGSPowerProfile) -> Result<(), AGSResult> {
        unsafe {
            type Fn = unsafe extern "C" fn(*const AGSContext, AGSInt, AGSPowerProfile) -> AGSResult;
            let set_profile: Symbol<Fn> = self.lib.get(b"AGSSetPowerProfile")
                .unwrap_or_else(|_| std::mem::transmute(0usize));
            let result = set_profile(self, adapter_index, profile);
            if result.is_success() { Ok(()) } else { Err(result) }
        }
    }

    /// Get current power profile
    pub fn get_power_profile(&self, adapter_index: i32) -> Result<AGSPowerProfile, AGSResult> {
        let mut profile = AGSPowerProfile::AGS_POWER_PROFILE_DEFAULT;
        unsafe {
            type Fn = unsafe extern "C" fn(*const AGSContext, AGSInt, *mut AGSPowerProfile) -> AGSResult;
            let get_profile: Symbol<Fn> = self.lib.get(b"AGSGetPowerProfile")
                .unwrap_or_else(|_| std::mem::transmute(0usize));
            let result = get_profile(self, adapter_index, &mut profile);
            if result.is_success() { Ok(profile) } else { Err(result) }
        }
    }

    /// Get fan information
    pub fn get_fan_info(&self, adapter_index: i32) -> Result<AGSFanInfo, AGSResult> {
        let mut fan = AGSFanInfo::default();
        unsafe {
            type Fn = unsafe extern "C" fn(*const AGSContext, AGSInt, *mut AGSFanInfo) -> AGSResult;
            let get_fan: Symbol<Fn> = self.lib.get(b"AGSGetFanInfo")
                .unwrap_or_else(|_| std::mem::transmute(0usize));
            let result = get_fan(self, adapter_index, &mut fan);
            if result.is_success() { Ok(fan) } else { Err(result) }
        }
    }

    /// Set fan speed (requires admin privileges)
    ///
    /// speed: 0-100 (percentage)
    pub fn set_fan_speed(&self, adapter_index: i32, speed: i32) -> Result<(), AGSResult> {
        unsafe {
            type Fn = unsafe extern "C" fn(*const AGSContext, AGSInt, AGSInt) -> AGSResult;
            let set_fan: Symbol<Fn> = self.lib.get(b"AGSSetFanSpeed")
                .unwrap_or_else(|_| std::mem::transmute(0usize));
            let result = set_fan(self, adapter_index, speed);
            if result.is_success() { Ok(()) } else { Err(result) }
        }
    }

    /// Get power information (power draw, limits, etc.)
    pub fn get_power_info(&self, adapter_index: i32) -> Result<AGSPowerInfo, AGSResult> {
        let mut power = AGSPowerInfo::default();
        unsafe {
            type Fn = unsafe extern "C" fn(*const AGSContext, AGSInt, *mut AGSPowerInfo) -> AGSResult;
            let get_power: Symbol<Fn> = self.lib.get(b"AGSGetPowerInfo")
                .unwrap_or_else(|_| std::mem::transmute(0usize));
            let result = get_power(self, adapter_index, &mut power);
            if result.is_success() { Ok(power) } else { Err(result) }
        }
    }

    /// Get thermal information
    pub fn get_thermals(&self, adapter_index: i32) -> Result<AGSThermals, AGSResult> {
        let mut thermal = AGSThermals::default();
        unsafe {
            type Fn = unsafe extern "C" fn(*const AGSContext, AGSInt, *mut AGSThermals) -> AGSResult;
            let get_thermal: Symbol<Fn> = self.lib.get(b"AGSGetThermals")
                .unwrap_or_else(|_| std::mem::transmute(0usize));
            let result = get_thermal(self, adapter_index, &mut thermal);
            if result.is_success() { Ok(thermal) } else { Err(result) }
        }
    }

    /// Get GPU utilization
    pub fn get_utilization(&self, adapter_index: i32) -> Result<AGSUtilization, AGSResult> {
        let mut util = AGSUtilization::default();
        unsafe {
            type Fn = unsafe extern "C" fn(*const AGSContext, AGSInt, *mut AGSUtilization) -> AGSResult;
            let get_util: Symbol<Fn> = self.lib.get(b"AGSGetUtilization")
                .unwrap_or_else(|_| std::mem::transmute(0usize));
            let result = get_util(self, adapter_index, &mut util);
            if result.is_success() { Ok(util) } else { Err(result) }
        }
    }

    /// Set performance level (requires admin privileges)
    ///
    /// HIGH = maximum performance, LOW = power saving
    pub fn set_performance_level(&self, adapter_index: i32, level: AGSPerformanceLevel) -> Result<(), AGSResult> {
        unsafe {
            type Fn = unsafe extern "C" fn(*const AGSContext, AGSInt, AGSPerformanceLevel) -> AGSResult;
            let set_level: Symbol<Fn> = self.lib.get(b"AGSSetPerformanceLevel")
                .unwrap_or_else(|_| std::mem::transmute(0usize));
            let result = set_level(self, adapter_index, level);
            if result.is_success() { Ok(()) } else { Err(result) }
        }
    }

    /// Get current performance level
    pub fn get_performance_level(&self, adapter_index: i32) -> Result<AGSPerformanceLevel, AGSResult> {
        let mut level = AGSPerformanceLevel::AGS_PERFORMANCE_LEVEL_DEFAULT;
        unsafe {
            type Fn = unsafe extern "C" fn(*const AGSContext, AGSInt, *mut AGSPerformanceLevel) -> AGSResult;
            let get_level: Symbol<Fn> = self.lib.get(b"AGSGetPerformanceLevel")
                .unwrap_or_else(|_| std::mem::transmute(0usize));
            let result = get_level(self, adapter_index, &mut level);
            if result.is_success() { Ok(level) } else { Err(result) }
        }
    }

    /// Reset power profile to default
    pub fn reset_power_profile(&self, adapter_index: i32) -> Result<(), AGSResult> {
        self.set_power_profile(adapter_index, AGSPowerProfile::AGS_POWER_PROFILE_DEFAULT)
    }
}

impl Drop for AGSContext {
    fn drop(&mut self) {
        unsafe {
            type Fn = unsafe extern "C" fn(*mut AGSContext) -> AGSResult;
            let shutdown: Symbol<Fn> = self.lib.get(b"AGSShutdown")
                .unwrap_or_else(|_| std::mem::transmute(0usize));
            shutdown(self);
        }
    }
}

// =============================================================================
// Convenience API
// =============================================================================

/// Check if AMD AGS is available
pub fn is_available() -> bool {
    AGSContext::new().is_ok()
}

/// Get a summary of all AMD GPUs
pub fn get_gpu_summary() -> Result<Vec<AGSAdapterInfo>, String> {
    let context = AGSContext::new()?;
    let count = context.adapter_count();
    
    let mut gpus = Vec::new();
    for i in 0..count {
        match context.get_adapter_info(i) {
            Ok(info) => gpus.push(info),
            Err(e) => eprintln!("Failed to get adapter info for GPU {}: {}", i, e),
        }
    }
    
    Ok(gpus)
}

/// Optimize GPU for performance
pub fn optimize_for_performance(adapter_index: i32) -> Result<(), String> {
    let context = AGSContext::new()?;
    
    // Set maximum performance profile
    context.set_power_profile(adapter_index, AGSPowerProfile::AGS_POWER_PROFILE_FORCE_HIGH)?;
    
    // Set high performance level
    context.set_performance_level(adapter_index, AGSPerformanceLevel::AGS_PERFORMANCE_LEVEL_HIGH)?;
    
    Ok(())
}

/// Optimize GPU for power saving
pub fn optimize_for_power_saving(adapter_index: i32) -> Result<(), String> {
    let context = AGSContext::new()?;
    
    // Set power saving profile
    context.set_power_profile(adapter_index, AGSPowerProfile::AGS_POWER_PROFILE_LOW)?;
    
    // Set low performance level
    context.set_performance_level(adapter_index, AGSPerformanceLevel::AGS_PERFORMANCE_LEVEL_LOW)?;
    
    Ok(())
}

/// Get current power draw and temperature
pub fn get_power_and_thermal(adapter_index: i32) -> Result<(AGSPowerInfo, AGSThermals), String> {
    let context = AGSContext::new()?;
    
    let power = context.get_power_info(adapter_index)?;
    let thermal = context.get_thermals(adapter_index)?;
    
    Ok((power, thermal))
}

/// Get GPU utilization stats
pub fn get_gpu_stats(adapter_index: i32) -> Result<AGSUtilization, String> {
    let context = AGSContext::new()?;
    context.get_utilization(adapter_index).map_err(|e| e.to_string())
}
