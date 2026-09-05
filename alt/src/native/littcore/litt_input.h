// LittInput - Lightweight input system
// Keyboard + mouse, action bindings

#pragma once
#include <unordered_map>
#include <vector>
#include <string>

namespace litt {

enum class Key : int {
    Unknown = 0,
    Enter = 13, Tab = 9,
    Space = 32,
    A = 65, B = 66, C = 67, D = 68, E = 69, F = 70, G = 71, H = 72,
    I = 73, J = 74, K = 75, L = 76, M = 77, N = 78, O = 79, P = 80, Q = 81, R = 82,
    S = 83, T = 84, U = 85, V = 86, W = 87, X = 88, Y = 89, Z = 90,
    Left = 256, Right = 257, Up = 258, Down = 259,
    // Distinct values: these previously collided with Left/Right, so pressing
    // an arrow also registered Escape/Shift.
    Escape = 260, Shift = 261, Ctrl = 262, Alt = 263,
    F1 = 290, F2 = 291, F3 = 292, F4 = 293, F5 = 294, F6 = 295,
    F7 = 296, F8 = 297, F9 = 298, F10 = 299, F11 = 300, F12 = 301
};

enum class Mouse : int { Left = 0, Right = 1, Middle = 2 };

class Input {
public:
    bool key_down(Key k) const { auto it = keys_.find((int)k); return it != keys_.end() && it->second; }
    bool key_pressed(Key k) const { auto it = just_.find((int)k); return it != just_.end() && it->second; }
    bool mouse_down(Mouse m) const { auto it = mkeys_.find((int)m); return it != mkeys_.end() && it->second; }
    bool mouse_pressed(Mouse m) const { auto it = mjust_.find((int)m); return it != mjust_.end() && it->second; }
    std::pair<double,double> mouse_pos() const { return {mx_, my_}; }
    std::pair<double,double> mouse_delta() const { return {mdx_, mdy_}; }
    double scroll() const { return mscl_; }
    
    bool action(const std::string& a) const {
        auto it = actions_.find(a);
        if (it == actions_.end()) return false;
        for (Key k : it->second.keys) if (key_down(k)) return true;
        for (Mouse m : it->second.mouses) if (mouse_down(m)) return true;
        return false;
    }
    
    bool action_pressed(const std::string& a) const {
        auto it = actions_.find(a);
        if (it == actions_.end()) return false;
        for (Key k : it->second.keys) if (key_pressed(k)) return true;
        for (Mouse m : it->second.mouses) if (mouse_pressed(m)) return true;
        return false;
    }
    
    void update() {
        // Edge detection: "just" = down now but not down at the previous
        // update(). Copying keys_ verbatim made key_pressed fire every frame
        // a key was held, breaking press-edge semantics (jump, UI clicks).
        just_.clear();
        for (const auto& kv : keys_) {
            auto prev = prev_keys_.find(kv.first);
            if (kv.second && (prev == prev_keys_.end() || !prev->second))
                just_[kv.first] = true;
        }
        prev_keys_ = keys_;
        mjust_.clear();
        for (const auto& kv : mkeys_) {
            auto prev = prev_mkeys_.find(kv.first);
            if (kv.second && (prev == prev_mkeys_.end() || !prev->second))
                mjust_[kv.first] = true;
        }
        prev_mkeys_ = mkeys_;
        mdx_ = mdy_ = 0;
        mscl_ = 0;
    }
    
    // Mutators (distinct names from queries)
    void press(Key k) { keys_[(int)k] = true; }
    void release(Key k) { keys_[(int)k] = false; }
    void mouse_press(Mouse m) { mkeys_[(int)m] = true; }
    void mouse_release(Mouse m) { mkeys_[(int)m] = false; }
    void mouse_move(double x, double y) { mdx_ = x - mx_; mdy_ = y - my_; mx_ = x; my_ = y; }
    void scroll(double y) { mscl_ += y; }
    
    void bind(const std::string& action, Key k) { actions_[action].keys.push_back(k); }
    void bind(const std::string& action, Mouse m) { actions_[action].mouses.push_back(m); }
    
    void load_defaults() {
        bind("forward", Key::W); bind("forward", Key::Up);
        bind("backward", Key::S); bind("backward", Key::Down);
        bind("left", Key::A); bind("left", Key::Left);
        bind("right", Key::D); bind("right", Key::Right);
        bind("jump", Key::Space);
        bind("interact", Key::E);
        bind("attack", Mouse::Left);
        bind("pause", Key::Escape);
    }
    
private:
    std::unordered_map<int, bool> keys_;
    std::unordered_map<int, bool> prev_keys_;
    std::unordered_map<int, bool> just_;
    std::unordered_map<int, bool> mkeys_;
    std::unordered_map<int, bool> prev_mkeys_;
    std::unordered_map<int, bool> mjust_;
    double mx_ = 0, my_ = 0, mdx_ = 0, mdy_ = 0;
    double mscl_ = 0;
    struct Bind { std::vector<Key> keys; std::vector<Mouse> mouses; };
    std::unordered_map<std::string, Bind> actions_;
};

} // namespace litt
