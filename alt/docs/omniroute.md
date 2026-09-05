# OmniRoute — AI Router Integration

OmniRoute (`C:\Users\roika\AppData\Roaming\npm\omniroute`) is the local smart
AI router (auto-fallback across providers) that ships with this machine. The
server runs on `127.0.0.1:20128` (CLI: `omniroute doctor` to verify).

## What agents use it for in litt

- **Game design briefs** — enemy rosters, objectives, zone naming for
  `enrich_game.py` brief JSONs (`omniroute chat "<design prompt>"`).
- **Content passes** — flavor text, NOTES.md polish, lore lines.
- **Routing checks** — `omniroute simulate` before long generations.

## Quick reference

```bash
omniroute doctor                 # health check (server, db, credentials)
omniroute chat "prompt"          # one-shot completion through the router
omniroute simulate "prompt"      # show which provider would answer
omniroute serve / stop / restart # server lifecycle
```

## Engine workflow position

The design pipeline in `AGENTS.md` is deterministic (seeds + tools). OmniRoute
sits BEFORE it: use it to draft the *brief.json* content (roster, waves,
objectives), then run the fixed pipeline so output stays reproducible.
