//! DXGI swapchain -- frame presentation (stub).

use crate::{Dx12Error, Dx12Result};

/// Presentation mode.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum PresentMode {
    /// Vsync on (DXGI_SWAP_EFFECT_FLIP_DISCARD + vsync).
    Fifo,
    /// Mailbox-equivalent: always present latest.
    Immediate,
    /// Allow tearing (required for frame generation).
    AllowTearing,
}

/// Swapchain description.
#[derive(Clone, Debug)]
pub struct SwapchainDesc {
    pub width: u32,
    pub height: u32,
    pub buffer_count: u32,
    pub format_bgra8: bool,
    pub present_mode: PresentMode,
}

impl Default for SwapchainDesc {
    fn default() -> Self {
        Self {
            width: 1280,
            height: 720,
            buffer_count: 3,
            format_bgra8: true,
            present_mode: PresentMode::Fifo,
        }
    }
}

/// DXGI swapchain wrapper. Stub.
#[derive(Debug, Default)]
pub struct DxgiSwapchain {
    pub desc: Option<SwapchainDesc>,
    pub current_back_buffer: u32,
}

impl DxgiSwapchain {
    /// Create a flip-model swapchain for an HWND.
    pub fn new(desc: SwapchainDesc) -> Dx12Result<Self> {
        if desc.width == 0 || desc.height == 0 {
            return Err(Dx12Error::InvalidParam("swapchain extent must be non-zero"));
        }
        Ok(Self { desc: Some(desc), current_back_buffer: 0 })
    }

    /// Acquire the next back buffer index.
    pub fn next_back_buffer(&mut self) -> Dx12Result<u32> {
        match &self.desc {
            Some(d) => {
                self.current_back_buffer = (self.current_back_buffer + 1) % d.buffer_count;
                Ok(self.current_back_buffer)
            }
            None => Err(Dx12Error::InvalidParam("swapchain not configured")),
        }
    }

    /// Present the current frame.
    pub fn present(&self, _vsync: bool) -> Dx12Result<()> {
        Err(Dx12Error::NotImplemented("IDXGISwapChain4::Present"))
    }

    /// Resize buffers on window change.
    pub fn resize(&mut self, width: u32, height: u32) -> Dx12Result<()> {
        match &mut self.desc {
            Some(d) => {
                if width == 0 || height == 0 {
                    return Err(Dx12Error::InvalidParam("resize extent must be non-zero"));
                }
                d.width = width;
                d.height = height;
                Ok(())
            }
            None => Err(Dx12Error::InvalidParam("swapchain not configured")),
        }
    }
}
