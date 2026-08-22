//! DX12 resource allocation -- heaps, buffers, textures

use super::*;

/// Heap type for resource allocation
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum HeapType {
    /// Default heap (GPU-only, fast)
    Default,
    /// Upload heap (CPU writes, GPU reads)
    Upload,
    /// Readback heap (GPU reads, CPU reads)
    Readback,
}

/// Resource creation flags
#[derive(Clone, Copy, Debug)]
pub struct ResourceFlags(pub u32);

impl Default for ResourceFlags {
    fn default() -> Self {
        Self(0)
    }
}

/// Buffer resource
#[derive(Debug)]
pub struct Buffer {
    pub resource: *mut winapi::um::d3d12::ID3D12Resource,
    pub heap_allocation: Option<*mut winapi::um::d3d12::ID3D12Heap>,
    pub size: u64,
    pub state: Dx12ResourceState,
}

/// Texture resource
#[derive(Debug)]
pub struct Texture {
    pub resource: *mut winapi::um::d3d12::ID3D12Resource,
    pub heap_allocation: Option<*mut winapi::um::d3d12::ID3D12Heap>,
    pub width: u32,
    pub height: u32,
    pub format: winapi::um::dxgi::DXGI_FORMAT,
    pub mip_levels: u32,
    pub array_layers: u32,
    pub state: Dx12ResourceState,
}

/// Current resource state
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Dx12ResourceState {
    Present,
    RenderTarget,
    DepthWrite,
    ShaderResource,
    UnorderedAccess,
    RayTracingSource,
    RayTracingAcceleration,
    CopyDest,
    CopySource,
    General,
}

impl Buffer {
    /// Create a new buffer
    pub fn new(
        device: *mut winapi::um::d3d12::ID3D12Device,
        size: u64,
        heap_type: HeapType,
        flags: ResourceFlags,
    ) -> Result<Self, Dx12Error> {
        unsafe {
            let mut heap_props: winapi::um::d3d12::D3D12_HEAP_PROPERTIES = std::mem::zeroed();
            heap_props.Type = match heap_type {
                HeapType::Default => winapi::um::d3d12::D3D12_HEAP_TYPE_DEFAULT,
                HeapType::Upload => winapi::um::d3d12::D3D12_HEAP_TYPE_UPLOAD,
                HeapType::Readback => winapi::um::d3d12::D3D12_HEAP_TYPE_READBACK,
            };
            heap_props.CPUPageProperty = winapi::um::d3d12::D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
            heap_props.MemoryPoolPreference = winapi::um::d3d12::D3D12_MEMORY_POOL_UNKNOWN;
            heap_props.CreationNodeMask = 1;
            heap_props.VisibleNodeMask = 1;

            let mut desc: winapi::um::d3d12::D3D12_RESOURCE_DESC = std::mem::zeroed();
            desc.Dimension = winapi::um::d3d12::D3D12_RESOURCE_DIMENSION_BUFFER;
            desc.Alignment = 0;
            desc.Width = size;
            desc.Height = 1;
            desc.DepthOrArraySize = 1;
            desc.MipLevels = 1;
            desc.Format = winapi::um::dxgi::DXGI_FORMAT_UNKNOWN;
            desc.SampleDesc.Count = 1;
            desc.SampleDesc.Quality = 0;
            desc.Layout = winapi::um::d3d12::D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            desc.Flags = flags.0;

            let mut state = winapi::um::d3d12::D3D12_RESOURCE_STATES::default();
            if heap_type == HeapType::Upload {
                state = winapi::um::d3d12::D3D12_RESOURCE_STATE_GENERIC_READ;
            } else {
                state = winapi::um::d3d12::D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
            }

            let mut resource: *mut winapi::um::d3d12::ID3D12Resource = std::ptr::null_mut();
            let hr = (*device).CreateCommittedResource(
                &heap_props,
                winapi::um::d3d12::D3D12_HEAP_FLAGS(0),
                &desc,
                state,
                std::ptr::null(),
                &winapi::um::d3d12::IID_ID3D12Resource,
                &mut resource as *mut _ as *mut _,
            );

            if winapi::shared::winerror::FAILED(hr) {
                return Err(Dx12Error::ResourceAllocation("Buffer creation failed".into()));
            }

            Ok(Buffer {
                resource,
                heap_allocation: None,
                size,
                state: Dx12ResourceState::General,
            })
        }
    }

