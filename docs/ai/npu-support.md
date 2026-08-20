# NPU (Neural Processing Unit) Support

Litt Engine supports NPU acceleration for AI-powered rendering tasks including denoising, upscaling, and frame generation.

## Supported NPUs

### Desktop/Laptop NPUs
| NPU | Vendor | Architecture | TOPS (INT8) |
|-----|--------|-------------|-------------|
| Ryzen AI XDNA 2 | AMD | XDNA 2 | 50 TOPS |
| Ryzen AI (first gen) | AMD | XDNA 1 | 25 TOPS |
| Intel AI Boost | Intel | Movidius VPU | 48 TOPS |
| Apple Neural Engine | Apple | Custom | 15.8 TOPS |

### Mobile NPUs
| NPU | Vendor | TOPS (INT8) |
|-----|--------|-------------|
| Exynos NPU (RDNA 2 iGPU) | Samsung | 12 TOPS |
| NPU | Vendor | TOPS (INT8) |
|-----|--------|-------------|
| Exynos NPU | Samsung | 12 TOPS |
| Hexagon | Qualcomm | 15 TOPS |
| APU | MediaTek | 10 TOPS |
| Da Vinci NPU | Huawei Kirin | 8 TOPS |
| Mali-NPU | ARM | 6 TOPS |

### RISC-V AI Accelerators
| Accelerator | Use Case |
|-------------|----------|
| Sophgo CV1800/CV1835 | Edge AI inference |
| microchip/mpfs | FPGA-based AI |
| vectortile/vt84 | RISC-V vector AI |

## How It Works

The NPU module detects available NPUs via Vulkan physical device properties:

\`\`\`rust
use litt_fidelityfx::npu::*;

let npus = detect_npus(&device, physical_device);
if npus[0].available {
    match npus[0].vendor {
        NpuVendor::AmdXdna => println!("Ryzen AI detected"),
        NpuVendor::IntelAiBoost => println!("Intel AI Boost detected"),
        NpuVendor::Kirin => println!("Huawei Kirin NPU detected"),
        NpuVendor::RiscvAi => println!("RISC-V AI accelerator detected"),
        _ => println!("Unknown NPU: {}", npus[0].name),
    }
}
\`\`\

## NPU Modes

| Mode | Value | Description |
|------|-------|-------------|
| Disabled | 0 | No NPU acceleration |
| Auto | 1 | Use NPU when beneficial |
| Forced | 2 | Force NPU for all denoising |
| Hybrid | 3 | NPU for denoise, GPU for RT |

## Precision Support

| Bit | Precision | Description |
|-----|-----------|-------------|
| 0 | FP16 | 16-bit floating point |
| 1 | INT8 | 8-bit integer |
| 2 | INT4 | 4-bit integer |
| 3 | BF16 | Bfloat16 |

## Environment Variables

\`\`\bash
# Force NPU mode
export LIT_NPU_MODE=3          # Hybrid
export LIT_NPU_PRECISION=7     # FP16 + INT8 + BF16
export LIT_NPU_FALLBACK=1      # Fallback to GPU if NPU fails

# Disable NPU
export LIT_NPU_MODE=0
\`\`\

## Performance Notes

1. **First frame overhead** — NPU compilation can add 1-3s to first frame
2. **Transfer overhead** — Copying data to/from NPU has latency (~0.5ms)
3. **Best use case** — NPU excels at batch denoising (multiple frames)
4. **Fallback** — Always fall back to GPU if NPU is unavailable or slow
