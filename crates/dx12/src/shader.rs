//! DX12 shader compilation — DXIL and HLSL support

use super::*;

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
}

/// Compile HLSL source to DXIL bytecode
pub fn compile_hlsl(
    source: &str,
    entry_point: &str,
    target: &str,
) -> Result<CompiledShader, Dx12Error> {
    // DXIL compilation requires dxcompiler.dll
    // This is a placeholder for the actual implementation
    
    // In a full implementation, you would:
    // 1. Load dxcompiler.dll
    // 2. Create IDxcBlobWide with source
    // 3. Compile with DxcCompile
    // 4. Return the resulting bytecode
    
    // For now, return an error indicating compilation requires the DXC compiler
    Err(Dx12Error::ShaderCompilation(
        "DXC compiler not available. Please ensure dxcompiler.dll is in the PATH.".into(),
    ))
}

/// Load a pre-compiled DXIL library
pub fn load_dxil_library(data: &[u8]) -> Result<ShaderBytecode, Dx12Error> {
    Ok(ShaderBytecode {
        data: data.to_vec(),
        size: data.len(),
    })
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
