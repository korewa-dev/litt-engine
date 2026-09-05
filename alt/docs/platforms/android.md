# Android Platform Support

> Adreno, Mali, and PowerVR GPU support via Vulkan.

## Supported GPUs

| GPU | Vendor | Vulkan Version | NPU |
|-----|--------|---------------|-----|
| Adreno 7xx | Qualcomm | 1.2/1.3 | Hexagon (15 TOPS) |
| Mali-G7xx | ARM | 1.2 | Mali-NPU (6 TOPS) |
| PowerVR 1000 | Imagination | 1.1 | -- |
| Samsung RDNA iGPU | Samsung | 1.2 | Exynos NPU (12 TOPS) |

## NPU Acceleration

Android NPUs are accessed via NNAPI (Neural Networks API):

```cpp
pub enum NpuVendor {
    QualcommHexagon,  // Adreno + Hexagon
    MaliNPU,          // ARM Mali + Mali-NPU
    SamsungNpu,       // Exynos RDNA iGPU + NPU
}
```

## Performance Tips

1. Target Vulkan 1.1 minimum for broad compatibility
2. Use fixed-step physics for battery conservation
3. Reduce ray tracer bounces on mobile (2 max)
4. Use FSR Quality preset for best quality/performance balance
5. Monitor thermal throttling -- reduce frame rate if GPU > 80C

## Roadmap

### Short-term
- [ ] Android build verification on Adreno 740
- [ ] NNAPI integration for Hexagon NPU

### Hardware-Specific
- **Adreno:** Vulkan 1.3, Hexagon DSP for NPU inference
- **Mali:** Vulkan 1.2, Mali-NPU for denoising
- **PowerVR:** Limited Vulkan support, consider software fallback

