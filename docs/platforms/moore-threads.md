# Moore Threads GPU Support

## Overview
Moore Threads (摩尔线程) is a Chinese GPU manufacturer with Vulkan 1.2/1.3 support via their MUSA driver.

## Supported Cards
- MTT S80 (consumer)
- MTT S3000 (professional)
- S4000 (datacenter)

## Vulkan Extension Support
| Extension | Status |
|-----------|--------|
| VK_KHR_ray_tracing_pipeline | Partial (driver-dependent) |
| VK_KHR_acceleration_structure | Partial (driver-dependent) |
| VK_EXT_robustness2 | Supported |
| VK_EXT_extended_srgb | Supported |
| VK_EXT_pipeline_cache_control | Supported |

## Driver Notes
- MUSA driver on Windows: Vulkan 1.2, partial RT support
- MUSA driver on Linux: Vulkan 1.2, better RT support
- Use `VK_EXT_robustness2` for robust buffer access
- Shader cache: Enable `VK_EXT_pipeline_cache_control`

## Environment Variables
```bash
# Linux MUSA driver
export MUSA_LOG_LEVEL=warning
export MUSA_RT_ENABLE=1        # Enable ray tracing
export MUSA_SHADER_CACHE=1     # Enable shader caching
```

## Performance Tips
1. Use smaller wave sizes (32) for compute shaders
2. Minimize push constant usage (MTT has limited push constant space)
3. Use VK_EXT_robustness2 for safer buffer access
4. Enable pipeline cache to avoid recompilation
