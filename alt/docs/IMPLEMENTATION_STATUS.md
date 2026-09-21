# Litt Engine Implementation Inventory

> This file inventories implemented surfaces. It is not a release-support matrix. `SUPPORTED_RUNTIME.md` is authoritative for release support and promotion status.

## Current surface inventory

### Implemented surfaces with varying support levels

**Core Math & Types** (`litt_math.h`):
- Vec2, Vec3, Vec4 with full arithmetic operators
- Mat4 (16-element flat array) with identity, translation, zero, multiplication
- Quat with slerp, from_axis_angle, to_mat4
- Aabb (axis-aligned bounding box) with empty(), expand(), contains()
- OBB (oriented bounding box) with transform + half_extents
- Ray with origin/direction
- Legacy aliases: `Vec2f = Vec2`, `Aabbf = Aabb` (backward compat)

**ECS** (`litt_ecs.h`):
- Entity/Component/System architecture
- Sparse set component storage
- No duplicate type stubs (Mesh, Material, Light, Camera removed)

**Event System** (`litt_event.h`):
- Type-safe event dispatcher with compile-time routing

**Memory** (`litt_memory.h`, `litt_memory_pool.h`):
- Arena allocator, frame allocator, object pool
- StaticPool template

**Scene** (`litt_scene.h`):
- Release-supported bounded hierarchy and scene-persistence v1 contract
- 8 MiB serialized input, 65,536 nodes, 4 KiB node-name limits
- Transactional deserialize failure semantics
- Component pointers to renderer types remain outside persistence v1

**Renderer** (`litt_renderer.h`, `litt_gpu_software.h`):
- MeshData, RenderMaterial, RenderCamera, Light (authoritative)
- Release-supported bounded headless software renderer for CPU buffer/texture/raster/depth/mesh operations
- 64 MiB software-buffer and 16,777,216-pixel image ceilings
- Generic Vulkan/DX12/OpenGL/Metal hardware backends remain experimental/unavailable; fail-fast is tested, not accelerated rendering

**Lighting & PBR** (`litt_lighting.h`, `litt_material.h`, `litt_pbr_material.h`):
- LightType enum, Light struct, PBRLighting
- PBRMaterial with metallic/roughness, CookTorranceBRDF
- MaterialSerializer with JSON/binary

**Assets / Materials** (`litt_asset.h`, `litt_asset_pipeline.*`, `litt_pbr_material.h`):
- Release-supported bounded OBJ/TGA loading and synchronous asset metadata/reimport contract
- PBR factories and PBR-to-render-material conversion
- Placeholder shader compilation now fails explicitly; GPU upload and async asset loading are outside the supported core

**Textures** (`litt_texture.h`):
- Texture class with formats, mip levels, sampling

**Physics** (`litt_physics.h`):
- Release-supported bounded AABB rigid-body core
- 4,096-body budget, finite-state validation, sweep-and-prune broadphase
- AABB contacts, triggers/static bodies, linear force/impulse integration
- Rotation, friction, joints, CCD, and complex collider shapes are not part of the supported core

**BVH** (`litt_bvh.h`):
- SAH-based BVH construction
- Ray traversal

**World** (`litt_world.h`, `litt_world.cpp`):
- WorldManager, SceneManager
- Entity management

**Audio** (`litt_audio.h`, `litt_audio_wav.h`):
- Release-supported bounded PCM WAV decode and source-state contract
- Cross-platform SoftwareAudioMixer produces interleaved stereo float PCM
- Mono/stereo mix, volume, pitch, loop, play/pause/stop, master gain and output clamp are tested
- Windows waveOut is an optional platform sink; cross-platform physical-device output is not claimed

**UI** (`litt_ui.h`):
- UIElementKind enum, UIPanel, UIButton, UILabel, UISlider
- UIManager singleton

**Advanced Rendering** (in `litt_engine_systems.h`):
- SSR (screen-space reflections)
- SSAO (screen-space ambient occlusion)
- HDRPipeline with tone mapping (Reinhard, ACES, Filmic)
- BloomEffect
- DepthOfField
- MotionBlur
- TAA (temporal anti-aliasing)
- VarianceShadowMap

**Animation** (in `litt_engine_systems.h`):
- Bone, Keyframe, AnimationClip
- SkeletalAnimationController
- Skeleton (bone hierarchy)
- AnimationBlender

**Scripting** (`litt_scripting_vm.h`, C bridge):
- Release-supported bounded embedded VM subset for variables, literals, print/return, arithmetic/comparison/logical expressions
- Source/instruction/stack budgets are enforced
- Unsupported control flow fails at compile time
- Python/C#/Lua remain separate experimental integrations

**Networking** (in `litt_engine_systems.h`):
- NetworkManager with CLIENT/SERVER modes

**Gameplay Systems** (in `litt_engine_systems.h`):
- SaveLoadSystem
- AchievementSystem
- QuestSystem
- DialogueSystem

**Performance** (in `litt_engine_systems.h`):
- Profiler (begin_scope/end_scope)
- OcclusionCulling
- LODSystem (select_lod)
- TextureStreaming
- MemoryTracker

**Large World** (in `litt_engine_systems.h`):
- TerrainRenderer
- FoliageSystem
- WorldPartitioning
- LevelStreaming

**Engine Loop** (in `litt_engine_systems.h`):
- UISystem
- AssetPackager
- EngineLoop (initialize/run/stop)
- Benchmark

**Serialization** (`litt_serialization.h`):
- Serializer base, JSONSerializer, BinarySerializer
- SceneSerializer

### 📦 Test Coverage
- `test_full.cpp` - Minimal compilation test (all core headers) ✅
- `litt_engine_tests.cpp` - 58 comprehensive tests covering all systems ✅

### 🚫 Removed / Cleaned Up
- 15 dead phase test files (~2000 lines)
- `litt_scene_graph.h/cpp` (conflicted with SceneNode)
- `litt_audio_system.h` (conflicted with litt_audio.h)
- All Rust references from documentation

### 🔗 Architecture
- **Headless-first design** - no editor dependency
- **AI-accessible inventory** - C API is release-tested; Python API, JSON-RPC, and web editor surfaces are experimental/inventory unless separately promoted
- **Header-heavy core** - many implementations are inline in .h files
- **Rendering design surface** - software/headless rendering is tested; generic Vulkan/DX12 and advanced path-tracing features remain experimental/unavailable unless explicitly promoted

### Next Steps (Blueprint Steps 40-44)
- Step 40: Skeletal Rigging (Bone Hierarchy) ✅ in litt_engine_systems.h
- Step 41: Animation Blending ✅ in litt_engine_systems.h
- Step 42: Canvas UI ✅ in litt_engine_systems.h
- Step 43: Editor Tooling (AI Editor API exists in python/ + editor/)
- Step 44: Serialization ✅ in litt_serialization.h