// Windows Input Backend - Real keyboard, mouse, and gamepad polling
// Uses Windows Raw Input API for low-latency input

#pragma once
#include "litt_input.h"
#include <windows.h>
#include <XInput.h>
#include <cmath>

namespace litt {

class WindowsInput {
public:
    WindowsInput() = default;
    ~WindowsInput() { shutdown(); }
    
    bool init(HWND hwnd) {
        hwnd_ = hwnd;
        
        // Register raw input devices
        RAWINPUTDEVICE rid[3];
        
        // Keyboard
        rid[0].usUsagePage = 0x01;
        rid[0].usUsage = 0x06;
        rid[0].dwFlags = RIDEV_INPUTSINK | RIDEV_NOLEGACY;
        rid[0].hwndTarget = hwnd;
        
        // Mouse
        rid[1].usUsagePage = 0x01;
        rid[1].usUsage = 0x02;
        rid[1].dwFlags = RIDEV_INPUTSINK;
        rid[1].hwndTarget = hwnd;
        
        // Gamepad
        rid[2].usUsagePage = 0x01;
        rid[2].usUsage = 0x05;
        rid[2].dwFlags = RIDEV_INPUTSINK;
        rid[2].hwndTarget = hwnd;
        
        if (!RegisterRawInputDevices(rid, 3, sizeof(RAWINPUTDEVICE))) {
            return false;
        }
        
        return true;
    }
    
    void shutdown() {
        hwnd_ = nullptr;
    }
    
    void poll() {
        // Poll gamepad
        pollGamepad();
        
        // Clear per-frame state
        scroll_delta_ = 0;
    }
    
    void handleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
        switch (msg) {
            case WM_INPUT: {
                UINT size = 0;
                GetRawInputData((HRAWINPUT)lParam, RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER));
                if (size == 0) break;
                
                std::vector<BYTE> data(size);
                if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, data.data(), &size, sizeof(RAWINPUTHEADER)) != size) break;
                
                RAWINPUT* raw = (RAWINPUT*)data.data();
                
                if (raw->header.dwType == RIM_TYPEKEYBOARD) {
                    handleKeyboard(raw->data.keyboard);
                } else if (raw->header.dwType == RIM_TYPEMOUSE) {
                    handleMouse(raw->data.mouse);
                }
                break;
            }
            case WM_MOUSEWHEEL: {
                scroll_delta_ += GET_WHEEL_DELTA_WPARAM(wParam) / (float)WHEEL_DELTA;
                break;
            }
        }
    }
    
    // Get the current input state
    const Input& getState() const { return state_; }
    Input& getState() { return state_; }
    
private:
    void handleKeyboard(const RAWKEYBOARD& kb) {
        int key = kb.VKey;
        bool down = !(kb.Flags & RI_KEY_BREAK);
        
        // Map Windows VK codes to our Key enum
        Key mapped = mapVKToKey(key);
        if (mapped != Key::Unknown) {
            if (down) state_.press(mapped);
            else state_.release(mapped);
        }
    }
    
    void handleMouse(const RAWMOUSE& mouse) {
        // Mouse buttons
        if (mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_DOWN) state_.mouse_press(Mouse::Left);
        if (mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_UP) state_.mouse_release(Mouse::Left);
        if (mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_DOWN) state_.mouse_press(Mouse::Right);
        if (mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_UP) state_.mouse_release(Mouse::Right);
        if (mouse.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_DOWN) state_.mouse_press(Mouse::Middle);
        if (mouse.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_UP) state_.mouse_release(Mouse::Middle);
        
        // Mouse movement
        if (mouse.lLastX != 0 || mouse.lLastY != 0) {
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(hwnd_, &pt);
            state_.mouse_move(pt.x, pt.y);
        }
    }
    
    void pollGamepad() {
        XINPUT_STATE xstate;
        if (XInputGetState(0, &xstate) == ERROR_SUCCESS) {
            // Map gamepad buttons
            if (xstate.Gamepad.wButtons & XINPUT_GAMEPAD_A) state_.press(Key::Space);
            if (xstate.Gamepad.wButtons & XINPUT_GAMEPAD_B) state_.press(Key::Escape);
            if (xstate.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP) state_.press(Key::Up);
            if (xstate.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) state_.press(Key::Down);
            if (xstate.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) state_.press(Key::Left);
            if (xstate.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) state_.press(Key::Right);
        }
    }
    
    Key mapVKToKey(int vk) {
        switch (vk) {
            case VK_RETURN: return Key::Enter;
            case VK_TAB: return Key::Tab;
            case VK_SPACE: return Key::Space;
            case VK_ESCAPE: return Key::Escape;
            case VK_SHIFT: return Key::Shift;
            case VK_CONTROL: return Key::Ctrl;
            case VK_MENU: return Key::Alt;
            case VK_LEFT: return Key::Left;
            case VK_RIGHT: return Key::Right;
            case VK_UP: return Key::Up;
            case VK_DOWN: return Key::Down;
            case VK_F1: return Key::F1;
            case VK_F2: return Key::F2;
            case VK_F3: return Key::F3;
            case VK_F4: return Key::F4;
            case VK_F5: return Key::F5;
            case VK_F6: return Key::F6;
            case VK_F7: return Key::F7;
            case VK_F8: return Key::F8;
            case VK_F9: return Key::F9;
            case VK_F10: return Key::F10;
            case VK_F11: return Key::F11;
            case VK_F12: return Key::F12;
            default:
                if (vk >= 'A' && vk <= 'Z') return (Key)(vk);
                if (vk >= '0' && vk <= '9') return (Key)(vk);
                return Key::Unknown;
        }
    }
    
    HWND hwnd_ = nullptr;
    Input state_;
    float scroll_delta_ = 0;
};

} // namespace litt
