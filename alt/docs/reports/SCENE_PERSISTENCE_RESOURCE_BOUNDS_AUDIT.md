# Scene persistence resource-bounds audit

Date: 2026-09-21
Base: `main` at `f3a71a79356dcf319bc8bcf2185df89d54483474`
Working branch: `scene-persistence-bounds-20260921`

## Revalidation

The current `Scene::deserializeFromJson` still parses the complete caller-provided JSON before applying semantic validation. It rejects malformed structures, duplicate IDs, invalid parents, cycles, non-finite transforms, and exhausted node IDs transactionally, but it has no explicit serialized-byte or node-count budget.

`Engine::load_scene` likewise streams the entire file into an `ostringstream` before calling scene deserialization. Therefore an untrusted or accidentally huge scene file can force memory growth in three layers: the input string, the parsed JSON tree, and the candidate scene. This confirms the resource-hardening gap recorded after scene persistence v1; the old finding remains current on the audited main SHA.

## Contract for the next implementation

The release-facing file-load path should reject scene files larger than a documented fixed byte budget before buffering/parsing them. Direct scene deserialization should independently enforce the same serialized-input budget because callers can bypass `Engine::load_scene`. After parsing, scene v1 should reject node arrays above a documented node budget before allocating the candidate scene.

Recommended initial bounds for the low-resource/headless-first core:

* serialized scene JSON: 8 MiB maximum;
* scene nodes: 65,536 maximum.

Both failures must return `false` and leave the active/live scene unchanged. The limits should be named constants in the scene persistence API so tests and callers do not duplicate magic numbers.

## Regression gates required

Add tests proving acceptance at ordinary sizes and rejection above both byte and node budgets, including transactional non-mutation. Gate the contract on normal Linux, ASan/UBSan, Windows/MSVC, and the strict GCC/Clang compiler matrix where the scene suite is available.

## Resource and compatibility impact

The proposed limits add no dependency, thread, graphics backend, or steady-state allocation. They bound worst-case persistence memory rather than increasing it. Existing scenes above either limit would become intentionally unsupported by v1 and should fail explicitly rather than risk process exhaustion. If real workloads later demonstrate a need for larger scenes, the format should move toward bounded/streaming parsing rather than simply removing the guard.

## Adjacent findings

Scene persistence remains header-inline and therefore consumers invoking it must link `litt_json.c`; moving implementation to a `.cpp` translation unit remains a cleanup opportunity. Runtime/render attachments remain outside scene JSON v1 and must not be described as complete runtime-state persistence.

## Status

Audit/contract complete. No behavior is claimed as promoted by this report alone. Implementation, regression evidence, full CI, and merge are still required.
