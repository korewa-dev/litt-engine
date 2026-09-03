// LittUI - User interface system for Litt Engine

#pragma once
#include "litt_math.h"
#include <algorithm>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>

namespace litt {

enum class UIElementKind {
    Button,
    Text,
    Image,
    Slider,
    Checkbox,
    TextField,
    Label,
    Panel,
    Window,
    ScrollBar
};

struct UIStyle {
    Vec3 backgroundColor = Vec3{0.1f, 0.1f, 0.1f};
    Vec3 textColor = Vec3{1.0f, 1.0f, 1.0f};
    Vec3 hoverColor = Vec3{0.2f, 0.2f, 0.3f};
    Vec3 activeColor = Vec3{0.3f, 0.3f, 0.4f};
    float cornerRadius = 4.0f;
    float borderWidth = 1.0f;
    Vec3 borderColor = Vec3{0.5f, 0.5f, 0.5f};
    float fontSize = 14.0f;
    std::string fontFamily = "Arial";
};

struct UIRect {
    Vec2 position; // top-left
    Vec2 size;
    Vec2 anchoredPos;
    Vec2 anchoredSize;
    bool visible = true;
    bool interactable = true;
    
    bool contains(const Vec2& point) const {
        return point.x >= position.x && point.x <= position.x + size.x &&
               point.y >= position.y && point.y <= position.y + size.y;
    }
};

class UIElementBase {
public:
    virtual ~UIElementBase() = default;
    virtual void render() = 0;
    virtual void update(float dt) = 0;
    virtual void onMouseDown(const Vec2&) {}
    virtual void onMouseUp(const Vec2&) {}
    virtual void onMouseMove(const Vec2&) {}
    virtual void onKeyPress(char) {}

    UIRect& rect() { return rect_; }
    const UIRect& rect() const { return rect_; }
    
    bool isHovered() const { return hovered_; }
    bool isPressed() const { return pressed_; }
    
protected:
    UIRect rect_;
    bool hovered_ = false;
    bool pressed_ = false;
    UIStyle style_;
};

class UIButton : public UIElementBase {
public:
    UIButton(const std::string& text, const Vec2& pos, const Vec2& size) {
        rect_.position = pos;
        rect_.size = size;
        text_ = text;
    }
    
    void onClick(std::function<void()> callback) {
        onClickCallback_ = callback;
    }
    
    void render() override {
        // Render button
    }

    void update(float) override {
        // Update button state
    }

    void onMouseDown(const Vec2& pos) override {
        if (rect_.contains(pos)) {
            pressed_ = true;
            if (onClickCallback_) onClickCallback_();
        }
    }

    void onMouseUp(const Vec2&) override {
        pressed_ = false;
    }
    
private:
    std::string text_;
    std::function<void()> onClickCallback_;
};

class UIText : public UIElementBase {
public:
    UIText(const std::string& text, const Vec2& pos, const Vec2& size = Vec2{100, 30}) {
        rect_.position = pos;
        rect_.size = size;
        text_ = text;
    }
    
    void setText(const std::string& text) {
        text_ = text;
    }
    
    const std::string& getText() const { return text_; }
    
    void render() override {
        // Render text
    }

    void update(float) override {}
    
private:
    std::string text_;
};

class UISlider : public UIElementBase {
public:
    UISlider(const Vec2& pos, const Vec2& size, float min = 0.0f, float max = 1.0f) {
        rect_.position = pos;
        rect_.size = size;
        minValue_ = min;
        maxValue_ = max;
        value_ = min;
    }
    
    float getValue() const { return value_; }
    void setValue(float value) {
        value_ = std::clamp(value, minValue_, maxValue_);
        if (onValueChanged_) onValueChanged_(value_);
    }
    
    void onValueChanged(std::function<void(float)> callback) {
        onValueChanged_ = callback;
    }
    
    void render() override {
        // Render slider
    }

    void update(float) override {
        // Update slider
    }
    
    void onMouseDown(const Vec2& pos) override {
        if (rect_.contains(pos)) {
            pressed_ = true;
            updateFromPosition(pos);
        }
    }
    
    void onMouseMove(const Vec2& pos) override {
        if (pressed_) {
            updateFromPosition(pos);
        }
    }
    
private:
    void updateFromPosition(const Vec2& pos) {
        float t = (pos.x - rect_.position.x) / rect_.size.x;
        t = std::clamp(t, 0.0f, 1.0f);
        setValue(minValue_ + t * (maxValue_ - minValue_));
    }
    
    float minValue_ = 0.0f;
    float maxValue_ = 1.0f;
    float value_ = 0.5f;
    std::function<void(float)> onValueChanged_;
};

class UIWindow : public UIElementBase {
public:
    UIWindow(const std::string& title, const Vec2& pos, const Vec2& size) {
        rect_.position = pos;
        rect_.size = size;
        title_ = title;
    }
    
    void addElement(std::shared_ptr<UIElementBase> element) {
        elements_.push_back(element);
    }
    
    void render() override {
        // Render window
        for (auto& elem : elements_) {
            elem->render();
        }
    }
    
    void update(float dt) override {
        for (auto& elem : elements_) {
            elem->update(dt);
        }
    }
    
    void onMouseDown(const Vec2& pos) override {
        for (auto& elem : elements_) {
            elem->onMouseDown(pos);
        }
    }
    
    void onMouseMove(const Vec2& pos) override {
        for (auto& elem : elements_) {
            elem->onMouseMove(pos);
        }
    }
    
private:
    std::string title_;
    std::vector<std::shared_ptr<UIElementBase>> elements_;
};

class UIManager {
public:
    UIManager() = default;
    
    void addWindow(std::shared_ptr<UIWindow> window) {
        windows_.push_back(window);
    }
    
    void removeWindow(UIWindow* window) {
        windows_.erase(
            std::remove_if(windows_.begin(), windows_.end(),
                [window](const std::shared_ptr<UIWindow>& w) { return w.get() == window; }),
            windows_.end());
    }
    
    void render() {
        for (auto& window : windows_) {
            window->render();
        }
    }
    
    void update(float dt) {
        for (auto& window : windows_) {
            window->update(dt);
        }
    }
    
    void onMouseDown(const Vec2& pos) {
        for (auto it = windows_.rbegin(); it != windows_.rend(); ++it) {
            (*it)->onMouseDown(pos);
        }
    }
    
    void onMouseMove(const Vec2& pos) {
        for (auto it = windows_.rbegin(); it != windows_.rend(); ++it) {
            (*it)->onMouseMove(pos);
        }
    }
    
private:
    std::vector<std::shared_ptr<UIWindow>> windows_;
};

} // namespace litt
