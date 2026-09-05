# RISC-V Platform Support

> RISC-V Linux with RVV vectorization and Vortex GPU support.

**Status:**  Planned -- Phase 11 of [../../ROADMAP.md](../../ROADMAP.md).

## Supported Processors

| Processor | Use Case | AI Accelerator |
|-----------|----------|----------------|
| Sophgo CV1800/CV1835 | Edge AI inference | CVITEK NPU |
| Microchip MPFS | FPGA-based AI | Soft CPU |
| VectorTile VT84 | RISC-V vector AI | RVV |

## Vulkan Support

- MESA driver with Vulkan 1.2/1.3
- Vortex GPU driver (proprietary)
- SwiftShader fallback for software rendering

## RVV Vectorization

RISC-V Vector Extension (RVV) provides variable-length SIMD:

```cpp
#[cfg(target_arch = "riscv64")]
unsafe fn broadphase_rvv(bodies: &mut [PhysicsBody]) {
    // RVV vectorized spatial hash
    // Handles variable-width vectors dynamically
}
```

## Limitations

- No NPU on most RISC-V boards (use CPU fallback)
- Limited Vulkan driver maturity
- No ray tracing hardware (software fallback)
- No DX12 (Vulkan only)

## Roadmap

### Short-term
- [ ] MESA Vulkan 1.3 validation
- [ ] RVV math library integration

### Hardware-Specific
- **RISC-V:** RVV vectorized spatial hash, software ray-cast fallback, CPU-only UI rendering

