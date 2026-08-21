//! DX12 Device Creation and Management
//! 
//! Creates D3D12 device, command queues, and feature queries.

use super::*;

/// D3D12 device with command queues
#[derive(Debug)]
pub struct Dx12Device {
    pub device: *mut d3d12::ID3D12Device,
    pub graphics_queue: *mut d3d12::ID3D12CommandQueue,
    pub compute_queue: *mut d3d12::ID3D12CommandQueue,
    pub feature_level: FeatureLevel,
    pub adapter_info: AdapterInfo,
    pub descriptor_handle_size: u32,
    pub ray_tracing_tier: u32,
}

/// Create a D3D12 device from an adapter
pub fn create_device(
    adapter: *mut dxgi::IDXGIAdapter1,
    debug: bool,
) -> Result<Dx12Device, Dx12Error> {
    unsafe {
        // Get adapter info
        let mut desc: dxgi::DXGI_ADAPTER_DESC1 = std::mem::zeroed();
        (*adapter).GetDesc1(&mut desc);
        
        let name = String::from_utf16_lossy(
            &desc.Description.iter().take_while(|&&c| c != 0).copied().collect::<Vec<u16>>(),
        );
        let vendor = GpuVendor::from_vendor_id(desc.VendorId);
        
        // Create device
        let mut device: *mut d3d12::ID3D12Device = std::ptr::null_mut();
        let feature_level = d3d12::D3D_FEATURE_LEVEL_12_1;
        
        let hr = d3d12::D3D12CreateDevice(
            adapter as *mut _,
            feature_level,
            &d3d12::IID_ID3D12Device,
            &mut device as *mut _ as *mut _,
        );
        
        if winapi::shared::winerror::FAILED(hr) {
            return Err(Dx12Error::DeviceCreation(format!(
                "D3D12CreateDevice failed with 0x{:X}", hr
            )));
        }
        
        // Enable debug layer if requested
        if debug {
            let mut debug_ptr: *mut d3d12::ID3D12Debug = std::ptr::null_mut();
            let hr = d3d12::D3D12GetDebugInterface(
                &d3d12::IID_ID3D12Debug,
                &mut debug_ptr as *mut _ as *mut _,
            );
            if winapi::shared::winerror::SUCCEEDED(hr) && !debug_ptr.is_null() {
                (*debug_ptr).EnableDebugLayer();
            }
        }
        
        // Query descriptor handle sizes
        let mut feature_caps: d3d12::D3D12_FEATURE_DATA_D3D12_OPTIONS = std::mem::zeroed();
        let hr = (*device).CheckFeatureSupport(
            d3d12::D3D12_FEATURE_D3D12_OPTIONS,
            &mut feature_caps as *mut _ as *mut _,
            std::mem::size_of::<d3d12::D3D12_FEATURE_DATA_D3D12_OPTIONS>() as u32,
        );
        let descriptor_handle_size = if winapi::shared::winerror::SUCCEEDED(hr) {
            feature_caps.NodeDescriptorHandleIncrementSize[0]
        } else { 16 };
        
        // Query ray tracing tier
        let mut rt_caps: d3d12::D3D12_FEATURE_DATA_D3D12_OPTIONS11 = std::mem::zeroed();
        let hr = (*device).CheckFeatureSupport(
            d3d12::D3D12_FEATURE_D3D12_OPTIONS11,
            &mut rt_caps as *mut _ as *mut _,
            std::mem::size_of::<d3d12::D3D12_FEATURE_DATA_D3D12_OPTIONS11>() as u32,
        );
        let ray_tracing_tier = if winapi::shared::winerror::SUCCEEDED(hr) {
            rt_caps.RaytracingTier as u32
        } else { 0 };
        
        // Create command queues
        let mut graphics_queue: *mut d3d12::ID3D12CommandQueue = std::ptr::null_mut();
        let mut compute_queue: *mut d3d12::ID3D12CommandQueue = std::ptr::null_mut();
        
        let mut queue_desc: d3d12::D3D12_COMMAND_QUEUE_DESC = std::mem::zeroed();
        queue_desc.Type = d3d12::D3D12_COMMAND_LIST_TYPE_GRAPHICS;
        queue_desc.Priority = 0;
        queue_desc.Flags = d3d12::D3D12_COMMAND_QUEUE_FLAG_NONE;
        queue_desc.NodeMask = 0;
        
        let hr = (*device).CreateCommandQueue(
            &queue_desc,
            &d3d12::IID_ID3D12CommandQueue,
            &mut graphics_queue as *mut _ as *mut _,
        );
        if winapi::shared::winerror::FAILED(hr) {
            return Err(Dx12Error::CommandQueueCreation("Graphics queue creation failed".into()));
        }
        
        // Create compute queue
        queue_desc.Type = d3d12::D3D12_COMMAND_LIST_TYPE_COMPUTE;
        let hr = (*device).CreateCommandQueue(
            &queue_desc,
            &d3d12::IID_ID3D12CommandQueue,
            &mut compute_queue as *mut _ as *mut _,
        );
        if winapi::shared::winerror::FAILED(hr) {
            // Fallback to graphics queue if compute not supported
            compute_queue = graphics_queue;
        }
        
        // Determine feature level
        let feature_level = if ray_tracing_tier >= 1 {
            FeatureLevel::D12_1
        } else {
            FeatureLevel::D12_0
        };
        
        // Build adapter info
        let adapter_info = AdapterInfo {
            name: name.clone(),
            vendor_id: desc.VendorId,
            device_id: desc.DeviceId,
            description: name,
            driver_version: ((desc.DriverVersion.High as u64) << 32) | desc.DriverVersion.Low as u64,
            feature_level,
            ray_tracing_support: ray_tracing_tier >= 1,
            ray_tracing_tier,
        };
        
        Ok(Dx12Device {
            device,
            graphics_queue,
            compute_queue,
            feature_level,
            adapter_info,
            descriptor_handle_size,
            ray_tracing_tier,
        })
    }
}

/// Check if ray tracing is supported
pub fn check_ray_tracing_support(device: &Dx12Device) -> bool {
    device.ray_tracing_tier >= 1
}

/// Get ray tracing tier
pub fn get_ray_tracing_tier(device: &Dx12Device) -> u32 {
    device.ray_tracing_tier
}
