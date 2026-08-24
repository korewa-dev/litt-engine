// Game entry point
#include "litt.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

using namespace litt;

int main(int argc, char** argv) {
    printf("Litt Engine Game\n");
    printf("Controls: WASD move, Space jump, ESC quit\n\n");
    
    // Parse args
    std::string scene;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--scene") == 0 && i+1 < argc) scene = argv[++i];
    }
    
    // Init
    Input in;
    in.load_defaults();
    WorldManager world;
    
    // Load scene
    if (!scene.empty()) {
        printf("Loading: %s\n", scene.c_str());
        world.load(scene);
    }
    
    // Game loop
    auto last = std::chrono::high_resolution_clock::now();
    while (1) {
        auto now = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration<float>(now - last).count();
        last = now;
        if (dt > 0.1f) dt = 0.1f;
        
        world.update(dt);
        world.input(in);
        
        if (world.state.game_over || world.state.won) break;
    }
    
    printf("Score: %u | Lives: %u\n", world.state.score, world.state.lives);
    return 0;
}
