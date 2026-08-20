# AMD AGS (AMD GPU Services)

> AMD AGS provides runtime control over GPU power, performance, and monitoring.

**Status:** Not implemented — no references in codebase.

## What is AMD AGS?

AMD GPU Services (AGS) is a C API that allows applications to:
- Query GPU information (temperature, power, clock speeds)
- Control GPU power profiles (performance vs power saving)
- Monitor GPU utilization in real-time
- Configure display settings per-GPU

## Planned Integration

```rust
pub struct AmdAgs {
    pub temperature: f32,       // GPU temperature (Celsius)
    pub power_watts: f32,       // Current power draw
    pub clock_mhz: u32,         // Current GPU clock
    pub utilization: f32,       // GPU utilization (0.0-1.0)
    pub power_profile: AmdPowerProfile,
}

pub enum AmdPowerProfile {
    LowPower,
    Balanced,
    HighPerformance,
    Custom,
}
```

## Use Cases

1. **Thermal management** — Reduce ray tracer samples when GPU exceeds thermal threshold
2. **Battery optimization** — Auto-switch to lower FSR quality on laptop battery
3. **Performance monitoring** — Report GPU metrics to debug overlay

## Roadmap

### Short-term
- [ ] Add AMD AGS dependency detection (check for ags.lib / libags.so)
- [ ] Wrap AGS queries in `GraphicsBackend` trait

### Mid-term
- [ ] Integrate thermal throttling into render loop
- [ ] Add power-profile-aware FSR quality selection

### Hardware-Specific
- **RDNA / AMD:** Full AGS support on Windows and Linux (RADV)
- **Other vendors:** Skip AGS initialization, use fallback metrics
