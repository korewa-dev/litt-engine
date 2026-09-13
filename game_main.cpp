// ash-escape: functional top-down arena survival game
// Uses Litt Engine C sim for physics/AI/collision, GDI for rendering.
#include <windows.h>
#undef near
#undef far
#include "alt/src/native/littcore/litt_world.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>

// ---- Globals ----
static LvSession g_s;
static HWND g_hwnd = 0;
static int g_w = 900, g_h = 700;
static int g_keystates[256] = {};
static int g_mouse_x = 0, g_mouse_y = 0;
static int g_mouse_click = 0;
static HBITMAP g_bmp = 0;
static HDC g_memdc = 0;
static void* g_pixels = 0;
static int g_game_result = -1; // 0 lost, 1 won
static char g_status_msg[256] = "";

// ---- Helpers ----
static int keydown(int vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; }

static void draw_pixel(int x, int y, int r, int g, int b) {
    if (x < 0 || y < 0 || x >= g_w || y >= g_h) return;
    auto* p = static_cast<uint32_t*>(g_pixels);
    p[y * g_w + x] = (r << 16) | (g << 8) | b;
}

static void fill_rect(int x0, int y0, int x1, int y1, int r, int g, int b) {
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++)
            draw_pixel(x, y, r, g, b);
}

static void draw_circle(int cx, int cy, int rad, int r, int g, int b) {
    for (int y = -rad; y <= rad; y++)
        for (int x = -rad; x <= rad; x++)
            if (x * x + y * y <= rad * rad)
                draw_pixel(cx + x, cy + y, r, g, b);
}

