# LITT ENGINE — COMMUNITY GUIDELINES
### Version 1.6 · Behavior rules · Developer rules · Responsibilities

---

## Part I — Community Guidelines

### 1. Respect the AI-Exclusive Philosophy
Do not attempt to turn Litt Engine into a human-driven engine. Humans prompt; agents build. PRs that add human-facing editing workflows contradict the project's purpose and will be rejected.

### 2. Respect Open-Source
All forks must remain open-source. No closed forks. No proprietary extensions. No "open-core" tricks.

### 3. No Commercial Abuse
No selling the engine. No selling forks. No monetized plugins. No monetized add-ons. Games built *with* the engine are exempt — sell those freely.

### 4. No Loophole Exploitation
No bypassing donation caps. No "premium features." No "support tiers." No "early access." No "pay-what-you-want" above the caps. If a revenue model needs a lawyer to distinguish it from selling the engine, it's a violation.

### 5. Respectful Conduct
No harassment. No discrimination. No trolling. No sabotage of the ecosystem (malicious PRs, supply-chain poisoning, license-laundering forks).

---

## Part II — Developer Guidelines

1. **Tools must be AI-exclusive.** Editors, GUIs, and plugins must not enable human editing of scenes, assets, or gameplay.
2. **Tools must be free.** No selling tools. No paid tiers.
3. **Tools must be open-source.** Same license family or less restrictive.
4. **Donation limits.** Tools may accept max **1 € per donor per month**.
5. **Model-sales contribution requirement.** Selling AI models trained with the Engine requires meaningful upstream contribution (see [POLICY.md](./POLICY.md)).

---

## Part III — Developer Responsibilities

If you maintain a fork, tool, or integration, you are responsible for:

| Responsibility | What it means in practice |
|---|---|
| **License propagation** | Your fork/tool ships the Litt Engine License (or less restrictive), unmodified in effect. |
| **AI-exclusive workflows** | Your tool's primary interface is agent-operable (headless, scriptable, text-native). No GUI-only features. |
| **Determinism discipline** | Simulations stay reproducible: fixed timestep, recorded inputs, verifiable state hashes. Breaking determinism is a bug, not a feature. |
| **Text-native data** | Worlds, configs, and manifests stay diffable by agents (JSON scenes, asset indexes). Binary blobs need a documented, loadable format. |
| **Documentation truth** | Docs must match code. Stale docs are treated as bugs (agents read docs, not intentions). |
| **Test coverage** | New engine features land with unit tests; CI must stay green per-crate. |
| **Upstream goodwill** | Meaningful improvements flow back upstream rather than being hoarded in forks. |
| **Safety** | Agents you build on the Engine follow the [AI Safety Policy](./POLICY.md). |

---

## Reporting Violations

Open a public issue (if safe) or contact the maintainer directly. Reports of closed forks, monetized tools, or unsafe agent deployments are reviewed against LICENSE, TERMS, and these guidelines.
