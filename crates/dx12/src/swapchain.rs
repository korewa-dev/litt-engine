//! DXGI swapchain -- frame presentation

use super::*;

/// Swapchain for frame presentation
#[derive(Debug)]
pub struct Swapchain {
    pub swapchain: *mut winapi::um::dxgi::IDXGISwapChain4,
    pub backbuffer_count: u32,
    pub width: u32,
    pub height: u32,
    pub format: DxgiFormat,
}

/// DXGI format enum
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum DxgiFormat {
    R8G8B8A8_UNORM,
    R16G16B16A16_FLOAT,
    B8G8R8A8_UNORM,
    Unknown,
}

impl Swapchain {
    /// Create a swapchain for the given window
    pub fn create(
        factory: *mut winapi::um::dxgi::IDXGIFactory2,
        device: *mut winapi::um::d3d12::ID3D12Device,
        hwnd: *mut std::ffi::c_void,
        width: u32,
        height: u32,
        buffer_count: u32,
    ) -> Result<Swapchain, Dx12Error> {
        unsafe {
            let mut swapchain: *mut winapi::um::dxgi::IDXGISwapChain4 = std::ptr::null_mut();
            let mut swapchain_desc: winapi::um::dxgi::DXGI_SWAP_CHAIN_DESC = std::mem::zeroed();

            swapchain_desc.BufferCount = buffer_count as i32;
            swapchain_desc.BufferUsage = winapi::um::dxgi::DXGI_USAGE_RENDER_TARGET_OUTPUT;
            swapchain_desc.BufferDesc.Format = winapi::um::dxgi::DXGI_FORMAT_R8G8B8A8_UNORM;
            swapchain_desc.BufferDesc.Width = width as i32;
            swapchain_desc.BufferDesc.Height = height as i32;
            swapchain_desc.SwapEffect = winapi::um::dxgi::DXGI_SWAP_EFFECT_FLIP_DISCARD;
            swapchain_desc.OutputWindow = hwnd as *mut winapi::shared::windef::HWND;
            swapchain_desc.Windowed = winapi::shared::windef::TRUE as i32;
            swapchain_desc.Flags = 0;

            let hr = (*factory).CreateSwapChain(
                device,
                &swapchain_desc,
                &mut swapchain as *mut _ as *mut _,
            );

            if winapi::shared::winerror::FAILED(hr) {
                return Err(Dx12Error::SwapchainCreation(format!(
                    "CreateSwapChain failed 0x{:X}", hr
                )));
            }

            // Resize to IDXGISwapChain4
            let mut swapchain4: *mut winapi::um::dxgi::IDXGISwapChain4 = std::ptr::null_mut();
            let hr = (*swapchain).QueryInterface(
                &winapi::um::dxgi::IID_IDXGISwapChain4,
                &mut swapchain4 as *mut _ as *mut _,
            );
            if winapi::shared::winerror::SUCCEEDED(hr) {
                winapi::com::Release(swapchain as *mut _);
                swapchain = swapchain4;
            }

            Ok(Swapchain {
                swapchain,
                backbuffer_count: buffer_count,
                width,
                height,
                format: DxgiFormat::R8G8B8A8_UNORM,
            })
        }
    }

    /// Present the current backbuffer to the screen
    pub fn present(&mut self, sync_interval: u32) -> Result<(), Dx12Error> {
        unsafe {
            let hr = (*self.swapchain).Present(sync_interval, 0);
            if winapi::shared::winerror::FAILED(hr) {
                return Err(Dx12Error::DxError(format!("Present failed 0x{:X}", hr)));
            }
            Ok(())
        }
    }

    /// Get the backbuffer texture index
    pub fn current_backbuffer(&self) -> u32 {
        unsafe { (*self.swapchain).GetCurrentBackBufferIndex() as u32 }
    }

    /// Get the backbuffer texture at the given index
    pub fn get_backbuffer(&self, index: u32) -> Result<*mut winapi::um::d3d12::ID3D12Resource, Dx12Error> {
        unsafe {
            let mut resource: *mut winapi::um::d3d12::ID3D12Resource = std::ptr::null_mut();
            let hr = (*self.swapchain).GetBuffer(
                index,
                &winapi::um::d3d12::IID_ID3D12Resource,
                &mut resource as *mut _ as *mut _,
            );
            if winapi::shared::winerror::FAILED(hr) {
                return Err(Dx12Error::ResourceAllocation(format!("GetBuffer failed 0x{:X}", hr)));
            }
            Ok(resource)
        }
    }
}

impl Drop for Swapchain {
    fn drop(&mut self) {
        if !self.swapchain.is_null() {
            unsafe { winapi::com::Release(self.swapchain as *mut _) };
        }
    }
}
