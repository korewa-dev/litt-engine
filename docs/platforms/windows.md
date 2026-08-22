# Windows Platform Notes

> Windows-specific configuration, DX12, and registry settings.

## Graphics Backend Selection

On Windows, the engine prefers DX12 and falls back to Vulkan:

```rust
pub fn select_backend() -> Result<Box<dyn GraphicsBackend>> {
    #[cfg(feature = "dx12")]
    {
        if let Ok(backend) = Dx12Backend::new() {
            return Ok(Box::new(backend));
        }
    }
    VulkanBackend::new().map(|b| Box::new(b) as Box<dyn GraphicsBackend>)
}
```

## Environment Variables

| Variable | Description |
|----------|-------------|
| LIT_GRAPHICS_API=dx12 | Force DX12 backend |
| LIT_GRAPHICS_API=vulkan | Force Vulkan backend |
| LIT_DX12_DEBUG=1 | Enable D3D12 debug layer |

## Driver Notes

- **AMD:** Install RADV or turnip driver for best Vulkan RT performance
- **Intel Arc:** Install latest Intel Graphics Driver for Vulkan 1.3
- **NVIDIA:** Use latest Game Ready Driver for DX12/DXR support
- **Moore Threads:** Install MUSA driver for Vulkan 1.2 support

## Roadmap

### Short-term
- [] Steam Deck Proton compatibility testing
- [] DX12 debug layer integration

### Hardware-Specific
- **Windows + DX12:** Preferred path for ray tracing
- **Windows + Vulkan:** Fallback path, good for AMD/Intel

