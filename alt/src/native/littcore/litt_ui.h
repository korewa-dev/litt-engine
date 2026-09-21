// LittUI - portable retained-mode UI with deterministic draw commands
#pragma once

#include "litt_math.h"
#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace litt {

enum class UIElementKind {
    Button, Text, Image, Slider, Checkbox, TextField, Label, Panel, Window, ScrollBar
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
    Vec2 position;
    Vec2 size;
    Vec2 anchoredPos;
    Vec2 anchoredSize;
    bool visible = true;
    bool interactable = true;

    bool contains(const Vec2& point) const {
        return visible && point.x >= position.x && point.x <= position.x + size.x &&
               point.y >= position.y && point.y <= position.y + size.y;
    }
};

struct UIDrawCommand {
    UIElementKind kind = UIElementKind::Panel;
    UIRect rect{};
    Vec3 color = Vec3::zero();
    Vec3 secondaryColor = Vec3::zero();
    std::string text;
    float value = 0.0f;
};

class UIElementBase {
public:
    virtual ~UIElementBase() = default;

    void render() {
        draw_cache_.clear();
        append_draw_commands(draw_cache_);
    }

    virtual void update(float) {}
    virtual void append_draw_commands(std::vector<UIDrawCommand>& out) const = 0;

    virtual void onMouseDown(const Vec2&) {}
    virtual void onMouseUp(const Vec2&) { pressed_ = false; }
    virtual void onMouseMove(const Vec2& point) { hovered_ = rect_.contains(point); }
    virtual void onKeyPress(char) {}

    UIRect& rect() { return rect_; }
    const UIRect& rect() const { return rect_; }
    UIStyle& style() { return style_; }
    const UIStyle& style() const { return style_; }

    bool isHovered() const { return hovered_; }
    bool isPressed() const { return pressed_; }
    const std::vector<UIDrawCommand>& draw_commands() const { return draw_cache_; }

protected:
    UIRect rect_;
    bool hovered_ = false;
    bool pressed_ = false;
    UIStyle style_;
    std::vector<UIDrawCommand> draw_cache_;
};

class UIButton : public UIElementBase {
public:
    UIButton(const std::string& text, const Vec2& pos, const Vec2& size) : text_(text) {
        rect_.position = pos;
        rect_.size = size;
    }

    void onClick(std::function<void()> callback) { onClickCallback_ = std::move(callback); }
    void setText(const std::string& text) { text_ = text; }
    const std::string& getText() const { return text_; }

    void append_draw_commands(std::vector<UIDrawCommand>& out) const override {
        if (!rect_.visible) return;
        UIDrawCommand command;
        command.kind = UIElementKind::Button;
        command.rect = rect_;
        command.color = pressed_ ? style_.activeColor : (hovered_ ? style_.hoverColor : style_.backgroundColor);
        command.secondaryColor = style_.textColor;
        command.text = text_;
        out.push_back(std::move(command));
    }

    void onMouseDown(const Vec2& pos) override {
        if (rect_.interactable && rect_.contains(pos)) {
            pressed_ = true;
            if (onClickCallback_) onClickCallback_();
        }
    }

private:
    std::string text_;
    std::function<void()> onClickCallback_;
};

class UIText : public UIElementBase {
public:
    UIText(const std::string& text, const Vec2& pos, const Vec2& size = Vec2{100, 30}) : text_(text) {
        rect_.position = pos;
        rect_.size = size;
    }

    void setText(const std::string& text) { text_ = text; }
    const std::string& getText() const { return text_; }

    void append_draw_commands(std::vector<UIDrawCommand>& out) const override {
        if (!rect_.visible) return;
        UIDrawCommand command;
        command.kind = UIElementKind::Text;
        command.rect = rect_;
        command.color = style_.textColor;
        command.text = text_;
        out.push_back(std::move(command));
    }

private:
    std::string text_;
};

class UISlider : public UIElementBase {
public:
    UISlider(const Vec2& pos, const Vec2& size, float min = 0.0f, float max = 1.0f)
        : minValue_(std::min(min, max)), maxValue_(std::max(min, max)), value_(std::min(min, max)) {
        rect_.position = pos;
        rect_.size = size;
    }

    float getValue() const { return value_; }
    void setValue(float value) {
        const float next = std::clamp(value, minValue_, maxValue_);
        if (next == value_) return;
        value_ = next;
        if (onValueChanged_) onValueChanged_(value_);
    }
    void onValueChanged(std::function<void(float)> callback) { onValueChanged_ = std::move(callback); }

