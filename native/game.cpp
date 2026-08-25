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

#ifdef _WIN32
// Real keyboard polling so the controls printed below actually work; the
// Input object used to sit empty forever, and the loop also exited on the
// first tick because the unsigned lives counter read 0.
static void poll_input(Input& in) {
    auto apply = [&in](int vk, Key k) {
        if ((GetAsyncKeyState(vk) & 0x8000) != 0) in.press(k);
        else in.release(k);
    };
    apply('W', Key::W); apply('S', Key::S); apply('A', Key::A); apply('D', Key::D);
    apply(VK_UP, Key::Up); apply(VK_DOWN, Key::Down);
    apply(VK_LEFT, Key::Left); apply(VK_RIGHT, Key::Right);
    apply(VK_SPACE, Key::Space);
}
#endif

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
#ifndef _WIN32
    int frames = 0; // no input backend on this platform: bounded demo run
#endif
    while (1) {
        auto now = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration<float>(now - last).count();
        last = now;
        if (dt > 0.1f) dt = 0.1f;

#ifdef _WIN32
        poll_input(in);
#endif
        in.update();          // fold press events into just-pressed edges
        world.update(dt);
        world.input(in);

#ifdef _WIN32
        if (in.key_down(Key::Escape)) break;
#else
        if (++frames > 600) break;
#endif
        if (world.state.game_over || world.state.won) break;
    }

    printf("Score: %u | Lives: %u\n", world.state.score, world.state.lives);
    return 0;
}
