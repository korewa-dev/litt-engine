#!/usr/bin/env python3
from pathlib import Path
import sys

required = {
    "README.md": [
        "Vulkan/DX12/OpenGL/Metal generic facade | Experimental/unavailable",
    ],
    "alt/docs/SUPPORTED_RUNTIME.md": [
        "generic Vulkan/DX12/OpenGL/Metal renderer facade backends that return unavailable",
    ],
    "alt/docs/graphics-api/graphics-api-status.md": [
        "| Vulkan | **GREEN fail-fast contract** | **Unavailable** |",
        "| DirectX 12 | **GREEN fail-fast contract** | **Unavailable** |",
        "Physical Litt certification",
    ],
    "alt/docs/platforms/windows.md": [
        "Generic DX12 and Vulkan device paths are unavailable today.",
    ],
}

forbidden = {
    "alt/docs/platforms/windows.md": [
        "engine prefers DX12 and falls back to Vulkan",
        "Preferred path for ray tracing",
    ],
    "alt/docs/graphics-api/fidelityfx.md": [
        "Litt Engine integrates the [AMD FidelityFX SDK]",
        "## GPU Support Matrix",
    ],
    "alt/docs/rendering/README.md": [
        "| AMD RDNA  | Vulkan",
        "| Intel Arc | DX12 + Vulkan",
    ],
}

errors = []
for filename, needles in required.items():
    text = Path(filename).read_text(encoding="utf-8")
    for needle in needles:
        if needle not in text:
            errors.append(f"{filename}: missing required capability boundary: {needle}")

for filename, needles in forbidden.items():
    text = Path(filename).read_text(encoding="utf-8")
    for needle in needles:
        if needle in text:
            errors.append(f"{filename}: stale unsupported capability claim: {needle}")

if errors:
    print("\n".join(errors), file=sys.stderr)
    raise SystemExit(1)

print("GPU capability claim contract: OK")
