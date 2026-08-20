# Steam Deck Support

> Steam Deck-specific configuration, controller mapping, and Proton compatibility.

## Hardware

- **GPU:** AMD RDNA 2 (8 CU, 1.5 TFLOPS)
- **CPU:** AMD Zen 2 (4 cores / 8 threads)
- **RAM:** 16 GB LPDDR5 (shared with GPU)
- **Display:** 1280x800 LCD, 60Hz
- **Battery:** 40 Wh

## Controller Mapping

| Button | Default Action |
|--------|---------------|
| A | Jump / Confirm |
| B | Shoot / Cancel |
| X | Interact |
| Y | Sprint |
| LB | Melee / Block |
| RB | Secondary weapon |
| L Stick | Move |
| R Stick | Look / Camera |
| D-pad | Menu navigation |
| Touchpad (left) | Touch aim / Map |
| Touchpad (right) | Radial menu |
| Gyro (left) | Yaw/pitch look |
| Gyro (right) | Fine aim |
| Start | Pause menu |
| View | Screenshot |

## Power Profiles

| Profile | TDP | FSR Quality | Ray Bounces |
|---------|-----|-------------|-------------|
| Battery saver | 5W | UltraPerformance | 1 |
| Balanced | 10W | Quality | 2 |
| Performance | 15W | Balanced | 3 |

## Proton Compatibility

For DX12 titles, Proton Experimental provides the best compatibility:

```bash
steamrt_proton=proton Experimental
PROTON_ENABLE_NVAPI=1
```

## Roadmap

### Short-term
- [ ] Steam Deck controller overlay integration
- [ ] Power profile aware FSR quality selection

### Hardware-Specific
- **RDNA 2:** Wave64 for RT, wave32 for compute
- **AMD NPU (X DNA 2):** 50 TOPS for NPC behavior inference
- **RADV:** Open-source Vulkan driver, excellent RT support
