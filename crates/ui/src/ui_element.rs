//! UI element base types.

use litt_math::Vec2;

/// UI element position mode
#[derive(Clone, Copy, Debug, PartialEq)]
pub enum PosMode {
    Absolute(Vec2),
    Percent(Vec2),
}

/// UI element
#[derive(Debug)]
pub struct UiElement {
    pub name: String,
    pub pos_mode: PosMode,
    pub size: Vec2,
    pub visible: bool,
    pub children: Vec<UiElement>,
}

impl UiElement {
    pub fn new(name: &str, x: f32, y: f32, width: f32, height: f32) -> Self {
        Self {
            name: name.to_string(),
            pos_mode: PosMode::Absolute(Vec2(x, y)),
            size: Vec2(width, height),
            visible: true,
            children: Vec::new(),
        }
    }

    pub fn add_child(&mut self, child: UiElement) {
        self.children.push(child);
    }
}
