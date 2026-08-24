// Working C++ consumer of the Litt FFI bridge.
// Build & run via scripts/ffi_demo.ps1 (uses MSVC from D:\Program Files\Program).

#include "litt_ffi.h"
#include <cstdio>
#include <cstring>

int main(int argc, char** argv) {
    const char* scene = argc > 1 ? argv[1]
        : "Project/kingsfall-hollow/assets/scenes/world.lscn.json";
    const char* base = argc > 2 ? argv[2]
        : "Project/kingsfall-hollow/assets";

    std::printf("bridge: %s\n", litt_version());

    char err[256] = {0};
    LittWorld* world = litt_deploy_world(scene, base, err);
    if (!world) {
        std::fprintf(stderr, "deploy failed: %s\n", err);
        return 1;
    }

    std::printf("deployed: %zu triangles, %zu spheres, %zu meshes\n",
                litt_world_triangles(world),
                litt_world_spheres(world),
                litt_world_meshes(world));

    int missing = litt_world_missing_count(world);
    for (int i = 0; i < missing; ++i) {
        char buf[128];
        if (litt_world_missing_at(world, i, buf, sizeof(buf)) > 0) {
            std::printf("missing model: %s\n", buf);
        }
    }

    litt_world_free(world);
    return missing == 0 ? 0 : 2;
}
