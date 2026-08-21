# AMD AGS (AMDGPU Services) - Real Implementation

This crate provides Rust bindings for the official AMD AGS library.

## Features

- GPU enumeration and detection
- Power management (profiles, limits)
- Fan control
- Performance profiling
- Thermal monitoring
- Driver information

## Usage

```rust
use litt_ags::{AGSContext, AGSPowerProfile, AGSPerformanceLevel};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let mut context = AGSContext::new()?;
    
    // Get GPU count
    let count = context.adapter_count();
    println!("Found {} AMD GPUs", count);
    
    // Get adapter info
    let info = context.get_adapter_info(0)?;
    println!("GPU: {}", info.adapter_name());
    
    // Set performance mode
    context.set_power_profile(0, AGSPowerProfile::AGS_POWER_PROFILE_FORCE_HIGH)?;
    
    // Monitor thermals
    let thermal = context.get_thermals(0)?;
    println!("Temperature: {}°C", thermal.CurrentTemperature);
    
    Ok(())
}
```

## Requirements

- AMD Radeon GPU with Adrenalin driver
- Windows or Linux
- Admin privileges for power/fan control

## License

MIT - Based on AMD AGS SDK
