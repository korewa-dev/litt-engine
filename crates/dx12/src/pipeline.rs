//! DX12 pipeline state objects (PSOs)

use super::*;

/// Pipeline state object for graphics rendering
#[derive(Debug)]
pub struct GraphicsPipeline {
    pub pso: *mut winapi::um::d3d12::ID3D12PipelineState,
    pub root_signature: *mut winapi::um::d3d12::ID3D12RootSignature,
}

/// Pipeline state object for compute
#[derive(Debug)]
pub struct ComputePipeline {
    pub pso: *mut winapi::um::d3d12::ID3D12PipelineState,
    pub root_signature: *mut winapi::um::d3d12::ID3D12RootSignature,
}

/// Pipeline state object for ray tracing
#[derive(Debug)]
pub struct RayTracingPipeline {
    pub pso: *mut winapi::um::d3d12::ID3D12PipelineState,
    pub root_signature: *mut winapi::um::d3d12::ID3D12RootSignature,
}

/// Create a graphics PSO from compiled bytecode
pub fn create_graphics_pso(
    device: *mut winapi::um::d3d12::ID3D12Device,
    root_signature: *mut winapi::um::d3d12::ID3D12RootSignature,
    vs_bytecode: *const winapi::um::d3dcommon::D3D12_SHADER_BYTECODE,
    ps_bytecode: *const winapi::um::d3dcommon::D3D12_SHADER_BYTECODE,
    rtv_format: winapi::um::dxgi::DXGI_FORMAT,
    dsv_format: winapi::um::dxgi::DXGI_FORMAT,
) -> Result<GraphicsPipeline, Dx12Error> {
    unsafe {
        let mut pso_desc: winapi::um::d3d12::D3D12_PIPELINE_STATE_STREAM_DESC = std::mem::zeroed();
        let mut pso: winapi::um::d3d12::D3D12_PIPELINE_STATE_OBJECT_DESC = std::mem::zeroed();

        pso_desc.Size = std::mem::size_of::<winapi::um::d3d12::D3D12_PIPELINE_STATE_OBJECT_DESC>() as u32;
        pso_desc.pPipelineStateDesc = &mut pso as *mut _ as *mut std::ffi::c_void;

        pso.Type = winapi::um::d3d12::D3D12_PIPELINE_STATE_TYPE_OBJECT;
        pso.Flags = winapi::um::d3d12::D3D12_PIPELINE_STATE_FLAGS(0);
        pso.RootSignature = root_signature;
        pso.InputLayout = winapi::um::d3d12::D3D12_INPUT_LAYOUT_DESC {
            pDescriptorArrays: std::ptr::null(),
            pInputElement: std::ptr::null(),
            NumElements: 0,
        };
        pso.BlendState = winapi::um::d3d12::D3D12_BLEND_DESC {
            AlphaToCoverageEnable: winapi::shared::windef::FALSE,
            IndependentBlendEnable: winapi::shared::windef::TRUE,
            RenderTarget: [std::mem::zeroed(); 8],
        };
        pso.RasterizerState = winapi::um::d3d12::D3D12_RASTERIZER_DESC {
            FillMode: winapi::um::d3d12::D3D12_FILL_MODE_SOLID,
            CullMode: winapi::um::d3d12::D3D12_CULL_MODE_BACK,
            FrontCounterClockwise: winapi::shared::windef::FALSE,
            DepthBias: 0,
            DepthBiasClamp: 0.0,
            SlopeScaledDepthBias: 0.0,
            DepthClipEnable: winapi::shared::windef::TRUE,
            MultisampleEnable: winapi::shared::windef::FALSE,
            AntialiasedLineEnable: winapi::shared::windef::FALSE,
            ForcedSampleCount: 0,
            ConservativeRaster: winapi::um::d3d12::D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF,
        };
        pso.DepthStencilState = winapi::um::d3d12::D3D12_DEPTH_STENCIL_DESC {
            DepthEnable: winapi::shared::windef::TRUE,
            DepthWriteMask: winapi::um::d3d12::D3D12_DEPTH_WRITE_MASK_ALL,
            DepthFunc: winapi::um::d3d12::D3D12_COMPARISON_FUNC_LESS,
            StencilEnable: winapi::shared::windef::FALSE,
            StencilReadMask: 0xFF,
            StencilWriteMask: 0xFF,
            FrontFace: winapi::um::d3d12::D3D12_DEPTH_STENCIL_DESC::default(),
            BackFace: winapi::um::d3d12::D3D12_DEPTH_STENCIL_DESC::default(),
        };
        pso.PrimitiveTopologyType = winapi::um::d3d12::D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        pso.NumRenderTargets = 1;
        pso.RTVFormats[0] = rtv_format;
        pso.DSVFormat = dsv_format;
        pso.SampleMask = 0xFFFFFFFF;
        pso.NumDSViews = 0;
        pso.NodeMask = 0;

        // Set shader bytecode (this is a simplified version)
        // In practice, you'd need proper VS/PS bytecode structures

        let mut pso_ptr: *mut winapi::um::d3d12::ID3D12PipelineState = std::ptr::null_mut();
        let hr = (*device).CreatePipelineState(
            &pso_desc,
            &winapi::um::d3d12::IID_ID3D12PipelineState,
            &mut pso_ptr as *mut _ as *mut _,
        );
        if winapi::shared::winerror::FAILED(hr) {
            return Err(Dx12Error::PipelineCreation("Graphics PSO creation failed".into()));
        }

        Ok(GraphicsPipeline {
            pso: pso_ptr,
            root_signature,
        })
    }
}

/// Create a compute PSO
pub fn create_compute_pso(
    device: *mut winapi::um::d3d12::ID3D12Device,
    root_signature: *mut winapi::um::d3d12::ID3D12RootSignature,
) -> Result<ComputePipeline, Dx12Error> {
    unsafe {
        let mut pso: *mut winapi::um::d3d12::ID3D12PipelineState = std::ptr::null_mut();
        let hr = (*device).CreateComputePipelineState(
            std::ptr::null(), // CS bytecode would go here
            &winapi::um::d3d12::IID_ID3D12PipelineState,
            &mut pso as *mut _ as *mut _,
        );
        if winapi::shared::winerror::FAILED(hr) {
            return Err(Dx12Error::PipelineCreation("Compute PSO creation failed".into()));
        }

        Ok(ComputePipeline { pso, root_signature })
    }
}
