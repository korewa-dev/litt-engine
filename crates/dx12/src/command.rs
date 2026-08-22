//! DX12 command system -- queues, allocators, and command lists

use super::*;

/// Command allocator for a frame
#[derive(Debug)]
pub struct CommandAllocator {
    pub allocator: *mut winapi::um::d3d12::ID3D12CommandAllocator,
}

/// Command list for recording graphics commands
#[derive(Debug)]
pub struct CommandList {
    pub list: *mut winapi::um::d3d12::ID3D12GraphicsCommandList,
}

/// Fence for synchronization
#[derive(Debug)]
pub struct Fence {
    pub fence: *mut winapi::um::d3d12::ID3D12Fence,
    pub current_value: u64,
}

/// Command context manages the ring of allocators and fences
#[derive(Debug)]
pub struct CommandContext {
    pub device: *mut winapi::um::d3d12::ID3D12Device,
    pub graphics_queue: *mut winapi::um::d3d12::ID3D12CommandQueue,
    pub allocators: Vec<CommandAllocator>,
    pub fences: Vec<Fence>,
    pub fence_event: *mut std::ffi::c_void,
    pub current_frame: u32,
    pub signal_value: u64,
}

impl CommandContext {
    /// Create a new command context with the given buffer counts
    pub fn new(
        device: *mut winapi::um::d3d12::ID3D12Device,
        graphics_queue: *mut winapi::um::d3d12::ID3D12CommandQueue,
        frame_count: u32,
    ) -> Result<Self, Dx12Error> {
        unsafe {
            let mut allocators = Vec::with_capacity(frame_count as usize);
            let mut fences = Vec::with_capacity(frame_count as usize);

            for _ in 0..frame_count {
                let mut allocator: *mut winapi::um::d3d12::ID3D12CommandAllocator = std::ptr::null_mut();
                let hr = (*device).CreateCommandAllocator(
                    winapi::um::d3d12::D3D12_COMMAND_LIST_TYPE_GRAPHICS,
                    &winapi::um::d3d12::IID_ID3D12CommandAllocator,
                    &mut allocator as *mut _ as *mut _,
                );
                if winapi::shared::winerror::FAILED(hr) {
                    return Err(Dx12Error::ResourceAllocation("Command allocator creation failed".into()));
                }
                allocators.push(CommandAllocator { allocator });
            }

            for _ in 0..frame_count {
                let mut fence: *mut winapi::um::d3d12::ID3D12Fence = std::ptr::null_mut();
                let hr = (*device).CreateFence(0, winapi::um::d3d12::D3D12_FENCE_FLAG_NONE, 
                    &winapi::um::d3d12::IID_ID3D12Fence, &mut fence as *mut _ as *mut _);
                if winapi::shared::winerror::FAILED(hr) {
                    return Err(Dx12Error::ResourceAllocation("Fence creation failed".into()));
                }
                fences.push(Fence { fence, current_value: 0 });
            }

            let event = winapi::Win32::Foundation::CreateEventW(
                std::ptr::null_mut(),
                winapi::Win32::Foundation::TRUE,
                winapi::Win32::Foundation::FALSE,
                std::ptr::null(),
            );

            Ok(CommandContext {
                device,
                graphics_queue,
                allocators,
                fences,
                fence_event: event as *mut std::ffi::c_void,
                current_frame: 0,
                signal_value: 0,
            })
        }
    }

    /// Reset the command allocator for the current frame
    pub fn reset_allocator(&mut self) -> Result<&mut CommandAllocator, Dx12Error> {
        unsafe {
            let idx = self.current_frame as usize % self.allocators.len();
            let hr = (*self.allocators[idx].allocator).Reset();
            if winapi::shared::winerror::FAILED(hr) {
                return Err(Dx12Error::ResourceAllocation("Command allocator reset failed".into()));
            }
            Ok(&mut self.allocators[idx])
        }
    }

    /// Create a new command list
    pub fn create_command_list(&mut self) -> Result<CommandList, Dx12Error> {
        unsafe {
            let idx = self.current_frame as usize % self.allocators.len();
            let mut list: *mut winapi::um::d3d12::ID3D12GraphicsCommandList = std::ptr::null_mut();
            let hr = (*self.device).CreateCommandList(
                0,
                winapi::um::d3d12::D3D12_COMMAND_LIST_TYPE_GRAPHICS,
                self.allocators[idx].allocator,
                std::ptr::null(), // PSO will be set later
                &winapi::um::d3d12::IID_ID3D12GraphicsCommandList,
                &mut list as *mut _ as *mut _,
            );
            if winapi::shared::winerror::FAILED(hr) {
                return Err(Dx12Error::PipelineCreation("Command list creation failed".into()));
            }
            Ok(CommandList { list })
        }
    }

    /// Signal the fence and wait for completion
    pub fn signal_and_wait(&mut self) -> Result<(), Dx12Error> {
        unsafe {
            self.signal_value += 1;
            let hr = (*self.graphics_queue).Signal(
                self.fences[self.current_frame as usize % self.fences.len()].fence,
                self.signal_value,
            );
            if winapi::shared::winerror::FAILED(hr) {
                return Err(Dx12Error::DxError("Signal failed".into()));
            }
            // Wait on CPU for simplicity (in production, use async)
            let fence = self.fences[self.current_frame as usize % self.fences.len()].fence;
            if (*fence).GetCompletedValue() < self.signal_value {
                (*fence).SetEventOnCompletion(self.signal_value, self.fence_event);
                winapi::Win32::System::Threading::WaitForSingleObject(self.fence_event, winapi::Win32::System::Threading::INFINITE);
            }
            Ok(())
        }
    }

    /// Execute the command list
    pub fn execute(&mut self, list: &mut CommandList) -> Result<(), Dx12Error> {
        unsafe {
            let command_lists: [*mut winapi::um::d3d12::ID3D12CommandList; 1] = [list.list];
            (*self.graphics_queue).ExecuteCommandLists(1, command_lists.as_ptr() as *mut _);
            Ok(())
        }
    }

    /// Advance to the next frame
    pub fn next_frame(&mut self) {
        self.current_frame = (self.current_frame + 1) % (self.allocators.len() as u32);
    }
}

impl Drop for CommandContext {
    fn drop(&mut self) {
        unsafe {
            if !self.fence_event.is_null() {
                winapi::Win32::Foundation::CloseHandle(self.fence_event);
            }
        }
    }
}
