//! D3D12 device creation and management

use super::*;

/// D3D12 device with command queues
#[derive(Debug)]
pub struct Dx12Device {
    pub device: *mut winapi::um::d3d12::ID3D12Device,
    pub graphics_queue: *mut winapi::um::d3d12::ID3D12CommandQueue,
    pub feature_level: FeatureLevel,
    pub adapter_info: AdapterInfo,
    pub descriptor_handle_size: u32,
}

/// Create a D3D12 device from an adapter
pub fn create_device(
    adapter: *mut winapi::um::dxgi::IDXGIAdapter1,
    debug: bool,
) -> Result<Dx12Device, Dx12Error> {
    unsafe {
        let feature_levels = [
            winapi::um::d3d12::D3D_FEATURE_LEVEL_12_1,
            winapi::um::d3d12::D3D_FEATURE_LEVEL_12_0,
        ];

        let mut device: *mut winapi::um::d3d12::ID3D12Device = std::ptr::null_mut();
        let mut feature_level = winapi::um::d3d12::D3D_FEATURE_LEVEL_11_0;

        let hr = winapi::um::d3d12::D3D12CreateDevice(
            adapter as *mut _,
            feature_levels[0],
            &winapi::um::d3d12::IID_ID3D12Device,
            &mut device as *mut _ as *mut _,
        );

        if winapi::shared::winerror::FAILED(hr) {
            let hr2 = winapi::um::d3d12::D3D12CreateDevice(
                adapter as *mut _,
                feature_levels[1],
                &winapi::um::d3d12::IID_ID3D12Device,
                &mut device as *mut _ as *mut _,
            );
            if winapi::shared::winerror::FAILED(hr2) {
                return Err(Dx12Error::DeviceCreation(format!("D3D12CreateDevice failed 0x{:X}", hr2)));
            }
            feature_level = feature_levels[1];
        } else {
            feature_level = feature_levels[0];
        }

        if device.is_null() {
            return Err(Dx12Error::DeviceCreation("Device is null".into()));
        }

        // Enable debug layer
        if debug {
            let mut debug_ptr: *mut winapi::um::d3d12::ID3D12Debug = std::ptr::null_mut();
            let hr = winapi::um::d3d12::D3D12GetDebugInterface(
                &winapi::um::d3d12::IID_ID3D12Debug,
                &mut debug_ptr as *mut _ as *mut _,
            );
            if winapi::shared::winerror::SUCCEEDED(hr) {
                (*debug_ptr).EnableDebugLayer();
            }
        }

        // Query descriptor handle size
        let mut caps: winapi::um::d3d12::D3D12_FEATURE_DATA_D3D12_OPTIONS = std::mem::zeroed();
        let hr = (*device).CheckFeatureSupport(
            winapi::um::d3d12::D3D12_FEATURE_D3D12_OPTIONS,
            &mut caps as *mut _ as *mut _,
            std::mem::size_of::<winapi::um::d3d12::D3D12_FEATURE_DATA_D3D12_OPTIONS>() as u32,
        );
        let descriptor_handle_size = if winapi::shared::winerror::SUCCEEDED(hr) {
            caps.NodeDescriptorHandleIncrementSize
        } else { 16 };

        // Create graphics command queue
        let mut queue_desc: winapi::um::d3d12::D3D12_COMMAND_QUEUE_DESC = std::mem::zeroed();
        queue_desc.Type = winapi::um::d3d12::D3D12_COMMAND_LIST_TYPE_GRAPHICS;

        let mut graphics_queue: *mut winapi::um::d3d12::ID3D12CommandQueue = std::ptr::null_mut();
        let hr = (*device).CreateCommandQueue(
            &queue_desc,
            &winapi::um::d3d12::IID_ID3D12CommandQueue,
            &mut graphics_queue as *mut _ as *mut _,
        );
        if winapi::shared::winerror::FAILED(hr) {
            return Err(Dx12Error::CommandQueueCreation("Graphics queue creation failed".into()));
        }

        let feature_level = match feature_level {
            winapi::um::d3d12::D3D_FEATURE_LEVEL_12_1 => FeatureLevel::D12_1,
            winapi::um::d3d12::D3D_FEATURE_LEVEL_12_0 => FeatureLevel::D12_0,
            _ => FeatureLevel::D11_0,
        };

        // Query adapter info
        let mut desc: winapi::um::dxgi::DXGI_ADAPTER_DESC1 = std::mem::zeroed();
        (adapter as *mut winapi::um::dxgi::IDXGIAdapter).as_ref().unwrap().GetDesc1(&mut desc);
        let adapter_info = AdapterInfo {
            name: String::from_utf16_lossy(&desc.Description.iter().take_while(|&&c| c != 0).copied().collect::<Vec<u16>>()),
            vendor_id: desc.VendorId,
            device_id: desc.DeviceId,
            description: String::new(),
            driver_version: 0,
            feature_level,
            ray_tracing_support: desc.Flags & winapi::um::dxgi::DXGI_ADAPTER_FLAG3_RAYTRACING >= winapi::um::dxgi::DXGI_ADAPTER_FLAG3_RAYTRACING,
        };

        Ok(Dx12Device {
            device,
            graphics_queue,
            feature_level,
            adapter_info,
            descriptor_handle_size,
        })
    }
}

/// Check ray tracing support
pub fn check_ray_tracing_support(device: &Dx12Device) -> bool {
    unsafe {
        let mut rt_caps: winapi::um::d3d12::D3D12_FEATURE_DATA_D3D12_OPTIONS11 = std::mem::zeroed();
        let hr = (*device.device).CheckFeatureSupport(
            winapi::um::d3d12::D3D12_FEATURE_D3D12_OPTIONS11,
            &mut rt_caps as *mut _ as *mut _,
            std::mem::size_of::<winapi::um::d3d12::D3D12_FEATURE_DATA_D3D12_OPTIONS11>() as u32,
        );
        winapi::shared::winerror::SUCCEEDED(hr) && rt_caps.RaytracingTier != 0
    }
}
