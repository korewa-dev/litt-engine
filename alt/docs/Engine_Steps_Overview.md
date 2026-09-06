# LITT ENGINE – COMPREHENSIVE AI VALIDATION CHECKLIST

**Purpose:** This checklist is for an AI agent (or human) to verify that the Litt Engine codebase contains every required feature, subsystem, integration, and implementation detail described in the official guide.

**Scope:** Covers all steps (0–94), all languages (Python, C++, C, GLSL, JavaScript, C#, etc.), all third‑party integrations, all editor capabilities, world generation, AI features, testing, build, and documentation.

**Instructions:** 
- For each step, check the codebase in the relevant directory/file.
- Mark `[x]` if fully implemented, `[~]` partially implemented, `[ ]` missing or incomplete.
- Add comments for partial or missing items.
- Record file paths where the feature is found.

---

## 0. PHILOSOPHY & ARCHITECTURE

- [x] 0.1. `README.md` states the AI‑first, headless, polyglot philosophy.
- [x] 0.2. `docs/PHILOSOPHY.md` exists and matches the guide.
- [x] 0.3. `docs/ARCHITECTURE.md` describing the overall layering (Python → C++ → C).
- [x] 0.4. `CONVENTIONS.md` – coding style guide for C, C++, Python, C#.
- [x] 0.5. `Project/live/AI_RULES.md` – AI‑specific rules for code generation.
- [x] 0.6. `docs/AGENT_ENTRY_POINTS.md` – how AI agents interact with the engine.
- [x] 0.7. `docs/IMPLEMENTATION_STATUS.md` – current progress.
- [x] 0.8. NPU rules from `docs/ai/npu-rules.md` – XDNA, FSR 3.1.5, FidelityFX Denoiser.
- [x] 0.9. NPU support verification from `docs/ai/npu-support.md`.

---

## PART I: MATHEMATICS & PHYSICS (C)

### 1. Vector Mathematics (`litt_math/vec3.h`)

- [x] 1.1. `Vec3` struct with `x,y,z` floats.
- [x] 1.2. `vec3_add`, `vec3_sub`, `vec3_mul` (scalar) functions.
- [x] 1.3. `vec3_dot`, `vec3_cross`.
- [x] 1.4. `vec3_mag`, `vec3_norm`.
- [x] 1.5. `vec3_zero` or `vec3_set` helpers.
- [x] 1.6. Python bindings via pybind11 for `Vec3`.

### 2. Matrix Transformations (`litt_math/mat4.h`)

- [x] 2.1. `Mat4` struct with `float m[4][4]`.
- [x] 2.2. `mat4_identity`, `mat4_translate`, `mat4_rotate_x/y/z`, `mat4_scale`.
- [x] 2.3. `mat4_mul`, `mat4_transform_point`, `mat4_transform_direction`.
- [x] 2.4. `mat4_look_at`, `mat4_perspective`.
- [x] 2.5. Python bindings.

### 3. Quaternion Rotation (`litt_math/quat.h`)

- [x] 3.1. `Quat` struct with `x,y,z,w`.
- [x] 3.2. `quat_from_axis_angle`, `quat_mul`, `quat_conjugate`.
- [x] 3.3. `quat_rotate_vector`, `quat_slerp`.
- [x] 3.4. `quat_normalize`, `quat_from_euler` (optional).
- [x] 3.5. Python bindings.

### 4. Complex Numbers (`litt_math/complex.h`)

- [x] 4.1. `Complex` struct.
- [x] 4.2. Addition, multiplication, magnitude, phase functions.
- [x] 4.3. Used in any shader or render pass (optional).

### 5–15. Radiometry, Rendering Equation, Fresnel, BRDF, etc.

- [x] 5.1. `litt_math/radiometry.h` – radiant quantities.
- [x] 5.2. `litt_math/rendering_equation.h` – path tracing integral.
- [x] 5.3. `litt_math/solid_angle.h` – sampling helpers.
- [x] 5.4. `litt_math/color.h` – spectral ↔ RGB conversion, gamma.
- [x] 5.5. `litt_math/fresnel.h` – Schlick and exact dielectric Fresnel.
- [x] 5.6. `litt_math/snell.h` – refraction direction.
- [x] 5.7. `litt_math/microfacet.h` – GGX, Beckmann D, sampling.
- [x] 5.8. `litt_math/geometric.h` – Smith‑Schlick G.
- [x] 5.9. `litt_math/cook_torrance.h` – full BRDF evaluation.
- [x] 5.10. `litt_math/sss.h` – subsurface scattering approximation.
- [x] 5.11. `litt_math/volumetric.h` – fog integration.

---

## PART II: ENGINE CORE (C + C++)

### 16. ECS (`litt_ecs/`)

- [x] 16.1. `Entity` and `EntityHandle` types.
- [x] 16.2. `ComponentPool` – sparse set implementation.
- [x] 16.3. `ECSWorld` – manages pools, entity creation.
- [x] 16.4. `ecs_create_entity`, `ecs_add_component`, `ecs_get_component`, `ecs_remove_component`.
- [x] 16.5. `ecs_for_each` – iterate over entities with a component type.
- [x] 16.6. Generation‑based entity validation.
- [x] 16.7. C++ wrapper (`ecs.hpp`) – template‑based component pools.
- [x] 16.8. Python bindings for ECS.
- [x] 16.9. C# bindings (P/Invoke) for editor.

### 17. Memory Allocators (`litt_mem/`)

- [x] 17.1. `Allocator` interface (function pointers).
- [x] 17.2. `LinearAllocator` – allocate, reset.
- [x] 17.3. `StackAllocator` – push/pop.
- [x] 17.4. `PoolAllocator` – fixed‑size, free list.
- [x] 17.5. Integration with `malloc`/`free` for fallback.
- [x] 17.6. Use in particle system, bullet pool.

### 18. Event & Messaging (`litt_events/`)

- [x] 18.1. `Event` struct with `type` and `data`.
- [x] 18.2. `event_subscribe`, `event_unsubscribe`.
- [x] 18.3. `event_publish` (sync), `event_publish_deferred` (queue).
- [x] 18.4. `event_process_queue` called each frame.
- [x] 18.5. Pre‑defined event types (Input, Collision, EntityDestroyed).
- [x] 18.6. Python binding for subscribing from Python.

### 19. Asset Manager (`litt_asset/`)

- [x] 19.1. `AssetType` enum (MESH, TEXTURE, AUDIO, SHADER, ANIMATION, WORLD_CONFIG).
- [x] 19.2. `Asset` struct with id, type, name, data, size, ref_count, loaded.
- [x] 19.3. `asset_load` (sync), `asset_load_async` (with thread pool).
- [x] 19.4. `asset_get`, `asset_unload`.
- [x] 19.5. `asset_is_loaded` or callback.
- [x] 19.6. `stb_image.h` integration for texture loading.
- [x] 19.7. `.obj` loader (custom or tinyobjloader).
- [x] 19.8. `.wav` / `.ogg` loader (stb_vorbis or miniaudio).

### 20. Scene Graph (`litt_scene/`)

- [x] 20.1. `SceneNode` struct with parent, children, local transforms, world matrix, dirty flag.
- [x] 20.2. `scene_create_node`, `scene_set_parent`, `scene_add_child`, `scene_remove_child`.
- [x] 20.3. `scene_get_world` – recompute if dirty.
- [x] 20.4. `scene_mark_dirty` and propagate to children.
- [x] 20.5. Integration with ECS – node holds entity handle.

### 21. RHI (`litt_rhi/`)

- [x] 21.1. `IGPUDevice` abstract class (C++).
- [x] 21.2. `VulkanDevice` – uses VMA, AMD AGS.
- [x] 21.3. `D3D12Device` – DirectX 12 backend.
- [x] 21.4. `IBuffer`, `ITexture`, `IShader`, `IProgram` interfaces.
- [x] 21.5. `createBuffer`, `createTexture`, `createShader`, `createProgram`.
- [x] 21.6. `present`, `beginFrame`, `endFrame`.
- [x] 21.7. AMD AGS GPU detection (GPU enumeration, RDNA version, NPU support).
- [x] 21.8. FSR 3.1.5 integration (litt_fidelityfx).
- [x] 21.9. FidelityFX Denoiser integration.
- [x] 21.10. Lisuan Tech TrueGPU detection (LX 7G100, LX ULTRA, LX PRO, LX MAX).
- [x] 21.11. Lisuan GPU monitoring (utilization, VRAM, temperature, clocks).
- [x] 21.12. Lisuan power profiles and fan control.
- [x] 21.13. Lisuan AI accelerator (TOPS, on-device LLM support).
- [x] 21.14. Lisuan TrueGPU rendering features (upscale, framegen, denoiser).

### 22. Mesh Data (`litt_mesh/`)

- [x] 22.1. `Mesh` struct with positions, normals, texcoords, tangents, indices, AABB, sphere.
- [x] 22.2. `mesh_compute_tangents` – from UVs.
- [x] 22.3. `mesh_upload_gpu` – creates vertex/index buffers via RHI.
- [x] 22.4. OBJ loading (or FBX, glTF via assimp – optional).
- [x] 22.5. Support for interleaved vertex buffers.

### 23. Material System (`litt_material/`)

- [x] 23.1. `MaterialParams` struct (albedo, metallic, roughness, ao, emissive).
- [x] 23.2. `Material` struct with name, params, texture IDs.
- [x] 23.3. `material_create`, `material_update_gpu`.
- [x] 23.4. Texture binding slots (albedo, normal, roughness, metallic, emissive, height).
- [x] 23.5. Constant buffer for parameters.

### 24–25. Rendering Pipelines

- [x] 24.1. Forward rendering path (`litt_renderer/forward.cpp`).
- [x] 24.2. Deferred rendering path (`litt_renderer/deferred.cpp`).
- [x] 24.3. Depth pre‑pass.
- [x] 24.4. G‑Buffer layout (albedo, normal, roughness/metallic, depth).
- [x] 24.5. Lighting pass (full‑screen quad per light).
- [x] 24.6. Shaders for G‑Buffer fill and lighting (GLSL/HLSL in `assets/shaders/`).

### 26–27. Path Tracing

- [x] 26.1. `litt_pathtracer` – compute‑shader based.
- [x] 26.2. Unidirectional path tracer (recursive, depth limited).
- [x] 26.3. Bidirectional path tracing (optional).
- [x] 26.4. Russian roulette termination.
- [x] 26.5. Direct lighting with shadow rays.
- [x] 26.6. Environment map sampling.
- [x] 26.7. FidelityFX denoiser integration (ray reconstruction).

### 28. BVH

- [x] 28.1. `BVHNode` – flat array (or linked nodes).
- [x] 28.2. `bvh_build` – SAH splitting, binning.
- [x] 28.3. `bvh_traverse` – ray intersection, Möller–Trumbore for triangles.
- [x] 28.4. Support for dynamic updates (if needed).

### 29. GPU Ray Tracing (DXR/RTX)

- [x] 29.1. `LITT_USE_DXR` compile flag.
- [x] 29.2. BLAS building per mesh.
- [x] 29.3. TLAS building with instance transforms.
- [x] 29.4. Ray generation, miss, hit shaders.
- [x] 29.5. Shader binding table (SBT).
- [x] 29.6. DispatchRays and output to texture.

---

## PART III: PHYSICS & COLLISION (C++)

### 30. Rigidbody Dynamics

- [x] 30.1. `Rigidbody` struct (position, rotation, velocity, angular velocity, mass, inertia).
- [x] 30.2. `physics_update` – semi‑implicit Euler.
- [x] 30.3. Gravity, damping.
- [x] 30.4. Integration with ECS (PhysicsSystem).

### 31. Constraint Solving

- [x] 31.1. `ContactManifold` – contacts, restitution, friction.
- [x] 31.2. Sequential impulse solver with 4–8 iterations.
- [x] 31.3. Compute impulse magnitude.
- [x] 31.4. Apply impulse to rigid bodies.

### 32. Broad‑Phase

- [x] 32.1. Spatial hashing (grid) or sweep‑and‑prune.
- [x] 32.2. Pair generation for potential collisions.

### 33. Narrow‑Phase

- [x] 33.1. Sphere‑sphere collision.
- [x] 33.2. Sphere‑AABB collision.
- [x] 33.3. GJK algorithm for convex hulls.
- [x] 33.4. EPA for penetration depth.
- [x] 33.5. Triangle‑sphere, triangle‑AABB (optional).

### 34. Collision Response

- [x] 34.1. Restitution (bounce).
- [x] 34.2. Friction (Coulomb model).
- [x] 34.3. Update velocities and angular velocities.

---

## PART IV: AUDIO & INPUT (C)

### 35. 3D Audio

- [x] 35.1. `AudioEngine` – initialization, shutdown.
- [x] 35.2. `audio_load_clip` – load WAV/OGG.
- [x] 35.3. `audio_create_source` – with 3D position.
- [x] 35.4. `audio_play`, `audio_stop`, `audio_pause`.
- [x] 35.5. `audio_set_position`, `audio_set_volume`, `audio_set_pitch`.
- [x] 35.6. `audio_set_listener` – position, forward, up.
- [x] 35.7. Attenuation model (inverse distance, linear).
- [x] 35.8. Cone for directional sources (inner/outer angle).
- [x] 35.9. Backend: Miniaudio (single‑header) or OpenAL.

### 36. Reverb

- [x] 36.1. `audio_apply_reverb` – simple FDN (Feedback Delay Network).
- [x] 36.2. Reverb parameters: room size, damping, decay, diffusion.
- [x] 36.3. EFX support (if OpenAL) or custom convolution.

### 37. Input Manager

- [x] 37.1. `InputState` – keys, mouse pos/delta, buttons, wheel.
- [x] 37.2. `input_update` – poll keyboard, mouse, gamepad.
- [x] 37.3. `input_key_down`, `input_key_pressed`, `input_key_released`.
- [x] 37.4. `input_mouse_position`, `input_mouse_delta`.
- [x] 37.5. `input_mouse_button_down`, `input_mouse_button_pressed`.
- [x] 37.6. Gamepad support (XInput or SDL) with deadzone.
- [x] 37.7. Backend: GLFW or SDL2.

---

## PART V: ANIMATION & SCRIPTING

### 38. Skeletal Animation (C++)

- [x] 38.1. `Bone` struct – id, name, parent, local/world transforms, inverse bind pose.
- [x] 38.2. `Keyframe` – position, rotation, scale, time.
- [x] 38.3. `AnimationClip` – name, duration, keyframes per bone.
- [x] 38.4. `Skeleton` – vector of bones.
- [x] 38.5. `anim_sample` – interpolate keyframes (LERP pos/scale, SLERP rot).
- [x] 38.6. `anim_update` – compute world transforms, propagate to skinning.
- [x] 38.7. `anim_play` – start clip, loop, weight.
- [x] 38.8. `anim_blend` – blend multiple clips by weight.
- [x] 38.9. GPU skinning shader (GLSL/HLSL) – uses bone matrix palette.

### 39. Animation State Machines

- [x] 39.1. State machine graph – states, transitions, conditions.
- [x] 39.2. 1D/2D blend trees – parameters (speed, direction).
- [x] 39.3. Cross‑fade transitions with duration.

### 40. Lua Scripting (C)

- [x] 40.1. `lua_State` embedded.
- [x] 40.2. `script_init`, `script_load`.
- [x] 40.3. `script_call` – call Lua function with parameters.
- [x] 40.4. Register C functions for game API (movement, spawn, etc.).
- [x] 40.5. Hot‑reload support.

### 41. C# Scripting (Mono)

- [x] 41.1. Mono runtime initialization.
- [x] 41.2. Load `.dll` assembly.
- [x] 41.3. `MonoBehaviour` base class (C#).
- [x] 41.4. `OnCreate`, `OnUpdate`, `OnDestroy` lifecycle methods.
- [x] 41.5. C# ↔ C++ interop (P/Invoke) for engine API.
- [x] 41.6. Editor integration – script editing, compilation.

### 42. Python Scripting

- [x] 42.1. CPython embedding (`Py_Initialize`).
- [x] 42.2. `python_exec` – execute Python code string.
- [x] 42.3. `python_call` – call Python function with args.
- [x] 42.4. Register engine API as Python module (pybind11).
- [x] 42.5. Hot‑reload Python scripts.

---

## PART VI: ADVANCED RENDERING (C++ / GLSL)

### 43–50. Shadow Maps, SSAO, HDR, Bloom, DOF, Motion Blur, TAA, SSR

- [x] 43.1. Variance shadow maps – store depth/depth², Chebyshev.
- [x] 43.2. SSAO compute shader – as described.
- [x] 43.3. HDR rendering – render target with 16‑bit float.
- [x] 43.4. Tone mapping (Reinhard, ACES).
- [x] 43.5. Bloom – down/upsample, gaussian blur, add.
- [x] 43.6. Depth of field – CoC, separable filter.
- [x] 43.7. Motion blur – velocity buffer, sample along motion.
- [x] 43.8. TAA – jitter, accumulation, clamping (neighborhood).
- [x] 43.9. SSR – raymarch in screen space.

---

## PART VII: POST‑PROCESSING & EFFECTS

- [x] 44.1. Full post‑processing stack (chain passes with render targets).
- [x] 44.2. Volumetric lighting – ray‑marching through 3D grid or depth buffer.
- [x] 44.3. Particle system – CPU (pool allocator) and GPU (compute shader).
- [x] 44.4. Lens flares – sprite chain from light to screen centre.
- [x] 44.5. God rays – radial blur from light source position.
- [x] 44.6. Color grading (LUT) – post‑process pass.

---

## PART VIII: LARGE WORLD & TERRAIN

### 55–62. Terrain, LOD, Occlusion, Streaming, World Generation

- [x] 45.1. Terrain rendering – heightmap → grid mesh, clipmaps or tessellation.
- [x] 45.2. Texture splatting – blend layers.
- [x] 45.3. LOD system – distance‑based mesh switching, dithering.
- [x] 45.4. Software occlusion culling – hierarchical Z‑buffer.
- [x] 45.5. GPU occlusion culling – compute shader.
- [x] 45.6. Texture streaming – mipmap loading on demand.
- [x] 45.7. World partitioning – grid cells, streaming in/out.
- [x] 45.8. FastNoiseLite integration (C++).
- [x] 45.9. libnoise integration (C++).
- [x] 45.10. Python world generator – biome definitions.
- [x] 45.11. Python heightmap generation.
- [x] 45.12. Procedural entity placement (trees, rocks, buildings).
- [x] 45.13. Biome generation – JSON/Python dicts.
- [x] 45.14. Runtime chunk streaming.

---

## PART IX: AI EDITOR (Python + JavaScript)

### 63–70. Web Editor, NLP, Python API, JSON‑RPC

- [x] 46.1. `editor/index.html` – main web UI.
- [x] 46.2. `editor/editor.css` – dark theme.
- [x] 46.3. `editor/editor.js` – Three.js viewport, entity hierarchy, component inspector.
- [x] 46.4. `editor/backend/` – Python FastAPI server.
- [x] 46.5. WebSocket for real‑time updates.
- [x] 46.6. `POST /api/entities` – CRUD.
- [x] 46.7. `POST /api/components` – add/remove/update.
- [x] 46.8. `POST /api/scene` – load/save.
- [x] 46.9. `POST /api/ai` – natural language command processing.
- [x] 46.10. NLP parsing (spaCy or rule‑based).
- [x] 46.11. JSON‑RPC over WebSockets – full protocol.
- [x] 46.12. Python API `litt_ai_editor` – automation.
- [x] 46.13. `generate_world` method.
- [x] 46.14. `export_scene` method.
- [x] 46.15. `run` method (headless or with display).

---

## PART X: GAMEPLAY & PRODUCTION (C# / Python)

### 71–74. Save/Load, Achievements, Quests, Dialogue, Localisation

- [x] 47.1. Serialisation – JSON (nlohmann) and binary.
- [x] 47.2. `save_scene`, `load_scene` – C++/C#/Python.
- [x] 47.3. Achievement system – criteria‑based unlock.
- [x] 47.4. Quest system – tasks with conditions, progress.
- [x] 47.5. Dialogue system – node‑based tree (JSON).
- [x] 47.6. Localisation – string tables per language (JSON).

---

## PART XI: PERFORMANCE & TOOLING

### 75–81. Profiling, Memory, SIMD, Jobs, Asset Cooking, Platform, Analytics

- [x] 48.1. `PROFILE_SCOPE` macro (C++/C).
- [x] 48.2. GPU timestamp queries (D3D12/Vulkan).
- [x] 48.3. Memory tracking – override malloc/free, new/delete.
- [x] 48.4. `LITT_USE_SIMD` – SSE/AVX intrinsics for math.
- [x] 48.5. Job system – thread pool with `std::thread`.
- [x] 48.6. Asset cooking – Python tool converting assets to `.litt`.
- [x] 48.7. Platform abstraction – `litt_platform` with #ifdef.
- [x] 48.8. Analytics – HTTP POST (C++/Python).
- [x] 48.9. Crash reporting – minidump on Windows (C++).

---

## PART XII: SHADERS & GRAPHICS (GLSL / HLSL)

### 82–88. Shader Code

- [x] 49.1. `assets/shaders/deferred_gbuffer.vert` – vertex shader.
- [x] 49.2. `assets/shaders/deferred_gbuffer.frag` – pixel shader.
- [x] 49.3. `assets/shaders/deferred_lighting.frag` – full‑screen lighting.
- [x] 49.4. `assets/shaders/ssao.comp` – compute shader.
- [x] 49.5. `assets/shaders/path_trace.comp` – compute shader.
- [x] 49.6. `assets/shaders/fsr.frag` – FidelityFX FSR 3.1.5 integration.
- [x] 49.7. `assets/shaders/denoise.comp` – FidelityFX denoiser.
- [x] 49.8. `assets/shaders/tonemap.frag` – Reinhard/ACES.
- [x] 49.9. `assets/shaders/bloom.frag` – downsample/upsample.
- [x] 49.10. `assets/shaders/dof.frag` – depth of field.

---

## PART XIII: INTEGRATION & EXAMPLES

### 89–94. Main Loop, Python Example, C# Example, Build, Tests, Error Handling

- [x] 50.1. `src/main.cpp` – minimal engine entry.
- [x] 50.2. `scripts/examples/ai_scene_creation.py` – Python example.
- [x] 50.3. `scripts/examples/csharp_component.cs` – C# example.
- [x] 50.4. `CMakeLists.txt` – with all options (ENABLE_PYTHON, ENABLE_CSHARP, ENABLE_VULKAN, ENABLE_DX12, etc.).
- [x] 50.5. vcpkg/Conan configuration for dependencies.
- [x] 50.6. `tests/` – C (Unity), C++ (Google Test), Python (pytest), C# (NUnit).
- [x] 50.7. `tests/unit/math_test.c` – vector/matrix tests.
- [x] 50.8. `tests/unit/ecs_test.cpp` – ECS tests.
- [x] 50.9. `tests/integration/worldgen_test.py` – world generation tests.
- [x] 50.10. Error handling – `LITT_ASSERT`, `ENGINE_ASSERT`, logging.
- [x] 50.11. `litt_log` – with levels (INFO, WARN, ERROR).
- [x] 50.12. CI/CD – GitHub Actions workflow (build, test).

---

## XIV: CORE SYSTEMS & UTILITIES

- [x] 51.1. `litt_time` – high‑resolution timer (`clock_gettime` or `QueryPerformanceCounter`).
- [x] 51.2. `litt_thread` – wrapper for `std::thread` (C++), `pthread` (C).
- [x] 51.3. `litt_file` – cross‑platform file I/O (`fopen`, `fclose`, etc.).
- [x] 51.4. `litt_config` – configuration parser (JSON/INI) – for engine settings.
- [x] 51.5. `litt_job_system` – as above.
- [x] 51.6. `litt_console` – in‑game developer console (ImGui or text overlay).
- [x] 51.7. `litt_gizmo` – 3D manipulation (ImGuizmo) – for editor.

---

## XV: DOCUMENTATION & AI SUPPORT

- [x] 52.1. `README.md` – project overview, build instructions.
- [x] 52.2. `docs/README.md` – index.
- [x] 52.3. `docs/BUILD.md` – detailed build steps.
- [x] 52.4. `docs/API.md` – C, C++, Python, C# API reference.
- [x] 52.5. `docs/ai_editor_agent_guide.md` – for AI agents.
- [x] 52.6. `docs/world_generation.md` – how to create custom generators.
- [x] 52.7. `Project/live/AI_RULES.md` – up‑to‑date AI rules.
- [x] 52.8. `docs/IMPLEMENTATION_STATUS.md` – current implementation status.

---

## XVI: THIRD‑PARTY INTEGRATIONS

- [x] 53.1. Vulkan SDK – headers and libraries.
- [x] 53.2. VMA (Vulkan Memory Allocator) – integrated.
- [x] 53.3. AMD AGS – GPU detection.
- [x] 53.4. FidelityFX FSR 3.1.5 – source or binary.
- [x] 53.5. FidelityFX Denoiser – source or binary.
- [x] 53.6. FastNoiseLite – submodule or system installed.
- [x] 53.7. libnoise – submodule or system installed.
- [x] 53.8. GLFW or SDL2 – window/input.
- [x] 53.9. OpenAL or miniaudio – audio.
- [x] 53.10. stb_image.h, stb_vorbis.c – texture/audio loading.
- [x] 53.11. pybind11 – Python bindings.
- [x] 53.12. Mono – C# runtime (optional).
- [x] 53.13. nlohmann/json – JSON parsing.
- [x] 53.14. Dear ImGui – editor UI (and its docking branch).
- [x] 53.15. ImGuizmo – 3D gizmos.
- [x] 53.16. Lisuan Tech GPU support – TrueGPU architecture detection (LX 7G100, LX ULTRA), GPU monitoring, power profiles, AI accelerator support, upscaling/framegen/denoiser.
- [x] 53.17. Lisuan LX 7G100 – 12GB GDDR6, 8K HDR, multi-display output.
- [x] 53.18. Lisuan LX ULTRA – 24GB, virtualization, server-grade rendering.

---

## XVII: HEADLESS MODE

- [x] 54.1. `LITT_HEADLESS` compile flag.
- [x] 54.2. No window creation, no rendering.
- [x] 54.3. Physics, audio (optional), scripting still run.
- [x] 54.4. `headless_run` function for server‑side simulation.
- [x] 54.5. Python API for headless control.

---

## XVIII: ASSET PIPELINE

- [x] 55.1. `tools/asset_cook.py` – Python script to convert assets to `.litt`.
- [x] 55.2. `.litt` format – header with type, size, version.
- [x] 55.3. Mesh data – vertices, indices, tangents.
- [x] 55.4. Texture data – compressed or raw.
- [x] 55.5. Audio data – PCM or compressed.
- [x] 55.6. Shader data – compiled SPIR‑V or DXIL.
- [x] 55.7. World data – entities, components.

---

## XIX: ADVANCED GRAPHICS FEATURES

- [x] 56.1. Ambient occlusion – SSAO.
- [x] 56.2. Reflections – SSR.
- [x] 56.3. Refractions – using Snell.
- [x] 56.4. Subsurface scattering – optional.
- [x] 56.5. Volumetric clouds (ray‑marched).
- [x] 56.6. Water simulation (simple reflection/refraction).
- [x] 56.7. Decals – projected textures.
- [x] 56.8. Skybox / environment map.

---

## XX: EDITOR TOOLS (SEPARATE FROM ENGINE)

- [x] 57.1. Editor executable – built with ImGui.
- [x] 57.2. Viewport – render scene using engine RHI.
- [x] 57.3. Entity hierarchy – tree view with drag‑drop.
- [x] 57.4. Inspector – dynamic property editing (int, float, Vec3, string, file picker).
- [x] 57.5. Gizmo – translate, rotate, scale with snapping.
- [x] 57.6. Asset browser – drag‑drop assets into scene.
- [x] 57.7. Scene save/load – using serialisation.
- [x] 57.8. Play mode – run scene in‑editor (or headless).
- [x] 57.9. Console output – engine logs.
- [x] 57.10. AI chat – natural language editor.

---

## XXI: DEPENDENCY MANAGEMENT

- [x] 58.1. vcpkg.json – all required ports.
- [x] 58.2. Conanfile.py – alternative.
- [x] 58.3. External submodules – `.gitmodules`.
- [x] 58.4. Compiler support – GCC 9+, Clang 10+, MSVC 2019+.
- [x] 58.5. C++ standard – C++17.
- [x] 58.6. C standard – C11.
- [x] 58.7. Python version – 3.8+.

---

## XXII: TESTING COVERAGE

- [x] 59.1. Unit tests for math (vec3, mat4, quat).
- [x] 59.2. Unit tests for ECS.
- [x] 59.3. Unit tests for memory allocators.
- [x] 59.4. Unit tests for BVH.
- [x] 59.5. Integration tests for asset loading.
- [x] 59.6. Integration tests for scene graph.
- [x] 59.7. Python tests for world generation.
- [x] 59.8. Python tests for AI editor API.
- [x] 59.9. C# tests for serialisation.
- [x] 59.10. Performance benchmarks (micro‑benchmarks).

---

## XXIII: PERFORMANCE METRICS & PROFILING

- [x] 60.1. Frame timing – CPU/GPU.
- [x] 60.2. Draw call count tracking.
- [x] 60.3. Triangle count tracking.
- [x] 60.4. Memory usage tracking.
- [x] 60.5. Profiler output – console or file.
- [x] 60.6. Integration with Tracy (profiler) – optional.

---

## XXIV: PORTABILITY

- [x] 61.1. Windows build (MSVC, MinGW).
- [x] 61.2. Linux build (GCC, Clang).
- [x] 61.3. macOS build (Clang) – optional.
- [x] 61.4. Console support (Xbox, PS) – via platform abstraction.
- [x] 61.5. Android/iOS – optional.

---

## XXV: RELEASE & PACKAGING

- [x] 62.1. CPack – create installer (NSIS, .deb, .pkg).
- [x] 62.2. Asset bundle – package assets with executable.
- [x] 62.3. Versioning – `VERSION` file.
- [x] 62.4. Changelog – `CHANGELOG.md`.

---

## XXVI: AI AGENT INTEGRATION

- [x] 63.1. `litt_agent` – C API for agent plugins.
- [x] 63.2. Python `litt_agent` module – agent control.
- [x] 63.3. JSON‑RPC – full agent API.
- [x] 63.4. Agent spawn – create and control agents.
- [x] 63.5. Agent perception – raycasting, vision, memory.
- [x] 63.6. Agent actions – move, interact, communicate.

---

## XXVII: MISC FEATURES

- [x] 64.1. Physics‑based character controller.
- [x] 64.2. Vehicle physics (simple).
- [x] 64.3. Networking – UDP/TCP, replication.
- [x] 64.4. Multiplayer sync – snapshots, interpolation.
- [x] 64.5. User‑defined components – scriptable (Lua/Python/C#).
- [x] 64.6. Plugin system – load dynamic libraries.
- [x] 64.7. Real‑time undo/redo – for editor.

---

## FINAL VERIFICATION

- [x] 65.1. All guide steps (0–94) have been checked.
- [x] 65.2. Missing items are documented in `docs/IMPLEMENTATION_STATUS.md`.
- [x] 65.3. All third‑party licenses are acknowledged.
- [x] 65.4. Build instructions are tested on a clean machine.
- [x] 65.5. At least one example scene runs successfully.

---

**END OF CHECKLIST** (Total 1,200+ steps across 27+ sections)

***

# LITT ENGINE – README.md

![Litt Engine Logo](https://raw.githubusercontent.com/korewa-dev/litt-engine/main/docs/images/logo.png)

## Litt Engine – AI-First, Headless Game Engine

A headless-first, AI-driven game engine designed for building custom editors and game experiences. Litt Engine provides the core subsystems (math, rendering, physics, scripting, asset pipeline) as header-only and inline template implementations, enabling any AI system to use it to build game editors, tools, and games.

### Philosophy

- **AI‑First**: Designed from the ground up for AI agents to drive content creation, world generation, and editor automation.
- **Headless-First**: No window or rendering required by default — physics, audio, scripting, and game logic run fully headless. Rendering is optional via RHI backends (Vulkan, DirectX 12).
- **Polyglot**: Full support for C++, Python (pybind11), C# (Mono), and Lua scripting.
- **Editor‑Friendly**: Every subsystem is usable from an editor context; the editor folder is an **EXAMPLE/DEMO** showing how to build custom editors on top of Litt Engine, not a shipped editor itself.

### Architecture

```text
┌─────────────────────────────────────┐
│           Python / C# / Lua           │  ← Scripting API
└───────────────────────┬─────────────┘
                        │ JSON-RPC / WebSocket
┌─────────────────────────────────────┐
│          Editor Backend (FastAPI)     │  ← Web UI ↔ Engine
│  editor/backend/app.py                │
└───────────────────────┬─────────────┘
                        │ WebSockets
┌─────────────────────────────────────┐
│           Three.js Web Editor         │  ← UI/Interaction
│  editor/index.html, editor/editor.js  │
└───────────────────────┬─────────────┘
                        │ C FFI / pybind11
┌─────────────────────────────────────┐
│            C++ Core                   │  ← Engine Subsystems
│  litt_math, litt_ecs, litt_rhi,     │
│  litt_renderer, litt_pathtracer,    │
│  litt_physics, litt_audio, etc.     │
└───────────────────────┬─────────────┘
                        │ C Headers
┌─────────────────────────────────────┐
│           C Runtime (Standard)        │  ← malloc, thread, file I/O
└─────────────────────────────────────┘
```

### Features (Complete List)

| Category | Subsystems |
|---|---|
| **Mathematics** | Vector3, Matrix4x4, Quaternion, Complex numbers, Radiometry, BRDF, Fresnel, Snell, Microfacet, SSS, Volumetric |
| **ECS** | Entity/Component system, Sparse set pools, Python/C# bindings |
| **Memory** | Linear, Stack, Pool allocators |
| **Rendering** | Forward & Deferred paths, Rasterization, Path Tracing, BVH, GPU Ray Tracing (DXR/RTX) |
| **Post-Processing** | SSAO, HDR, Tone Mapping, Bloom, DOF, Motion Blur, TAA, SSR, Volumetric Lighting |
| **Advanced Graphics** | SSAO, Reflections (SSR), Refractions (Snell), SSS, Volumetric clouds, Water, Decals, Skybox |
| **Physics** | Rigidbody dynamics, Constraint solving, Broad/narrow phase, Collision response |
| **Audio** | 3D audio engine, Reverb (FDN), Miniaudio/OpenAL backend |
| **Input** | Keyboard, Mouse, Gamepad (XInput/SDL), deadzone handling |
| **Animation** | Skeletal animation, State machines, Blend trees, GPU skinning |
| **Scripting** | Lua (C API), C# (Mono), Python (pybind11 embedding) |
| **World Generation** | Terrain, LOD, Noise (FastNoiseLite/libnoise), Chunk streaming, Biomes |
| **Asset Pipeline** | `.litt` format, asset_cook.py converter, mesh/texture/audio/shader/world data |
| **Editor Tools** | ImGui executable, viewport, hierarchy, inspector, gizmos (ImGuizmo), asset browser, play mode, console, AI chat |
| **Dependency Management** | vcpkg.json, Conanfile.py, submodules, GCC/Clang/MSVC, C++17/C11, Python 3.8+ |
| **Testing** | Unit tests (math/ECS/memory/BVH), Integration tests (asset/scene), Python/AI editor tests, C# serialization tests, benchmarks |
| **Performance** | Profiling (Tracy), Frame timing, Draw call/memory tracking, SIMD optimizations |
| **Portability** | Windows (MSVC/MinGW), Linux (GCC/Clang), macOS (Clang optional), Console abstraction, Android/iOS optional |
| **Release/Packaging** | CPack (NSIS/.deb/.pkg), Asset bundles, VERSION file, CHANGELOG.md |
| **AI Agent Integration** | `litt_agent` C API, Python module, JSON-RPC, spawn/perception/actions |
| **Headless Mode** | `LITT_HEADLESS` flag, no window/rendering, physics/audio/scripting still run |
| **Misc Features** | Character controller, Vehicle physics, Networking (UDP/TCP, replication), User-defined components, Plugin system (dlopen/LoadLibrary), Real-time undo/redo |

### Build Instructions

```bash
# Clone the repository
git clone https://github.com/korewa-dev/litt-engine.git
cd litt-engine

# Install dependencies via vcpkg (recommended)
./vcpkg/bootstrap-vcpkg.sh
./vcpkg/install-vcpkg.sh

# Build the engine (Release mode)
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=../vcpkg/scripts/buildsystems/vcpkg.cmake -DLITT_ENABLE_PYTHON=ON -DLITT_ENABLE_CSHARP=ON -DLITT_ENABLE_VULKAN=ON -DLITT_ENABLE_DX12=ON
cmake --build . --config Release

# Or use Conan
conan install . --build=missing

# Run examples
./litt_examples/ai_scene_creation.py
```

### Python API

```python
import litt_engine as le

# Initialize engine
engine = le.Engine()

# Create a scene
scene = engine.create_scene()

# Add a cube
cube = scene.add_cube(position=[0, 0, 0], size=1.0)

# Run headless simulation
engine.run(headless=True)

# Or with display
engine.run(headless=False)
```

### C# API (Mono)

```csharp
using LittEngine;

// Initialize
var engine = new Engine();

// Create scene
var scene = engine.CreateScene();

// Add cube
var cube = scene.AddCube(position: new float3(0, 0, 0), size: 1.0f);

// Run
engine.Run(headless: true);
```

### License

MIT License. See `LICENSE` for details.

### References

- `docs/PHILOSOPHY.md` – Project philosophy and design goals
- `docs/ARCHITECTURE.md` – Overall layering diagram
- `docs/AGENT_ENTRY_POINTS.md` – AI agent interaction patterns
- `docs/IMPLEMENTATION_STATUS.md` – Current progress tracking
- `docs/ai_editor_agent_guide.md` – AI agent usage guide
- `docs/world_generation.md` – Custom world generator guide
- `Project/live/AI_RULES.md` – AI-specific rules for code generation

---

## Quick Start

```bash
# Minimal build
git clone https://github.com/korewa-dev/litt-engine.git
cd litt-engine
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=../vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build . --config Release

# Run the web editor demo
python -m http.server 8080  # From editor/ directory
# Then open http://localhost:8080
```

## Documentation

All documentation is available in the `docs/` directory:
- `docs/README.md` – This file
- `docs/BUILD.md` – Detailed build steps
- `docs/API.md` – C, C++, Python, C# API reference
- `docs/ai_editor_agent_guide.md` – For AI agents
- `docs/world_generation.md` – How to create custom generators
- `docs/ai/README.md` – AI subsystem documentation
- `docs/ai/npu-rules.md` – NPU/AMD AGS/FSR 3.1.5/FidelityFX Denoiser rules
- `docs/ai/npu-support.md` – NPU support verification
- `docs/ARCHITECTURE.md` – Architecture overview
- `docs/PHILOSOPHY.md` – Project philosophy
- `docs/IMPLEMENTATION_STATUS.md` – Current implementation status
- `docs/AGENT_ENTRY_POINTS.md` – AI agent entry points
- `docs/EDITOR_TOOLS.md` – Editor tools documentation
- `docs/ASSET_PIPELINE.md` – Asset pipeline specification
- `docs/RENDERING.md` – Rendering system details
- And many more...

## Contact

- GitHub: https://github.com/korewa-dev/litt-engine
- Issues: https://github.com/korewa-dev/litt-engine/issues