    /// Map the buffer for CPU access (upload/readback only)
    pub fn map(&mut self) -> Result<*mut std::ffi::c_void, Dx12Error> {
        unsafe {
            let mut mapped_ptr: *mut std::ffi::c_void = std::ptr::null_mut();
            let hr = (*self.resource).Map(0, std::ptr::null(), &mut mapped_ptr as *mut _ as *mut _);
            if winapi::shared::winerror::FAILED(hr) {
                return Err(Dx12Error::ResourceAllocation("Buffer map failed".into()));
            }
            Ok(mapped_ptr)
        }
    }

    /// Unmap the buffer
    pub fn unmap(&mut self) {
        unsafe {
            (*self.resource).Unmap(0, std::ptr::null());
        }
    }
}

impl Texture {
    /// Create a new texture
    pub fn new(
        device: *mut winapi::um::d3d12::ID3D12Device,
        width: u32,
        height: u32,
        format: winapi::um::dxgi::DXGI_FORMAT,
        mip_levels: u32,
        array_layers: u32,
        heap_type: HeapType,
    ) -> Result<Self, Dx12Error> {
        unsafe {
            let mut heap_props: winapi::um::d3d12::D3D12_HEAP_PROPERTIES = std::mem::zeroed();
            heap_props.Type = match heap_type {
                HeapType::Default => winapi::um::d3d12::D3D12_HEAP_TYPE_DEFAULT,
                HeapType::Upload => winapi::um::d3d12::D3D12_HEAP_TYPE_UPLOAD,
                HeapType::Readback => winapi::um::d3d12::D3D12_HEAP_TYPE_READBACK,
            };
            heap_props.CPUPageProperty = winapi::um::d3d12::D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
            heap_props.MemoryPoolPreference = winapi::um::d3d12::D3D12_MEMORY_POOL_UNKNOWN;
            heap_props.CreationNodeMask = 1;
            heap_props.VisibleNodeMask = 1;

            let mut desc: winapi::um::d3d12::D3D12_RESOURCE_DESC = std::mem::zeroed();
            desc.Dimension = winapi::um::d3d12::D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            desc.Alignment = 0;
            desc.Width = width as u64;
            desc.Height = height;
            desc.DepthOrArraySize = array_layers as i16;
            desc.MipLevels = mip_levels;
            desc.Format = format;
            desc.SampleDesc.Count = 1;
            desc.SampleDesc.Quality = 0;
            desc.Layout = winapi::um::d3d12::D3D12_TEXTURE_LAYOUT_UNKNOWN;
            desc.Flags = winapi::um::d3d12::D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET.0;

            let state = if heap_type == HeapType::Upload {
                winapi::um::d3d12::D3D12_RESOURCE_STATE_GENERIC_READ
            } else {
                winapi::um::d3d12::D3D12_RESOURCE_STATE_COPY_DEST
            };

            let mut resource: *mut winapi::um::d3d12::ID3D12Resource = std::ptr::null_mut();
            let hr = (*device).CreateCommittedResource(
                &heap_props,
                winapi::um::d3d12::D3D12_HEAP_FLAGS(0),
                &desc,
                state,
                std::ptr::null(),
                &winapi::um::d3d12::IID_ID3D12Resource,
                &mut resource as *mut _ as *mut _,
            );

            if winapi::shared::winerror::FAILED(hr) {
                return Err(Dx12Error::ResourceAllocation("Texture creation failed".into()));
            }

            Ok(Texture {
                resource,
                heap_allocation: None,
                width,
                height,
                format,
                mip_levels,
                array_layers,
                state: Dx12ResourceState::General,
            })
        }
    }
}

impl Drop for Buffer {
    fn drop(&mut self) {
        if !self.resource.is_null() {
            unsafe { winapi::com::Release(self.resource as *mut _) };
        }
    }
}

impl Drop for Texture {
    fn drop(&mut self) {
        if !self.resource.is_null() {
            unsafe { winapi::com::Release(self.resource as *mut _) };
        }
    }
}