static void draw_line(int x0, int y0, int x1, int y1, int r, int g, int b) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    while (true) {
        draw_pixel(x0, y0, r, g, b);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

// ---- Render ----
static void render(HDC hdc) {
    // Clear background
    fill_rect(0, 0, g_w, g_h, 18, 18, 24);

    // Camera: centered on player
    float px = g_s.pos[0], pz = g_s.pos[2];
    int cx0 = g_w / 2, cy0 = g_h / 2;
    float scale = 18.0f; // 1 world unit = 18 pixels

    auto w2s = [&](float wx, float wz) -> std::pair<int, int> {
        int sx = cx0 + (int)((wx - px) * scale);
        int sy = cy0 + (int)((wz - pz) * scale);
        return { sx, sy };
    };

    // Draw arena bounds
    int arena_r = 20;
    auto [ax0, ay0] = w2s(-arena_r, -arena_r);
    auto [ax1, ay1] = w2s(arena_r, arena_r);
    for (int x = ax0; x < ax1; x += 4) { draw_pixel(x, ay0, 35, 35, 45); draw_pixel(x, ay1, 35, 35, 45); }
    for (int y = ay0; y < ay1; y += 4) { draw_pixel(ax0, y, 35, 35, 45); draw_pixel(ax1, y, 35, 35, 45); }

    // Draw coins (yellow)
    for (int i = 0; i < g_s.ent_count; i++) {
        LvEnt* e = &g_s.ents[i];
        if (!e->alive) continue;
        if (e->flags & LV_F_SCORING) {
            auto [sx, sy] = w2s(e->pos[0], e->pos[2]);
            draw_circle(sx, sy, (int)(0.35f * scale), 255, 210, 40);
            draw_circle(sx, sy, (int)(0.18f * scale), 255, 240, 120);
        }
    }

    // Draw goal (green ring)
    for (int i = 0; i < g_s.ent_count; i++) {
        LvEnt* e = &g_s.ents[i];
        if (!e->alive) continue;
        if (e->flags & LV_F_GOAL) {
            auto [gx, gy] = w2s(e->pos[0], e->pos[2]);
            int gr = (int)(3.0f * scale);
            for (int a = 0; a < 360; a += 3) {
                float ang = a * 3.14159f / 180.0f;
                for (int t = -2; t <= 2; t++) {
                    int rr = gr + t;
                    int x = gx + (int)(cosf(ang) * rr);
                    int y = gy + (int)(sinf(ang) * rr);
                    draw_pixel(x, y, 60, 255, 80);
                }
            }
        }
    }

    // Draw enemies (red)
    for (int i = 0; i < g_s.ent_count; i++) {
        LvEnt* e = &g_s.ents[i];
        if (!e->alive) continue;
        if (e->flags & LV_F_ENEMY) {
            auto [ex, ey] = w2s(e->pos[0], e->pos[2]);
            int er = (int)(1.0f * scale);
            draw_circle(ex, ey, er, 220, 40, 40);
            draw_circle(ex, ey, (int)(0.5f * scale), 255, 80, 80);
            // Direction to player
            float dx = g_s.pos[0] - e->pos[0];
            float dz = g_s.pos[2] - e->pos[2];
            float len = sqrtf(dx * dx + dz * dz);
            if (len > 0.01f) {
                dx /= len; dz /= len;
                draw_line(ex, ey, ex + (int)(dx * er * 0.7f), ey + (int)(dz * er * 0.7f), 255, 200, 200);
            }
        }
    }

    // Draw player (cyan)
    {
        int pr = (int)(0.45f * scale);
        draw_circle(cx0, cy0, pr, 60, 200, 255);
        draw_circle(cx0, cy0, (int)(pr * 0.5f), 180, 255, 255);
    }

    // HUD
    char buf[256];
    snprintf(buf, sizeof(buf), "Lives: %u  Score: %u  Mode: %s", g_s.lives_left, g_s.score, lv_mode_name(g_s.cfg.mode));
    // Simple text (draw blocky letters)
    // ... use TextOut for HUD
    SetBkMode(g_memdc, TRANSPARENT);
    SetTextColor(g_memdc, RGB(255, 255, 255));
    TextOutA(g_memdc, 10, 10, buf, (int)strlen(buf));

    if (g_game_result == 0) {
        TextOutA(g_memdc, g_w / 2 - 60, g_h / 2, "GAME OVER", 9);
    } else if (g_game_result == 1) {
        TextOutA(g_memdc, g_w / 2 - 50, g_h / 2, "YOU WIN!", 8);
    }

    // Blit
    BitBlt(hdc, 0, 0, g_w, g_h, g_memdc, 0, 0, SRCCOPY);
}

// ---- Window proc ----
static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) PostQuitMessage(0);
        g_keystates[wp] = 1;
        return 0;
    case WM_KEYUP:
        g_keystates[wp] = 0;
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        render(hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

// ---- Entry ----
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow) {
    // Load scene + state
    const char* dir = "alt/Project/ash-escape";
    char path[1024];
    char* state = 0;
    snprintf(path, sizeof(path), "%s/world_state.json", dir);
    FILE* f = fopen(path, "rb");
    if (!f) { MessageBoxA(0, "Missing world_state.json", "Error", MB_OK); return 1; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    state = (char*)malloc(sz + 1);
    fread(state, 1, sz, f);
    state[sz] = 0;
    fclose(f);

    snprintf(path, sizeof(path), "%s/assets/scenes/world.lscn.json", dir);
    char models_dir[1024];
    snprintf(models_dir, sizeof(models_dir), "%s/assets/models", dir);

    if (lv_session_create(state, path, models_dir, &g_s)) {
        free(state);
        MessageBoxA(0, "lv_session_create failed", "Error", MB_OK);
        return 1;
    }
    free(state);

    // Window
    WNDCLASSA wc = {};
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = wndproc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(0, IDC_ARROW);
    wc.lpszClassName = "LittGame";
    RegisterClassA(&wc);

    RECT rc = { 0, 0, g_w, g_h };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    g_hwnd = CreateWindowA("LittGame", "Ash Escape - Litt Engine Demo",
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top,
        0, 0, hInst, 0);
    ShowWindow(g_hwnd, nCmdShow);

    // DIB section for pixel buffer
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = g_w;
    bmi.bmiHeader.biHeight = -g_h; // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    HDC hdc = GetDC(g_hwnd);
    g_bmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &g_pixels, 0, 0);
    g_memdc = CreateCompatibleDC(hdc);
    SelectObject(g_memdc, g_bmp);
    ReleaseDC(g_hwnd, hdc);

    // Game loop
    MSG msg = {};
    LARGE_INTEGER freq, last, now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&last);

    while (g_game_result < 0) {
        while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) goto done;
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }

        QueryPerformanceCounter(&now);
        float dt = (float)(now.QuadPart - last.QuadPart) / freq.QuadPart;
        last = now;
        if (dt > 0.05f) dt = 0.05f;

        // Input → sim
        float forward = 0, strafe = 0;
        int jump = 0;
        if (keydown('W') || keydown(VK_UP)) forward += 1;
        if (keydown('S') || keydown(VK_DOWN)) forward -= 1;
        if (keydown('D') || keydown(VK_RIGHT)) strafe += 1;
        if (keydown('A') || keydown(VK_LEFT)) strafe -= 1;
        if (keydown(VK_SPACE)) jump = 1;

        lv_step(&g_s, dt, forward, strafe, jump);

        if (g_s.game_over) g_game_result = 0;
        if (g_s.won) g_game_result = 1;

        // Render
        hdc = GetDC(g_hwnd);
        render(hdc);
        ReleaseDC(g_hwnd, hdc);

        Sleep(16);
    }

done:
    lv_session_free(&g_s);
    if (g_bmp) DeleteObject(g_bmp);
    if (g_memdc) DeleteDC(g_memdc);
    return 0;
}
