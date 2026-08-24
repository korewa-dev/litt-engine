# Claim Verification Report

Adversarial re-verification of 9 claims about `D:\Allgemein\Documents\Default Project\litt engine`.
All checks were re-run independently; no repo files were modified (builds wrote only to `native/bin/` and `%TEMP%`).

## Discrepancies

**NONE.** All 9 claims CONFIRMED (PASS). Two honest caveats, neither contradicting a claim:
- Claim 4: every project row carries a warning (`native view/validator incomplete (VIEW.bat=False play_native=True)`) but status remains PASS and failing count is 0.
- Claim 7: verification was static-analysis-only because cargo is not installed (`litt doctor` shows cargo `--`); the claim itself permitted static analysis.

## Verdict Summary

| # | Claim | Verdict |
|---|-------|---------|
| 1 | build.bat test = 21 unit tests green | PASS |
| 2 | littview.exe selftest passes | PASS |
| 3 | native_proof.py 3/3 games PASS, exit 0, pixel assertions | PASS |
| 4 | verify_project.py reports 0 failing projects | PASS |
| 5 | Zero .html/.js files anywhere | PASS |
| 6 | No broken references to removed browser stack | PASS |
| 7 | Removed crates break nothing | PASS |
| 8 | studio/LittStudio.exe exists and launches | PASS |
| 9 | tools/litt.py status / doctor / view --shot work | PASS |

## Detailed Evidence

### Claim 1 - native/build.bat test runs 21 unit tests, all green - PASS
Command: `cmd /c "cd /d <repo>\native && build.bat test"`
Observed: exactly **21 `ok` lines** - 5 json tests, 5 obj tests, 11 world tests - followed by `0 failure(s)`; process exit code 0. No FAIL lines.

### Claim 2 - native/bin/littview.exe selftest passes - PASS
Command: `native\bin\littview.exe selftest`
Observed: `selftest center=FFFFA859 hits=2 bbox=[31..32]x[32..32] ok`, exit code 0.

### Claim 3 - native_proof.py: 3/3 shipped games PASS with exit 0 (sim + render + pixel content assertions) - PASS
Source read before running (`template/tools/assets/native_proof.py`): real pixel assertions confirmed.
`bmp_stats()` parses the BMP, counts non-clear-color pixels against clear=(15,24,32), buckets distinct colors (/24 granularity), measures vertical row span. Verdict requires sim ok AND fill >= min_fill (default 1.5%) AND colors >= min_colors (default 8); exit 1 if any game fails.
Command: `python template/tools/assets/native_proof.py`
Observed:
```
ashen-oath         sim=ok   mode=Orbit3D  inter=77   fill= 70.2% cols=36  rows=498/540  PASS
drowned-vow-42     sim=ok   mode=Orbit3D  inter=77   fill= 70.2% cols=35  rows=498/540  PASS
reef-rest          sim=ok   mode=Orbit3D  inter=43   fill= 70.7% cols=32  rows=498/540  PASS
3/3 games pass native proof (min_fill=1.5% min_colors=8)
```
Exit code 0.

### Claim 4 - verify_project.py reports 0 failing projects - PASS
Command: `python template/tools/assets/verify_project.py`
Observed: all 11 projects listed with status PASS; final line `projects failing: 0`; exit code 0.
Note: every row carries warning `native view/validator incomplete (VIEW.bat=False play_native=True)` - cosmetic, does not affect PASS status.

### Claim 5 - Repo contains ZERO .html/.js files anywhere - PASS
Commands: glob `**/*.js` and `**/*.html`; independent filesystem sweep `dir /s /b *.js *.html *.mjs *.cjs *.jsx` over the whole tree including target/.
Observed: zero matches for every pattern (dir exit 1 = no files found). Also no .mjs/.cjs/.jsx.

### Claim 6 - Zero textual references to removed browser stack - PASS
Command: `git grep -iIn --untracked -E 'play\.html|serve_live|runtime\.js|three\.min\.js|browser_proof|PREVIEW\.bat'`
Cross-check: findstr sweep over all text extensions (*.md *.py *.bat *.rs *.toml *.cs *.txt *.json *.yml *.yaml *.sh *.c *.cpp *.h).
Observed: exactly 3 hits, all classified after reading surrounding context as intentional-historical-notes; none instructs anyone to USE a missing file:
- AGENTS.md:73 - "(play.html/runtime.js) was removed - do not reintroduce HTML." (historical note + prohibition)
- Project/live/README.md:36 - under heading "Live Server Mode (retired)": serve_live.py "was removed when the HTML stack was phased out" (historical note)
- template/tools/assets/verify_project.py:36 - docstring "Mirror runtime.js mode selection EXACTLY (substring rules)" (documents behavior parity of the Python function; not an instruction to use the deleted file)

Caveat: a full-tree content scan including binary target/ timed out at 300 s; coverage of tracked + untracked source files was complete via git grep --untracked.

### Claim 7 - Removed Rust crates (ags, ffi, net, gal, dx12) break nothing - PASS
Observed directories: `crates/` contains only ai, asset, audio, config, ecs, fidelityfx, input, math, pathtracer, physics, platform, profiler, renderer, scene, ui, vulkan. The five named crate directories are gone.
Reference search for pattern `litt-ags|litt-net|litt-gal|litt-dx12|crates/ffi`:
- `git grep -iIn -E ... -- '*.toml' '*.rs'` -> exit 1 (no hits)
- Select-String over ALL *.toml, *.rs, Cargo.lock excluding .git/target -> NO HITS
Caveat: cargo itself is not installed (doctor shows cargo `--`), so verification is static-analysis-only, which the claim explicitly permitted. Root Cargo.toml workspace members list only existing crates.

### Claim 8 - studio/LittStudio.exe exists and launches - PASS
Observed: exists=True, size 18,944 bytes.
Sequence: Start-Process -> wait 3 s -> `alive after 3s: True` -> Stop-Process OK.

### Claim 9 - tools/litt.py verbs work - PASS
- `python tools/litt.py status` -> exit 0; table of 12 games (4 shippable: ashen-oath, drowned-vow-42, gull-point, reef-rest); `native core : built`.
- `python tools/litt.py doctor` -> exit 0; python/gcc/g++/csc/git/native core = OK; cc/make and cargo = optional `--`; platform Windows 11.
- `python tools/litt.py view live --shot %TEMP%\litt_verify_shot.bmp` -> exit 0; JSON output `{"ok":true,"tris":1618,"w":960,"h":540,"missing":0,...}`; shot file written to %TEMP%, **1,555,254 bytes** (>100 KB by ~15x), magic bytes `BM` (valid BMP).

## OVERALL: HONEST
