# LITT ENGINE — POLICIES
### Version 1.6 · AI Safety · Contributions · Donations

---

## AI Safety Policy

1. **No harmful training.** AI Agents built on the Engine must not be trained for harmful, malicious, deceptive, or unethical behavior.
2. **No surveillance or discrimination.** The Engine must not power surveillance systems, profiling, or discriminatory automation.
3. **Monitored operation.** Agent behavior during training-in-the-loop scenarios should remain observable and auditable — the deterministic replay system (`litt::replay`) exists partly for this purpose: every session can be recorded, hashed, and verified.
4. **Headless safety.** Agents operating headlessly (GAL `NullDevice`, CI runs) inherit the same rules as GPU-backed runs; absence of rendering does not relax policy.

## Contribution Policy

To qualify as a **meaningful contributor** (required to sell AI models trained with the Engine, see LICENSE §7), at least one of the following must be merged or demonstrably adopted upstream:

- Code (features, fixes, performance)
- Documentation or guides
- Bug reports with reproducible detail
- Testing (unit, integration, platform)
- Infrastructure (CI, build, release tooling)
- AI improvements (agents, benchmarks, RL environments)

Self-certification is not enough — contributions must exist in the public project history.

## Donation Policy

| Target | Cap |
|---|---|
| Engine itself | **max 1 € per donor per year** |
| Plugins / GUIs / editors / tools | **max 1 € per donor per month** |
| Games built with the Engine | unlimited |
| Game funding / crowdfunding | unlimited |

Any structure designed to exceed these caps (multiple payment channels, tiers, subscriptions, tokens, bundled services) is a License violation (§11).
