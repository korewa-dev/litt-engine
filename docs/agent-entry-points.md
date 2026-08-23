# AI Coding Tool Support Matrix

How each known AI coding tool picks up Litt Engine's agent rules.
Canonical source of truth: root `AGENTS.md`. Mirror files embed the critical
rules INLINE (The One Rule + routing + live-mode quickstart) because not every
tool reliably follows links.

**If you change the rules: update `AGENTS.md` AND all mirrors listed below.**

## Natively covered by `AGENTS.md` (no extra file needed)

| Tool | Reads | Status |
|------|-------|--------|
| OpenAI Codex CLI | `AGENTS.md` | covered |
| DeepSeek Harness | `AGENTS.md` (workspace) | covered - verified live in-session |
| OpenCode | `AGENTS.md` | covered |
| Google Jules | `AGENTS.md` | covered |
| Devin (Cognition) | `AGENTS.md` | covered |
| Amp (Sourcegraph) | `AGENTS.md` | covered |
| Amazon Q Developer | `AGENTS.md` | covered |
| Factory Droid | `AGENTS.md` | covered |
| Replit Agent | `AGENTS.md` | covered |
| Zed (recent versions) | `AGENTS.md` | covered + `.rules` mirror |

## Dedicated mirror files

| Tool | File in this repo |
|------|-------------------|
| Claude Code | `CLAUDE.md` |
| Gemini CLI (Google) | `GEMINI.md` |
| GitHub Copilot | `.github/copilot-instructions.md` |
| Cursor (current) | `.cursor/rules/litt-engine.mdc` (alwaysApply) |
| Cursor (legacy) | `.cursorrules` |
| Windsurf | `.windsurfrules` |
| Cline | `.clinerules` |
| Roo Code | `.roorules` |
| Zed (older) | `.rules` |
| Aider | `CONVENTIONS.md` (add via `--read CONVENTIONS.md` or /read) |
| Project IDX / Firebase Studio | `.idx/airules.md` |
| Kiro (AWS) | `.kiro/steering/litt-engine.md` (inclusion: always) |
| Continue.dev | `.continue/rules/litt-engine.md` |

## What every mirror contains

1. **The One Rule** - never write outside `Project/` without an explicit engine task.
2. **Routing** - develop -> LIVE MODE (`Project/live/`, start observer server, read
   `AI_RULES.md`, orient from `world_state.json` + `LIVE_LOG.md`, then build);
   separate game -> `Project/<name>/`; engine work -> `docs/ARCHITECTURE.md` first.
3. Pointer to the full protocol (`AGENTS.md`) and the math cookbook.
