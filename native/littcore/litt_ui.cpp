// LittUI - Working UI Implementation
// Basic UI framework with rendering support

#include "litt_ui.h"
#include <algorithm>
#include <cstdio>

namespace litt {

// =============================================================================
// UIManager Implementation
// =============================================================================

UIManager::UIManager() : focused_element_(nullptr),
                         mouse_pos_(0, 0),
                         hover_element_(nullptr),
                         drag_element_(nullptr) {}

void UIManager::add_element(std::shared_ptr<UIElementBase> element) {
    elements_.push_back(element);
}

void UIManager::remove_element(UIElementBase* element) {
    elements_.erase(
        std::remove_if(elements_.begin(), elements_.end(),
            [element](const std::shared_ptr<UIElementBase>& e) {
                return e.get() == element;
            }),
        elements_.end()
    );
}

void UIManager::clear() {
    elements_.clear();
    focused_element_ = nullptr;
    hover_element_ = nullptr;
    drag_element_ = nullptr;
}

void UIManager::update(float dt) {
    // Update all elements
    for (auto& element : elements_) {
        element->update(dt);
    }
}

void UIManager::render(Renderer* renderer) {
    if (!renderer) return;
    
    // Render all visible elements
    for (auto& element : elements_) {
        if (element->rect().visible) {
            element->render();
        }
    }
}

// =============================================================================
// Input Handling
// =============================================================================

void UIManager::handle_mouse_move(double x, double y) {
    mouse_pos_ = Vec2f(static_cast<float>(x), static_cast<float>(y));
    
    // Update hover state
    hover_element_ = nullptr;
    
    for (auto it = elements_.rbegin(); it != elements_.rend(); ++it) {
        if ((*it)->rect().visible && (*it)->rect().interactable &&
            (*it)->rect().contains(mouse_pos_)) {
            hover_element_ = it->get();
            break;
        }
    }
    
    // Notify elements
    for (auto& element : elements_) {
        element->on_mouse_move(mouse_pos_);
    }
}

void UIManager::handle_mouse_down(MouseButton button, double x, double y) {
    Vec2f pos(static_cast<float>(x), static_cast<float>(y));
    
    // Find clicked element
    for (auto it = elements_.rbegin(); it != elements_.rend(); ++it) {
        if ((*it)->rect().visible && (*it)->rect().interactable &&
            (*it)->rect().contains(pos)) {
            (*it)->on_mouse_down(pos);
            
            if ((*it)->rect().interactable) {
                focused_element_ = it->get();
                drag_element_ = it->get();
            }
            break;
        }
    }
}

void UIManager::handle_mouse_up(MouseButton button, double x, double y) {
    Vec2f pos(static_cast<float>(x), static_cast<float>(y));
    
    if (drag_element_) {
        drag_element_->on_mouse_up(pos);
        
        // Check if still hovered for click
        if (drag_element_->rect().contains(pos)) {
            drag_element_->on_click(pos);
        }
        
        drag_element_ = nullptr;
    }
    
    focused_element_ = nullptr;
}

void UIManager::handle_key_press(char key) {
    if (focused_element_) {
        focused_element_->on_key_press(key);
    }
}

void UIManager::handle_scroll(double delta) {
    for (auto& element : elements_) {
        element->on_scroll(delta);
    }
}

// =============================================================================
// UI Element Implementations
// =============================================================================

// Button
Button::Button(const std::string& text, const Vec2f& position, const Vec2f& size)
    : UIElementBase(UIElement::Button, position, size) {
    text_ = text;
    style_.fontSize = 14.0f;
}

void Button::render() {
    // Draw button background
    renderer_->fill_rect(rect_.position, rect_.size, 
        is_hovered() ? style_.hoverColor : style_.backgroundColor);
    
    // Draw border
    renderer_->draw_rect(rect_.position, rect_.size, 
        style_.borderColor, style_.borderWidth);
    
    // Draw text
    renderer_->draw_text(text_, 
        Vec2f(rect_.position.x + 10, rect_.position.y + rect_.size.y / 2),
        style_.textColor, style_.fontSize);
}

void Button::on_click(const Vec2f& pos) {
    if (on_click_callback_) {
        on_click_callback_(pos);
    }
}

// Text
Text::Text(const std::string& text, const Vec2f& position, const Vec2f& size)
    : UIElementBase(UIElement::Text, position, size) {
    text_ = text;
    style_.fontSize = 14.0f;
}

void Text::render() {
    if (!rect_.visible) return;
    
    renderer_->draw_text(text_, rect_.position, style_.textColor, style_.fontSize);
}

// Panel
Panel::Panel(const std::string& name, const Vec2f& position, const Vec2f& size)
    : UIElementBase(UIElement::Panel, position, size) {
    name_ = name;
    style_.backgroundColor = Vec3f(0.1f, 0.1f, 0.1f);
}

void Panel::render() {
    if (!rect_.visible) return;
    
    // Draw background
    renderer_->fill_rect(rect_.position, rect_.size, style_.backgroundColor);
    
    // Draw border
    renderer_->draw_rect(rect_.position, rect_.size, 
        style_.borderColor, style_.borderWidth);
    
    // Render children
    for (auto& child : children_) {
        child->render();
    }
}

void Panel::add_child(std::shared_ptr<UIElementBase> child) {
    children_.push_back(child);
}

// Slider
Slider::Slider(float min, float max, float value, const Vec2f& position, const Vec2f& size)
    : UIElementBase(UIElement::Slider, position, size),
      min_(min), max_(max), value_(value) {
    style_.backgroundColor = Vec3f(0.3f, 0.3f, 0.3f);
    style_.activeColor = Vec3f(0.5f, 0.5f, 0.5f);
}

void Slider::render() {
    // Draw track
    renderer_->fill_rect(rect_.position, rect_.size, style_.backgroundColor);
    
    // Draw fill
    float fill_width = ((value_ - min_) / (max_ - min_)) * rect_.size.x;
    Vec2f fill_pos = rect_.position;
    fill_pos.x += fill_width;
    fill_pos.x -= rect_.size.x; // Adjust for left-aligned fill
    renderer_->fill_rect(fill_pos, Vec2f(fill_width, rect_.size.y), style_.activeColor);
    
    // Draw handle
    Vec2f handle_pos = rect_.position;
    handle_pos.x += fill_width - 10;
    renderer_->fill_rect(handle_pos, Vec2f(20, rect_.size.y), style_.textColor);
}

void Slider::on_mouse_down(const Vec2f& pos) {
    is_dragging_ = true;
    update_value(pos);
}

void Slider::on_mouse_up(const Vec2f& pos) {
    is_dragging_ = false;
}

void Slider::on_mouse_move(const Vec2f& pos) {
    if (is_dragging_) {
        update_value(pos);
    }
}

void Slider::update_value(const Vec2f& pos) {
    float ratio = (pos.x - rect_.position.x) / rect_.size.x;
    ratio = std::clamp(ratio, 0.0f, 1.0f);
    value_ = min_ + ratio * (max_ - min_);
    
    if (on_value_changed_callback_) {
        on_value_changed_callback_(value_);
    }
}

} // namespace litt