    void append_draw_commands(std::vector<UIDrawCommand>& out) const override {
        if (!rect_.visible) return;
        UIDrawCommand command;
        command.kind = UIElementKind::Slider;
        command.rect = rect_;
        command.color = style_.backgroundColor;
        command.secondaryColor = pressed_ ? style_.activeColor : style_.hoverColor;
        const float range = maxValue_ - minValue_;
        command.value = range > 0.0f ? (value_ - minValue_) / range : 0.0f;
        out.push_back(std::move(command));
    }

    void onMouseDown(const Vec2& pos) override {
        if (rect_.interactable && rect_.contains(pos)) {
            pressed_ = true;
            updateFromPosition(pos);
        }
    }

    void onMouseMove(const Vec2& pos) override {
        hovered_ = rect_.contains(pos);
        if (pressed_) updateFromPosition(pos);
    }

private:
    void updateFromPosition(const Vec2& pos) {
        if (rect_.size.x <= 0.0f) return;
        const float t = std::clamp((pos.x - rect_.position.x) / rect_.size.x, 0.0f, 1.0f);
        setValue(minValue_ + t * (maxValue_ - minValue_));
    }

    float minValue_ = 0.0f;
    float maxValue_ = 1.0f;
    float value_ = 0.0f;
    std::function<void(float)> onValueChanged_;
};

class UIWindow : public UIElementBase {
public:
    UIWindow(const std::string& title, const Vec2& pos, const Vec2& size) : title_(title) {
        rect_.position = pos;
        rect_.size = size;
    }

    void addElement(std::shared_ptr<UIElementBase> element) {
        if (element) elements_.push_back(std::move(element));
    }

    void append_draw_commands(std::vector<UIDrawCommand>& out) const override {
        if (!rect_.visible) return;
        UIDrawCommand panel;
        panel.kind = UIElementKind::Window;
        panel.rect = rect_;
        panel.color = style_.backgroundColor;
        panel.secondaryColor = style_.textColor;
        panel.text = title_;
        out.push_back(std::move(panel));
        for (const auto& element : elements_) {
            if (element) element->append_draw_commands(out);
        }
    }

    void update(float dt) override {
        for (auto& element : elements_) if (element) element->update(dt);
    }
    void onMouseDown(const Vec2& pos) override {
        for (auto& element : elements_) if (element) element->onMouseDown(pos);
    }
    void onMouseUp(const Vec2& pos) override {
        pressed_ = false;
        for (auto& element : elements_) if (element) element->onMouseUp(pos);
    }
    void onMouseMove(const Vec2& pos) override {
        hovered_ = rect_.contains(pos);
        for (auto& element : elements_) if (element) element->onMouseMove(pos);
    }

private:
    std::string title_;
    std::vector<std::shared_ptr<UIElementBase>> elements_;
};

class UIManager {
public:
    void addWindow(std::shared_ptr<UIWindow> window) {
        if (window) windows_.push_back(std::move(window));
    }

    void removeWindow(UIWindow* window) {
        windows_.erase(std::remove_if(windows_.begin(), windows_.end(),
            [window](const std::shared_ptr<UIWindow>& candidate) { return candidate.get() == window; }),
            windows_.end());
    }

    void render() {
        draw_commands_.clear();
        for (auto& window : windows_) {
            if (window) window->append_draw_commands(draw_commands_);
        }
    }

    const std::vector<UIDrawCommand>& draw_commands() const { return draw_commands_; }

    void update(float dt) {
        for (auto& window : windows_) if (window) window->update(dt);
    }

    void onMouseDown(const Vec2& pos) {
        for (auto it = windows_.rbegin(); it != windows_.rend(); ++it) {
            if (*it && (*it)->rect().contains(pos)) {
                (*it)->onMouseDown(pos);
                break;
            }
        }
    }

    void onMouseUp(const Vec2& pos) {
        for (auto& window : windows_) if (window) window->onMouseUp(pos);
    }

    void onMouseMove(const Vec2& pos) {
        for (auto& window : windows_) if (window) window->onMouseMove(pos);
    }

private:
    std::vector<std::shared_ptr<UIWindow>> windows_;
    std::vector<UIDrawCommand> draw_commands_;
};

} // namespace litt
