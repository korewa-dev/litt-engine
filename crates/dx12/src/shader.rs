//! DX12 Shader Compilation using DirectXShaderCompiler (DXC)
//!
//! Uses the official Microsoft DirectXShaderCompiler to compile HLSL to DXIL.
//! Requires dxcompiler.dll in PATH or specified via DXC_PATH environment variable.

use super::*;
use std::ffi::{CStr, CString};
use std::path::PathBuf;

/// Compiled shader bytecode
#[derive(Debug)]
pub struct ShaderBytecode {
    pub data: Vec<u8>,
    pub size: usize,
}

/// Shader compilation result
#[derive(Debug)]
pub struct CompiledShader {
    pub vs: Option<ShaderBytecode>,
    pub ps: Option<ShaderBytecode>,
    pub gs: Option<ShaderBytecode>,
    pub hs: Option<ShaderBytecode>,
    pub ds: Option<ShaderBytecode>,
    pub cs: Option<ShaderBytecode>,
    pub library: Option<ShaderBytecode>,
    pub errors: Option<String>,
    pub warnings: Option<String>,
}

/// DXC compiler interface
#[derive(Debug)]
pub struct DxcCompiler {
    lib: Option<libloading::Library>,
    compile_fn: Option<libloading::Symbol<'static, CompileFn>>,
    create_instance_fn: Option<libloading::Symbol<'static, CreateInstanceFn>>,
}

type CompileFn = unsafe extern "C" fn(
    *const u16,  // shader path
    *const u16,  // entry point
    *const u16,  // target
    *const u16,  // defines
    *mut *mut winapi::um::d3dcommon::IDxcBlob,
    *mut *mut winapi::um::d3dcommon::IDxcBlob,
) -> i32;

type CreateInstanceFn = unsafe extern "C" fn(
    *mut winapi::shared::guiddef::REFIID,
    *mut *mut winapi::um::d3dcommon::IDxcLibrary,
) -> i32;

/// DXC error
#[derive(Debug)]
pub enum DxcError {
    LibraryNotFound(String),
    CompilationFailed(String),
    InvalidArgument(String),
}

impl std::fmt::Display for DxcError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::LibraryNotFound(m) => write!(f, "DXC library not found: {}", m),
            Self::CompilationFailed(m) => write!(f, "Compilation failed: {}", m),
            Self::InvalidArgument(m) => write!(f, "Invalid argument: {}", m),
        }
    }
}

impl std::error::Error for DxcError {}

impl DxcCompiler {
    /// Create a new DXC compiler instance
    ///
    /// Searches for dxcompiler.dll in:
    /// 1. DXC_PATH environment variable
    /// 2. Current directory
    /// 3. System PATH
    pub fn new() -> Result<Self, DxcError> {
        #[cfg(target_os = "windows")]
        {
            let paths = Self::find_dxc_paths();
            
            for path in &paths {
                if let Ok(lib) = unsafe { libloading::Library::new(path) } {
                    let compile = unsafe { lib.get::<CompileFn>(b"Compile") };
                    let create = unsafe { lib.get::<CreateInstanceFn>(b"CreateInstance") };
                    
                    if compile.is_ok() && create.is_ok() {
                        return Ok(Self {
                            lib: Some(lib),
                            compile_fn: compile.ok(),
                            create_instance_fn: create.ok(),
                        });
                    }
                }
            }
            
            Err(DxcError::LibraryNotFound(
                "dxcompiler.dll not found. Set DXC_PATH or add to system PATH.".to_string()
            ))
        }
        
        #[cfg(not(target_os = "windows"))]
        {
            Err(DxcError::LibraryNotFound(
                "DXC is only available on Windows".to_string()
            ))
        }
    }
    
    fn find_dxc_paths() -> Vec<PathBuf> {
        let mut paths = Vec::new();
        
        // Check environment variable
        if let Ok(path) = std::env::var("DXC_PATH") {
            paths.push(PathBuf::from(path));
        }
        
        // Common installation locations
        let common_paths = [
            "C:\\Program Files (x86)\\Microsoft DirectX SDK (June 2010)\\Utilities\\bin\\x64\\dxcompiler.dll",
            "C:\\Windows\\System32\\dxcompiler.dll",
            ".\\dxcompiler.dll",
            "..\\dxcompiler.dll",
        ];
        
        for p in &common_paths {
            let path = PathBuf::from(p);
            if path.exists() {
                paths.push(path);
            }
        }
        
        paths
    }
    
