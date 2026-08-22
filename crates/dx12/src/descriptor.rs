//! DX12 descriptor heaps -- CBV, SRV, UAV, RTV, DSV, sampler

use super::*;

/// Descriptor heap type
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum DescriptorHeapType {
    /// Constant buffer / shader resource / UAV descriptors
    ShaderVisible,
    /// Render target views
    RenderTarget,
    /// Depth stencil views
    DepthStencil,
    /// Sampler descriptors
    Sampler,
}

/// Descriptor heap for shader-visible resources
#[derive(Debug)]
pub struct DescriptorHeap {
    pub heap: *mut winapi::um::d3d12::ID3D12DescriptorHeap,
    pub heap_type: DescriptorHeapType,
    pub handle_size: u32,
}

/// CPU handle to a descriptor
#[derive(Clone, Copy, Debug)]
pub struct CpuDescriptor(pub winapi::um::d3d12::D3D12_CPU_DESCRIPTOR_HANDLE);

/// GPU handle to a descriptor
#[derive(Clone, Copy, Debug)]
pub struct GpuDescriptor(pub winapi::um::d3d12::D3D12_GPU_DESCRIPTOR_HANDLE);

impl DescriptorHeap {
    /// Create a new descriptor heap
    pub fn new(
        device: *mut winapi::um::d3d12::ID3D12Device,
        heap_type: DescriptorHeapType,
        max_descriptors: u32,
    ) -> Result<Self, Dx12Error> {
        unsafe {
            let dxgi_format = match heap_type {
                DescriptorHeapType::ShaderVisible => winapi::um::d3d12::D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
                DescriptorHeapType::RenderTarget => winapi::um::d3d12::D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
                DescriptorHeapType::DepthStencil => winapi::um::d3d12::D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
                DescriptorHeapType::Sampler => winapi::um::d3d12::D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
            };

            let mut flags = winapi::um::d3d12::D3D12_DESCRIPTOR_HEAP_FLAGS(0);
            if heap_type == DescriptorHeapType::ShaderVisible {
                flags.0 |= winapi::um::d3d12::D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE.0;
            }

            let mut desc: winapi::um::d3d12::D3D12_DESCRIPTOR_HEAP_DESC = std::mem::zeroed();
            desc.Type = dxgi_format;
            desc.NumDescriptors = max_descriptors;
            desc.Flags = flags;
            desc.NodeMask = 0;

            let mut heap: *mut winapi::um::d3d12::ID3D12DescriptorHeap = std::ptr::null_mut();
            let hr = (*device).CreateDescriptorHeap(
                &desc,
                &winapi::um::d3d12::IID_ID3D12DescriptorHeap,
                &mut heap as *mut _ as *mut _,
            );
            if winapi::shared::winerror::FAILED(hr) {
                return Err(Dx12Error::DescriptorHeapCreation("Descriptor heap creation failed".into()));
            }

            let handle_size = (*device).GetDescriptorHandleIncrementSize(dxgi_format);

            Ok(DescriptorHeap {
                heap,
                heap_type,
                handle_size,
            })
        }
    }

    /// Get the next available CPU descriptor
    pub fn allocate_cpu(&self, offsets: u32) -> CpuDescriptor {
        unsafe {
            let base = (*self.heap).GetCPUDescriptorHandleForHeapStart();
            CpuDescriptor(winapi::um::d3d12::D3D12_CPU_DESCRIPTOR_HANDLE {
                ptr: base.ptr + (self.handle_size * offsets) as usize,
            })
        }
    }

    /// Get the GPU descriptor handle
    pub fn gpu_handle(&self, cpu: &CpuDescriptor) -> GpuDescriptor {
        unsafe {
            GpuDescriptor(winapi::um::d3d12::D3D12_GPU_DESCRIPTOR_HANDLE {
                ptr: cpu.0.ptr as u64 - (*self.heap).GetGPUDescriptorHandleForHeapStart().ptr as u64 + self.heap as u64,
            })
        }
    }
}

impl Drop for DescriptorHeap {
    fn drop(&mut self) {
        if !self.heap.is_null() {
            unsafe { winapi::com::Release(self.heap as *mut _) };
        }
    }
}

/// Create a CBV descriptor
pub fn create_cbv(
    device: *mut winapi::um::d3d12::ID3D12Device,
    cbv_desc: &winapi::um::d3d12::D3D12_CONSTANT_BUFFER_VIEW_DESC,
    heap: &DescriptorHeap,
    offset: u32,
) -> Result<CpuDescriptor, Dx12Error> {
    unsafe {
        let cpu_handle = heap.allocate_cpu(offset);
        (*device).CreateConstantBufferView(cbv_desc, cpu_handle.0);
        Ok(cpu_handle)
    }
}

/// Create a SRV descriptor
pub fn create_srv(
    device: *mut winapi::um::d3d12::ID3D12Device,
    srv_desc: &winapi::um::d3d12::D3D12_SHADER_RESOURCE_VIEW_DESC,
    heap: &DescriptorHeap,
    offset: u32,
) -> Result<CpuDescriptor, Dx12Error> {
    unsafe {
        let cpu_handle = heap.allocate_cpu(offset);
        (*device).CreateShaderResourceView(std::ptr::null(), srv_desc, cpu_handle.0);
        Ok(cpu_handle)
    }
}

/// Create an RTV descriptor
pub fn create_rtv(
    device: *mut winapi::um::d3d12::ID3D12Device,
    resource: *mut winapi::um::d3d12::ID3D12Resource,
    rtv_desc: &winapi::um::d3d12::D3D12_RENDER_TARGET_VIEW_DESC,
    heap: &DescriptorHeap,
    offset: u32,
) -> Result<CpuDescriptor, Dx12Error> {
    unsafe {
        let cpu_handle = heap.allocate_cpu(offset);
        (*device).CreateRenderTargetView(resource, rtv_desc, cpu_handle.0);
        Ok(cpu_handle)
    }
}

/// Create a DSV descriptor
pub fn create_dsv(
    device: *mut winapi::um::d3d12::ID3D12Device,
    resource: *mut winapi::um::d3d12::ID3D12Resource,
    dsv_desc: &winapi::um::d3d12::D3D12_DEPTH_STENCIL_VIEW_DESC,
    heap: &DescriptorHeap,
    offset: u32,
) -> Result<CpuDescriptor, Dx12Error> {
    unsafe {
        let cpu_handle = heap.allocate_cpu(offset);
        (*device).CreateDepthStencilView(resource, dsv_desc, cpu_handle.0);
        Ok(cpu_handle)
    }
}
