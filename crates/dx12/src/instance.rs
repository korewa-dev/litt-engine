//! DXGI instance — adapter enumeration and GPU selection

use super::*;

/// Create the DXGI factory (DXGI 2.0 for Win10+)
pub fn create_dxgi_factory() -> Result<*mut winapi::um::dxgi::IDXGIFactory2, Dx12Error> {
    use winapi::um::dxgi::{CreateDXGIFactory2, IDXGIFactory2};
    use winapi::shared::guiddef::IID_IDXGIFactory2;

    unsafe {
        let mut factory: *mut IDXGIFactory2 = std::ptr::null_mut();
        let hr = CreateDXGIFactory2(0, &IID_IDXGIFactory2, &mut factory as *mut _ as *mut _);
        if winapi::shared::winerror::FAILED(hr) {
            return Err(Dx12Error::DxgiFactoryCreation(format!(
                "CreateDXGIFactory2 failed with 0x{:X}", hr
            )));
        }
        Ok(factory)
    }
}

/// Enumerate all DXGI adapters (GPUs)
pub fn enumerate_adapters(
    factory: *mut winapi::um::dxgi::IDXGIFactory2,
) -> Result<Vec<AdapterInfo>, Dx12Error> {
    let mut adapters: Vec<AdapterInfo> = Vec::new();
    let mut index: u32 = 0;

    unsafe {
        loop {
            let mut adapter: *mut winapi::um::dxgi::IDXGIAdapter1 = std::ptr::null_mut();
            let hr = (*factory).EnumAdapters1(index, &mut adapter);
            if winapi::shared::winerror::SUCCEEDED(hr) && !adapter.is_null() {
                adapters.push(query_adapter_info(adapter));
                winapi::com::Release(adapter as *mut _);
                index += 1;
            } else {
                break;
            }
        }
    }

    if adapters.is_empty() {
        return Err(Dx12Error::AdapterEnumeration("No DXGI adapters found".into()));
    }
    Ok(adapters)
}

/// Query detailed info about a single adapter
pub fn query_adapter_info(adapter: *mut winapi::um::dxgi::IDXGIAdapter1) -> AdapterInfo {
    unsafe {
        let mut desc: winapi::um::dxgi::DXGI_ADAPTER_DESC1 = std::mem::zeroed();
        (*adapter).GetDesc1(&mut desc);

        let name = String::from_utf16_lossy(
            &desc.Description.iter().take_while(|&&c| c != 0).copied().collect::<Vec<u16>>(),
        );
        let vendor = GpuVendor::from_vendor_id(desc.VendorId);
        let ray_tracing = vendor == GpuVendor::Amd || vendor == GpuVendor::Nvidia || vendor == GpuVendor::Intel;

        AdapterInfo {
            name: name.clone(),
            vendor_id: desc.VendorId,
            device_id: desc.DeviceId,
            description: name,
            driver_version: ((desc.DriverVersion.High as u64) << 32) | desc.DriverVersion.Low as u64,
            feature_level: FeatureLevel::D12_0,
            ray_tracing_support: ray_tracing,
        }
    }
}

/// Select the best adapter for rendering
pub fn select_best_adapter(adapters: &[AdapterInfo]) -> Result<usize, Dx12Error> {
    for (i, adapter) in adapters.iter().enumerate() {
        if adapter.ray_tracing_support && adapter.vendor_id != 0x8086 {
            return Ok(i);
        }
    }
    for (i, adapter) in adapters.iter().enumerate() {
        if adapter.feature_level >= FeatureLevel::D12_0 {
            return Ok(i);
        }
    }
    if !adapters.is_empty() {
        Ok(0)
    } else {
        Err(Dx12Error::AdapterEnumeration("No suitable adapter".into()))
    }
}

/// Get the selected adapter info
pub fn get_adapter_info(adapters: &[AdapterInfo], index: usize) -> Result<&AdapterInfo, Dx12Error> {
    adapters.get(index).ok_or(Dx12Error::InvalidParameter(format!(
        "Adapter {} out of range", index
    )))
}