    /// Compile HLSL source to DXIL bytecode
    pub fn compile(
        &self,
        source: &str,
        entry_point: &str,
        target: &str,
    ) -> Result<CompiledShader, DxcError> {
        #[cfg(target_os = "windows")]
        {
            if let (Some(compile), Some(create)) = (&self.compile_fn, &self.create_instance_fn) {
                unsafe {
                    // Create DXC library instance
                    let mut library: *mut winapi::um::d3dcommon::IDxcLibrary = std::ptr::null_mut();
                    let iid_library: winapi::shared::guiddef::GUID = winapi::um::d3dcommon::IID_IDxcLibrary;
                    
                    let hr = create(
                        &iid_library as *const _ as *mut _,
                        &mut library as *mut _ as *mut _,
                    );
                    
                    if winapi::shared::winerror::FAILED(hr) {
                        return Err(DxcError::CompilationFailed(
                            "Failed to create DXC library".to_string()
                        ));
                    }
                    
                    // Create source blob
                    let source_cstr = CString::new(source).map_err(|_| {
                        DxcError::InvalidArgument("Source contains null bytes".to_string())
                    })?;
                    
                    let mut source_blob: *mut winapi::um::d3dcommon::IDxcBlob = std::ptr::null_mut();
                    let hr = (*library).CreateBlob(
                        source_cstr.as_ptr() as *const i8,
                        source.len() as u32,
                        &mut source_blob as *mut _ as *mut _,
                    );
                    
                    if winapi::shared::winerror::FAILED(hr) || source_blob.is_null() {
                        return Err(DxcError::CompilationFailed(
                            "Failed to create source blob".to_string()
                        ));
                    }
                    
                    // Compile
                    let entry_cstr = CString::new(entry_point).unwrap();
                    let target_cstr = CString::new(target).unwrap();
                    
                    let mut result_blob: *mut winapi::um::d3dcommon::IDxcBlob = std::ptr::null_mut();
                    let mut error_blob: *mut winapi::um::d3dcommon::IDxcBlob = std::ptr::null_mut();
                    
                    let hr = compile(
                        std::ptr::null(), // path (unused for source blob)
                        entry_cstr.as_ptr(),
                        target_cstr.as_ptr(),
                        std::ptr::null(), // defines
                        &mut source_blob as *mut _ as *mut _,
                        &mut result_blob as *mut _ as *mut _,
                        &mut error_blob as *mut _ as *mut _,
                    );
                    
                    // Release source
                    if !source_blob.is_null() {
                        (*source_blob).Release();
                    }
                    
                    if winapi::shared::winerror::FAILED(hr) || result_blob.is_null() {
                        let mut message: *const i8 = std::ptr::null();
                        if !error_blob.is_null() {
                            (*error_blob).GetBufferPointer(&mut message);
                        }
                        
                        let error_msg = if !message.is_null() {
                            CStr::from_ptr(message).to_string_lossy().to_string()
                        } else {
                            "Compilation failed".to_string()
                        };
                        
                        if !error_blob.is_null() {
                            (*error_blob).Release();
                        }
                        
                        return Err(DxcError::CompilationFailed(error_msg));
                    }
                    
                    // Get result data
                    let mut ptr: *const u8 = std::ptr::null();
                    let mut len: u32 = 0;
                    (*result_blob).GetBufferPointer(&mut ptr);
                    (*result_blob).GetBufferSize(&mut len);
                    
                    let data = std::slice::from_raw_parts(ptr, len as usize).to_vec();
                    
                    if !result_blob.is_null() {
                        (*result_blob).Release();
                    }
                    if !library.is_null() {
                        (*library).Release();
                    }
                    
                    Ok(CompiledShader {
                        library: Some(ShaderBytecode { data, size: len as usize }),
                        vs: None,
                        ps: None,
                        gs: None,
                        hs: None,
                        ds: None,
                        cs: None,
                        errors: None,
                        warnings: None,
                    })
                }
            } else {
                Err(DxcError::LibraryNotFound(
                    "Failed to load DXC functions".to_string()
                ))
            }
        }
        
        #[cfg(not(target_os = "windows"))]
        {
            Err(DxcError::LibraryNotFound(
                "DXC is only available on Windows".to_string()
            ))
        }
    }
    
    /// Load pre-compiled DXIL library
    pub fn load_library(data: &[u8]) -> Result<ShaderBytecode, DxcError> {
        Ok(ShaderBytecode {
            data: data.to_vec(),
            size: data.len(),
        })
    }
    
    /// Check if DXC is available
    pub fn is_available() -> bool {
        Self::new().is_ok()
    }
}

/// Compile HLSL source to DXIL bytecode
pub fn compile_hlsl(
    source: &str,
    entry_point: &str,
    target: &str,
) -> Result<CompiledShader, Dx12Error> {
    let compiler = DxcCompiler::new()
        .map_err(|e| Dx12Error::ShaderCompilation(e.to_string()))?;
    
    compiler.compile(source, entry_point, target)
        .map_err(|e| Dx12Error::ShaderCompilation(e.to_string()))
}

/// Load a pre-compiled DXIL library
pub fn load_dxil_library(data: &[u8]) -> Result<ShaderBytecode, Dx12Error> {
    DxcCompiler::load_library(data)
        .map_err(|e| Dx12Error::ShaderCompilation(e.to_string()))
}

/// Create a root signature from bytecode
pub fn create_root_signature(
    device: *mut winapi::um::d3d12::ID3D12Device,
    bytecode: &[u8],
) -> Result<*mut winapi::um::d3d12::ID3D12RootSignature, Dx12Error> {
    unsafe {
        let mut signature: *mut winapi::um::d3d12::ID3D12RootSignature = std::ptr::null_mut();
        let hr = (*device).CreateRootSignature(
            0,
            bytecode.as_ptr() as *const _,
            bytecode.len() as u32,
            &winapi::um::d3d12::IID_ID3D12RootSignature,
            &mut signature as *mut _ as *mut _,
        );
        if winapi::shared::winerror::FAILED(hr) {
            return Err(Dx12Error::PipelineCreation("Root signature creation failed".into()));
        }
        Ok(signature)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn test_dxc_available() {
        // This test will fail if DXC is not installed
        assert!(DxcCompiler::is_available());
    }
}
