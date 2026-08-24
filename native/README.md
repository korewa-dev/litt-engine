# Litt Engine C++ Core

Lightweight, high-performance game engine core in C++17. Header-only modules,
zero external dependencies.

## Verified Status

| Check | Result |
|-------|--------|
| Unit tests (`tests.cpp`) | **23/23 PASS** |
| Umbrella header `litt.h` | compiles clean (`g++ -std=c++17 -fsyntax-only`) |
| Council self-test (`council_demo.cpp`) | tiers, weighted votes, quorum, overrides all pass |
| Benchmarks (`benchmarks.cpp`) | see table below |

```text
 GROUP  OPERATION      LIB ns/op    RAW ns/op    OVERHEAD
---------------------------------------------------------
 Vec3   add                1.645        1.669       0.99x
        mul                1.711        1.736       0.99x
        dot                1.735        1.630       1.06x
        cross              3.704        2.939       1.26x
        normalize          6.797        4.993       1.36x   <- safety epsilon check
 Mat4   multiply          10.792       15.382       0.70x   <- faster than raw
        transform          4.774        6.053       0.79x
 Quat   multiply           7.792        8.752       0.89x
 AABB   intersects         ~4.5         ~4.8        0.94x
```

## Modules (`native/littcore/`)

| File | Purpose |
|------|---------|
| `litt_math.h` | Vec2/3/4, Mat4, Quat, Aabb, Ray, raycasts, lerp/clamp/smoothstep |
| `litt_ecs.h` | Entity + typed component storage (O(1) add/remove), systems |
| `litt_input.h` | Keyboard/mouse state, action bindings |
| `litt_world.h` | Game world sim: gravity, enemies, goals, win/lose |
| `litt_council.h` | Compile-time feature flags + runtime weighted-vote council |
| `litt_scene.h` | Scene graph with hierarchical transforms |
| `litt_audio.h` | Clip/source/listener management (backend stub) |
| `litt_config.h` | Key/value Settings store + quality presets |
| `litt_ui.h`, `litt_profiler.h` | UI helpers, profiler |

Executables / tests: `tests.cpp`, `benchmarks.cpp`, `council_demo.cpp`,
`game.cpp`, `litteditor.cpp`.

## Build & Test

```powershell
cd native
.\run_tests.ps1          # build + run unit tests (Windows)

# Linux/macOS
./run_tests.sh
```

Manual compile of anything:

```bash
g++ -std=c++17 -O2 -I. <file.cpp> -o <out>
```

## Usage

```cpp
#include "litt.h"

using namespace litt;

int main() {
    // Math
    Mat4 view = Mat4::look_at(Vec3(0,5,-10), Vec3::zero(), Vec3::up());
    Vec3 v = view * Vec3(1, 2, 3);            // world -> camera space

    // ECS
    World w;
    auto e  = w.create();
    w.add<Transform>(e, Transform{Vec3(1,2,3)});
    auto* t = w.get<Transform>(e);

    // Input (mutators are press/release; queries are key_down/action)
    Input in;
    in.load_defaults();
    if (in.action("jump")) {}

    // Council: decide which features load
    Council c;
    c.apply_tier(Tier::High);
    c.add_voter({"lead", 3});
    c.vote("lead", Feature::Renderer, Vote::Yes);
    bool gfx = c.decide(Feature::Renderer);

    return 0;
}
```

## API Notes (learned the hard way — kept accurate)

- `Input`: queries `key_down/key_pressed/mouse_down/...`; mutators are
  `press/release/mouse_press/mouse_release` (no query/mutator name collisions).
- `Mat4::look_at` builds a proper **view** matrix (camera axes as rows);
  `m * eye == origin`.
- `World` (ECS) and `WorldState`/`WorldManager` (world sim) are distinct;
  `Config` (game rules, in `litt_world.h`) vs `Settings` (key/value store, in
  `litt_config.h`) are distinct.
- Compile-time feature cuts: `-DLITT_ENABLE_RENDERER=0` etc.

## License

See ../LICENSE
