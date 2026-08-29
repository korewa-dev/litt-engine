// LittInput - Working Input Implementation
// Keyboard and mouse input handling

#include "litt_input.h"
#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#elif defined(__linux__)
#include <X11/Xlib.h>
#endif

namespace litt {

// =============================================================================
// Input Implementation
// =============================================================================

Input::Input() : mouse_x_(0), mouse_y_(0),
                 mouse_delta_x_(0), mouse_delta_y_(0),
                 mouse_scroll_(0) {
    // Initialize key states
    for (int i = 0; i < 512; ++i) {
        keys_[i] = false;
        just_[i] = false;
    }
    
    // Initialize mouse states
    for (int i = 0; i < 3; ++i) {
        mkeys_[i] = false;
        mjust_[i] = false;
    }
}

bool Input::key_down(Key k) const {
    auto it = keys_.find(static_cast<int>(k));
    return it != keys_.end() && it->second;
}

bool Input::key_pressed(Key k) const {
    auto it = just_.find(static_cast<int>(k));
    return it != just_.end() && it->second;
}

bool Input::mouse_down(Mouse m) const {
    auto it = mkeys_.find(static_cast<int>(m));
    return it != mkeys_.end() && it->second;
}

bool Input::mouse_pressed(Mouse m) const {
    auto it = mjust_.find(static_cast<int>(m));
    return it != mjust_.end() && it->second;
}

std::pair<double, double> Input::mouse_pos() const {
    return {static_cast<double>(mouse_x_), static_cast<double>(mouse_y_)};
}

std::pair<double, double> Input::mouse_delta() const {
    return {static_cast<double>(mouse_delta_x_), static_cast<double>(mouse_delta_y_)};
}

double Input::scroll() const {
    return mouse_scroll_;
}

bool Input::action(const std::string& a) const {
    auto it = actions_.find(a);
    if (it == actions_.end()) return false;
    
    for (Key k : it->second.keys) {
        if (key_down(k)) return true;
    }
    for (Mouse m : it->second.mouses) {
        if (mouse_down(m)) return true;
    }
    
    return false;
}

void Input::update() {
    // Clear "just pressed" states
    for (auto& [key, pressed] : just_) {
        pressed = false;
    }
    for (auto& [button, pressed] : mjust_) {
        pressed = false;
    }
}

void Input::set_key(Key k, bool down) {
    int key_code = static_cast<int>(k);
    
    if (down && !keys_[key_code]) {
        just_[key_code] = true;
    }
    
    keys_[key_code] = down;
}

void Input::set_mouse(Mouse m, bool down) {
    int mouse_code = static_cast<int>(m);
    
    if (down && !mkeys_[mouse_code]) {
        mjust_[mouse_code] = true;
    }
    
    mkeys_[mouse_code] = down;
}

void Input::set_mouse_pos(double x, double y) {
    mouse_delta_x_ = x - mouse_x_;
    mouse_delta_y_ = y - mouse_y_;
    
    mouse_x_ = static_cast<float>(x);
    mouse_y_ = static_cast<float>(y);
}

void Input::set_scroll(double delta) {
    mouse_scroll_ += delta;
}

void Input::bind_action(const std::string& action, Key key) {
    ActionBinding binding;
    binding.keys.push_back(key);
    actions_[action] = binding;
}

void Input::bind_action(const std::string& action, Mouse mouse) {
    ActionBinding binding;
    binding.mouses.push_back(mouse);
    actions_[action] = binding;
}

void Input::bind_action(const std::string& action, const std::vector<Key>& keys) {
    ActionBinding binding;
    binding.keys = keys;
    actions_[action] = binding;
}

void Input::bind_action(const std::string& action, const std::vector<Mouse>& mouses) {
    ActionBinding binding;
    binding.mouses = mouses;
    actions_[action] = binding;
}

// =============================================================================
// Platform Input (Windows)
// =============================================================================

#ifdef _WIN32

void Input::poll_window_messages() {
    MSG msg;
    
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        switch (msg.message) {
            case WM_KEYDOWN:
            case WM_KEYUP: {
                Key key = vk_to_key(msg.wParam);
                bool down = (msg.message == WM_KEYDOWN);
                set_key(key, down);
                break;
            }
            
            case WM_LBUTTONDOWN:
            case WM_LBUTTONUP:
                set_mouse(Mouse::Left, msg.message == WM_LBUTTONDOWN);
                break;
                
            case WM_RBUTTONDOWN:
            case WM_RBUTTONUP:
                set_mouse(Mouse::Right, msg.message == WM_RBUTTONDOWN);
                break;
                
            case WM_MBUTTONDOWN:
            case WM_MBUTTONUP:
                set_mouse(Mouse::Middle, msg.message == WM_MBUTTONDOWN);
                break;
                
            case WM_MOUSEMOVE: {
                short x = LOWORD(msg.lParam);
                short y = HIWORD(msg.lParam);
                set_mouse_pos(x, y);
                break;
            }
            
            case WM_MOUSEWHEEL: {
                short delta = GET_WHEEL_DELTA_WPARAM(msg.wParam);
                set_scroll(delta / 120.0f);
                break;
            }
        }
    }
}

