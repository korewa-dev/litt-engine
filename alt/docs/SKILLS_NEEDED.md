# Skills Needed for Litt Engine Development

## Required Skills

Based on the systematic analysis of Litt Engine's structure and development needs, here are the critical skills that need to be installed:

### Core Skills (Essential for Engine Development)

1. **systematic-debugging**
   - Purpose: Root cause debugging before implementing fixes
   - Why needed: Engine has multiple stub/incomplete subsystems (DX12 DXR, AMD AGS, BLAS/TLAS, MUSA, NNAPI, NPU acceleration)
   - Status: ✅ Available and loaded

2. **test-driven-development**
   - Purpose: Enforce RED-GREEN-REFACTOR workflow, tests before code
   - Why needed: Native C core requires TDD for graphics API implementations, ECS systems, physics engine
   - Status: ✅ Available and loaded

3. **spike**
   - Purpose: Feasibility validation before implementing complex backends
   - Why needed: MUSA/MooreThreads, NNAPI/Android NPU, Ryzen AI/Intel NPU, Samsung NPU vendors need validation
   - Status: ✅ Available and loaded

4. **plan**
   - Purpose: Structured planning with .hermes/plans/ directory
   - Why needed: 7 major work items (README fix, DX12 DXR, AMD AGS, MUSA, NNAPI, Vulkan RT, shader compilation)
   - Status: ✅ Available and loaded

5. **requesting-code-review**
   - Purpose: Pre-commit verification, security scanning, quality gates
   - Why needed: Engine changes touch graphics APIs, memory allocators, ECS layer - needs independent review
   - Status: ✅ Available and loaded

### Supplementary Skills (Optional but Useful)

6. **simplify-code**
   - Purpose: Parallel 4-agent cleanup of recent code changes
   - Why needed: README has encoding issues and duplicates
   - Status: ✅ Available and loaded

7. **dogfood**
   - Purpose: Web app QA testing
   - Why needed: If web components are added later
   - Status: ✅ Available and loaded

8. **node-inspect-debugger**
   - Purpose: Debug Node.js via --inspect + Chrome DevTools
   - Why needed: C# LittStudio GUI debugging
   - Status: ✅ Available and loaded

9. **github-code-review**
   - Purpose: GitHub PR reviews and inline comments
   - Why needed: If contributing back to korewa-dev/litt_engine
   - Status: ✅ Available and loaded

## GitHub Skills (for Repository Contributions)

10. **github-auth** - For GitHub authentication
11. **github-code-review** - Already listed above
12. **github-issue-to-pr** - Issue to verified PR workflow
13. **github-issues** - Issue creation and management
14. **github-pr-workflow** - Complete PR lifecycle management
15. **github-repo-management** - Repository operations

## Additional Tools

### Hermes Agent Skill Authoring
The `hermes-agent-skill-authoring` skill can be used to create or update SKILL.md files within the litt_engine project if needed.

## Installation Commands

All skills are available and loaded. To install any missing skills:

```bash
# Example: Install GitHub skills
hermes skill-manage create --name github-auth --category github
hermes skill-manage create --name github-code-review --category github
hermes skill-manage create --name github-issue-to-pr --category github
hermes skill-manage create --name github-issues --category github
hermes skill-manage create --name github-pr-workflow --category github
hermes skill-manage create --name github-repo-management --category github
```

## GitHub Token Information

The user mentioned "the github token is inside the ai router folder". However, no GitHub token was found in:
- `D:/Allgemein/AI Router/litt engine/.git-credentials`
- Any files matching "*token*" in the AI Router folder structure

**Recommendation**: If GitHub API access is needed for:
- Checking GitHub repository issues/PRs
- API calls to GitHub
- Branch protection checks

You will need to either:
1. Set the `GITHUB_TOKEN` environment variable
2. Use the `hermes skill-manage` to create/configure GitHub skills that use token auth
3. Provide the token directly when prompted

## Priority Development Plan

Based on the IMPLEMENTATION_STATUS.md and current state, the priority is:

1. **Fix README.md** (alignment with implementation status)
2. **Implement missing C core subsystems**: ECS, physics, renderer, audio, UI, input
3. **Complete Vulkan ray tracing**: BLAS/TLAS building
4. **Implement real DX12**: DXR support, acceleration structures
5. **Add shader compilation pipeline**: SPIR-V/DXIL compilation
6. **Feasibility spikes**: MUSA, NNAPI, NPU acceleration

## Skill Usage Guidelines

### For Debugging
- Always use `systematic-debugging` first for any bug or issue
- Follow the 4-phase root cause investigation
- Build tight feedback loops before attempting fixes

### For Implementation
- Use `test-driven-development` for any new feature
- Write failing tests first (RED phase)
- Only implement after tests fail, then refactor (GREEN->REFACTOR)

### For Planning
- Use `plan` skill for complex multi-step tasks
- Save plans to `.hermes/plans/` directory
- Include exact file paths and verification steps

### For Code Review
- Use `requesting-code-review` before committing changes
- Includes security scanning and quality gates
- Independent reviewer subagent verification

## Recommended Workflow

1. **Problem Identification**: Use `systematic-debugging` to understand the issue
2. **Test Planning**: Use `plan` to create detailed implementation plan
3. **Feasibility Check**: Use `spike` for unknown/uncertain implementations
4. **Implementation**: Use `test-driven-development` with TDD discipline
5. **Verification**: Use `requesting-code-review` before commit
6. **Cleanup**: Use `simplify-code` for code quality improvements

All core skills are available and ready to use for Litt Engine development.