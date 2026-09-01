// Phase 6: Advanced Features - UI System

#pragma once

#include "litt_math.h"
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace litt {

// UI element types
enum class UIElementType {
    PANEL,
    BUTTON,
    LABEL,
    TEXT_INPUT,
    SLIDER,
    CHECKBOX,
    IMAGE,
    SCROLL_VIEW
};

// UI element base class
class UIElement {
public:
    UIElement(UIElementType type) : type_(type) {}
    virtual ~UIElement() = default;
    
    // Get type
    UIElementType get_type() const { return type_; }
    
    // Set position
    void set_position(const Vec2& pos) { position_ = pos; }
    const Vec2& get_position() const { return position_; }
    
    // Set size
    void set_size(const Vec2& size) { size_ = size; }
    const Vec2& get_size() const { return size_; }
    
    // Set visible
    void set_visible(bool visible) { visible_ = visible; }
    bool is_visible() const { return visible_; }
    
    // Set enabled
    void set_enabled(bool enabled) { enabled_ = enabled; }
    bool is_enabled() const { return enabled_; }
    
    // Add child
    void add_child(std::unique_ptr<UIElement> child);
    
    // Get children
    const std::vector<std::unique_ptr<UIElement>>& get_children() const { return children_; }
    
    // Render
    virtual void render();
    
    // Update
    virtual void update(float delta_time);

protected:
    UIElementType type_;
    Vec2 position_;
    Vec2 size_;
    bool visible_ = true;
    bool enabled_ = true;
    std::vector<std::unique_ptr<UIElement>> children_;
};

// UI panel
class UIPanel : public UIElement {
public:
    UIPanel() : UIElement(UIElementType::PANEL) {}
    
    // Set background color
    void set_background_color(const Vec4& color) { background_color_ = color; }
    
    // Set border width
    void set_border_width(float width) { border_width_ = width; }
    
    // Set border color
    void set_border_color(const Vec4& color) { border_color_ = color; }
    
    void render() override;

private:
    Vec4 background_color_ = Vec4(0.2f, 0.2f, 0.2f, 1.0f);
    Vec4 border_color_ = Vec4(0.5f, 0.5f, 0.5f, 1.0f);
    float border_width_ = 1.0f;
};

// UI button
class UIButton : public UIElement {
public:
    UIButton() : UIElement(UIElementType::BUTTON) {}
    
    // Set text
    void set_text(const std::string& text) { text_ = text; }
    const std::string& get_text() const { return text_; }
    
    // Set on click callback
    void set_on_click(std::function<void()> callback) { on_click_ = callback; }
    
    // Click
    void click();
    
    void render() override;

private:
    std::string text_;
    std::function<void()> on_click_;
};

// UI label
class UILabel : public UIElement {
public:
    UILabel() : UIElement(UIElementType::LABEL) {}
    
    // Set text
    void set_text(const std::string& text) { text_ = text; }
    const std::string& get_text() const { return text_; }
    
    // Set text color
    void set_text_color(const Vec4& color) { text_color_ = color; }
    
    // Set font size
    void set_font_size(uint32_t size) { font_size_ = size; }
    
    void render() override;

private:
    std::string text_;
    Vec4 text_color_ = Vec4(1.0f);
    uint32_t font_size_ = 16;
};

// UI text input
class UITextInput : public UIElement {
public:
    UITextInput() : UIElement(UIElementType::TEXT_INPUT) {}
    
    // Set text
    void set_text(const std::string& text) { text_ = text; }
    const std::string& get_text() const { return text_; }
    
    // Set placeholder
    void set_placeholder(const std::string& placeholder) { placeholder_ = placeholder; }
    
    // Set on text changed callback
    void set_on_text_changed(std::function<void(const std::string&)> callback) { on_text_changed_ = callback; }
    
    // Append text
    void append_text(const std::string& text);
    
    // Backspace
    void backspace();
    
    void render() override;

private:
    std::string text_;
    std::string placeholder_;
    std::function<void(const std::string&)> on_text_changed_;
};

// UI slider
class UISlider : public UIElement {
public:
    UISlider() : UIElement(UIElementType::SLIDER) {}
    
    // Set value
    void set_value(float value) { value_ = value; }
    float get_value() const { return value_; }
    
    // Set min/max
    void set_min(float min) { min_ = min; }
    void set_max(float max) { max_ = max; }
    float get_min() const { return min_; }
    float get_max() const { return max_; }
    
    // Set on value changed callback
    void set_on_value_changed(std::function<void(float)> callback) { on_value_changed_ = callback; }
    
    void render() override;

private:
    float value_ = 0.5f;
    float min_ = 0.0f;
    float max_ = 1.0f;
    std::function<void(float)> on_value_changed_;
};

// UI manager
class UIManager {
public:
    static UIManager& get_instance() {
        static UIManager instance;
        return instance;
    }
    
    // Initialize UI
    bool initialize();
    
    // Shutdown UI
    void shutdown();
    
    // Create element
    template<typename T, typename... Args>
    T* create_element(Args&&... args) {
        auto element = std::make_unique<T>(std::forward<Args>(args)...);
        T* ptr = element.get();
        root_elements_.push_back(std::move(element));
        return ptr;
    }
    
    // Get root elements
    const std::vector<std::unique_ptr<UIElement>>& get_root_elements() const { return root_elements_; }
    
    // Render all
    void render();
    
    // Update all
    void update(float delta_time);
    
    // Handle mouse input
    void handle_mouse_move(const Vec2& pos);
    void handle_mouse_click(const Vec2& pos);
    void handle_mouse_release(const Vec2& pos);
    
    // Handle keyboard input
    void handle_key_down(uint32_t key);
    void handle_key_up(uint32_t key);
    void handle_text_input(const std::string& text);

private:
    UIManager() = default;
    std::vector<std::unique_ptr<UIElement>> root_elements_;
    Vec2 mouse_pos_;
    UIElement* focused_element_ = nullptr;
};

} // namespace litt