Key Input::vk_to_key(WPARAM vk) {
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
            if (vk >= 'A' && vk <= 'Z') return static_cast<Key>(vk);
            if (vk >= '0' && vk <= '9') return static_cast<Key>(vk - '0' + 48);
            return Key::Unknown;
    }
}

#endif // _WIN32

// =============================================================================
// Platform Input (Linux/X11)
// =============================================================================

#ifdef __linux__

void Input::poll_window_messages(Display* display, Window window) {
    XEvent event;
    
    while (XPending(display)) {
        XNextEvent(display, &event);
        
        switch (event.type) {
            case KeyPress:
            case KeyRelease: {
                Key key = xkey_to_key(event.xkey.keycode);
                bool down = (event.type == KeyPress);
                set_key(key, down);
                break;
            }
            
            case ButtonPress:
            case ButtonRelease: {
                Mouse mouse = xbutton_to_mouse(event.xbutton.button);
                bool down = (event.type == ButtonPress);
                set_mouse(mouse, down);
                break;
            }
            
            case MotionNotify: {
                set_mouse_pos(event.xmotion.x, event.xmotion.y);
                break;
            }
            
            case ScrollUp:
            case ScrollDown:
            case ScrollLeft:
            case ScrollRight: {
                double delta = 0;
                switch (event.type) {
                    case ScrollUp: delta = 1.0; break;
                    case ScrollDown: delta = -1.0; break;
                    case ScrollLeft: delta = -1.0; break;
                    case ScrollRight: delta = 1.0; break;
                }
                set_scroll(delta);
                break;
            }
        }
    }
}

Key Input::xkey_to_key(unsigned int keycode) {
    // X11 keycodes vary by keyboard layout
    // This is a simplified mapping
    switch (keycode) {
        case 36: return Key::Return; // Enter
        case 23: return Key::Tab;   // Tab
        case 65: return Key::Space; // Space
        case 9: return Key::Escape; // Escape
        case 50: return Key::Shift; // Shift
        case 37: return Key::Ctrl;  // Control
        case 64: return Key::Alt;   // Alt
        case 113: return Key::Left; // Left arrow
        case 114: return Key::Right;// Right arrow
        case 111: return Key::Up;   // Up arrow
        case 116: return Key::Down; // Down arrow
        default:
            // Try to map ASCII
            if (keycode >= 24 && keycode <= 53) {
                return static_cast<Key>('A' + (keycode - 24));
            }
            return Key::Unknown;
    }
}

Mouse Input::xbutton_to_mouse(unsigned int button) {
    switch (button) {
        case 1: return Mouse::Left;
        case 2: return Mouse::Middle;
        case 3: return Mouse::Right;
        default: return Mouse::Left;
    }
}

#endif // __linux__

} // namespace litt
