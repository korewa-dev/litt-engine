// Concrete Echoes — GTA IV Clone
// Built using Litt Engine APIs (corrected to match actual header signatures)
#include "litt_gpu_software.h"
#include "litt_ecs.h"
#include "litt_physics.h"
#include "litt_input.h"
#include "litt_scene.h"
#include "litt_math.h"
#include "litt_ui.h"
#include "litt_profiler.h"
#include "litt_config.h"
#include "litt_collision.h"

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <string>

using namespace litt;

// Global game state
static SoftwareRenderer* renderer = nullptr;
static Input* input = nullptr;
static World* world = nullptr;

// Player state
static Entity playerEntity;
static Vec3 playerPos = Vec3(0, 0, 5);
static Vec3 playerVel = Vec3::zero();
static float playerYaw = 0.0f;
static float playerHealth = 100.0f;
static int cash = 500;
static int wantedLevel = 0;
static bool running = true;

// Camera
static Vec3 cameraPos;
static Vec3 cameraTarget;
static Mat4 viewMatrix;
static Mat4 projMatrix;

void updatePlayer(float dt) {
    float mx = 0.0f, mz = 0.0f;
    if (input->key_down(Key::W)) mz += 1.0f;
    if (input->key_down(Key::S)) mz -= 1.0f;
    if (input->key_down(Key::A)) mx -= 1.0f;
    if (input->key_down(Key::D)) mx += 1.0f;
    
    float len = std::sqrt(mx*mx + mz*mz);
    if (len > 0.0f) { mx /= len; mz /= len; }
    
    float speed = input->key_down(Key::Shift) ? 8.0f : 5.5f;
    playerVel.x = mx * speed;
    playerVel.z = mz * speed;
    
    playerPos.x += playerVel.x * dt;
    playerPos.z += playerVel.z * dt;
    
    if (len > 0.0f) playerYaw = std::atan2(mx, mz);
    
    if (input->key_pressed(Key::Space) && playerPos.y <= 0.01f)
        playerVel.y = 6.0f;
    
    playerVel.y += -9.81f * dt;
    playerPos.y += playerVel.y * dt;
    if (playerPos.y < 0.0f) { playerPos.y = 0.0f; playerVel.y = 0.0f; }
}

void updateCamera() {
    float cx = playerPos.x - std::sin(playerYaw) * 8.0f;
    float cz = playerPos.z - std::cos(playerYaw) * 8.0f;
    float cy = playerPos.y + 4.0f;
    cameraPos = Vec3(cx, cy, cz);
    cameraTarget = playerPos + Vec3(0, 1.5f, 0);
    viewMatrix = Mat4::look_at(cameraPos, cameraTarget, Vec3(0, 1, 0));
    projMatrix = Mat4::perspective(60.0f * 3.14159f / 180.0f, 16.0f/9.0f, 0.1f, 1000.0f);
}

void renderWorld() {
    renderer->clear(0x646E78);
    
    // Ground grid
    for (int x = -20; x <= 20; x++) {
        for (int z = -20; z <= 20; z++) {
            int sx, sy; float depth;
            renderer->project(Vec3(x*5.0f, 0, z*5.0f), viewMatrix * projMatrix, sx, sy, depth);
            if (sx >= 0 && sx < 800 && sy >= 0 && sy < 600)
                renderer->draw_rect(sx-2, sy-2, 4, 4, 0x505050);
        }
    }
    
    // Player (green)
    {
        int sx, sy; float depth;
        renderer->project(playerPos + Vec3(0,1,0), viewMatrix * projMatrix, sx, sy, depth);
        if (sx >= 0 && sx < 800 && sy >= 0 && sy < 600)
            renderer->fill_rect(sx-8, sy-16, 16, 32, 0x00C800);
    }
    
    // Buildings
    struct B { Vec3 pos; float w, h, d; };
    B buildings[] = {{{-15,0,-15},4,8,4}, {{15,0,-10},6,12,5}, {{-10,0,20},5,6,5}, {{20,0,15},8,15,6}};
    for (auto& b : buildings) {
        int sx, sy; float depth;
        renderer->project(b.pos + Vec3(0, b.h/2, 0), viewMatrix * projMatrix, sx, sy, depth);
        if (sx >= -50 && sx < 850 && sy >= -50 && sy < 650)
            renderer->fill_rect(sx - (int)(b.w*1.5f), sy - (int)(b.h*5), (int)(b.w*3), (int)(b.h*5), 0x787880);
    }
}

void renderHUD() {
    renderer->fill_rect(20, 20, 200, 20, 0x323232);
    renderer->fill_rect(20, 20, (int)(2.0f * playerHealth), 20, 0x00C800);
    
    std::string cashStr = "$" + std::to_string(::cash);
    renderer->draw_text(600, 20, cashStr, 0xFFFF00);
    
    for (int i = 0; i < wantedLevel; i++)
        renderer->fill_rect(600 + i * 25, 50, 20, 20, 0xFFFF00);
    
    int cx = 400, cy = 300;
    renderer->draw_line(cx-10, cy, cx+10, cy, 0xFFFFFF);
    renderer->draw_line(cx, cy-10, cx, cy+10, 0xFFFFFF);
    
    renderer->draw_text(20, 560, "WASD: Move | Shift: Sprint | Space: Jump | ESC: Quit", 0xC8C8C8);
}

int main() {
    printf("=====================================\n");
    printf("  CONCRETE ECHOES — GTA IV Clone\n");
    printf("  Built with Litt Engine\n");
    printf("=====================================\n\n");
    
    // Initialize renderer (headless for testing)
    renderer = new SoftwareRenderer();
    if (!renderer->initialize("headless")) {
        printf("Failed to initialize renderer\n");
        return 1;
    }
    
    input = new Input();
    world = new World();
    
    // Create player entity
    playerEntity = world->create();
    printf("Player entity created: id=%u\n", playerEntity.id);
    
    // Add transform component
    Transform transform;
    transform.position = playerPos;
    transform.scale = Vec3(1,1,1);
    world->add<Transform>(playerEntity, transform);
    
    printf("Concrete Echoes initialized\n");
    printf("Starting game loop...\n\n");
    
    int frameCount = 0;
    float dt = 1.0f / 60.0f;
    
    while (running) {
        Profiler::get_instance().begin_sample("frame");
        input->update();
        
        if (input->key_pressed(Key::Escape)) running = false;
        
        updatePlayer(dt);
        updateCamera();
        renderWorld();
        renderHUD();
        renderer->present();
        
        Profiler::get_instance().end_sample("frame");
        Profiler::get_instance().update_fps();
        frameCount++;
        
        if (frameCount % 60 == 0)
            printf("Frame %d, FPS: %.1f\n", frameCount, Profiler::get_instance().get_fps());
    }
    
    printf("\nShutting down...\n");
    delete renderer;
    delete input;
    delete world;
    printf("Goodbye!\n");
    return 0;
}
