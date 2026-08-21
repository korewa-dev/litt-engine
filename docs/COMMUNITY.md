﻿# Litt Engine — Community Rules

## Code of Conduct

- **Be respectful.** All contributors, maintainers, and users deserve a harassment-free environment.
- **No spam.** Keep discussions on-topic. Feature requests, bug reports, and PRs belong in their respective channels.
- **No proprietary code.** Do not submit code containing closed-source or licensed material you do not own.
- **AI agents are welcome.** Litt Engine is designed for AI-assisted development. AI-generated contributions are accepted if they follow these rules.

## Issue Reporting

- Use the issue template.
- Include reproduction steps, platform, and backend (Vulkan/DX12).
- Screenshots/logs are encouraged.
- Search before opening a duplicate.

## Feature Requests

- Explain the use case, not just the feature.
- Check ROADMAP.md — the feature may already be planned.
- Label your issue enhancement.

## Chat

- Keep questions in public channels.
- No asking for private help — the issue tracker is the record.
- No advertising other projects without maintainer approval.

## NPU Inference Policy

The engine prioritizes NPU-exclusive inference for AI workloads. Fallback paths (GPU/CPU) exist for compatibility but are **deprecated and will be removed in future releases**.

Pull requests that introduce new fallback paths for neural inference will be rejected unless they serve as a temporary compatibility measure with a documented removal timeline.

---

*Last updated: 2026*
