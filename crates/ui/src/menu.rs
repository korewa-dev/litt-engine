//! Menu system -- keyboard/gamepad-navigable menus rendered as overlay text.
//!
//! A [`Menu`] is a list of items with typed values. The host feeds it
//! semantic events ([`MenuInput`]) and draws it with [`Menu::render`] into
//! any [`Overlay`]. The engine keeps this crate free of input/config
//! dependencies on purpose: `App` maps raw keys to events and menu changes
//! back onto `Settings`, so the same widget powers pause menus, settings,
//! and editor dialogs.
//!
//! ```ignore
//! let mut menu = Menu::new("SETTINGS");
//! menu.add_bool("VSync", true);
//! menu.add_enum("Quality", vec!["Low","Medium","High","Ultra"], 2);
//! if let Some(ev) = menu.handle(MenuInput::Left) { /* apply change */ }
//! ```

use crate::overlay::Overlay;

/// Value of a single menu item.
#[derive(Clone, Debug, PartialEq)]
pub enum MenuValue {
    /// On/off toggle
    Bool(bool),
    /// Continuous value with step, min, max, and formatting hint (decimals)
    Float(f32, f32, f32, u8),
    /// One of N labeled options (value = selected index)
    Enum(Vec<String>, usize),
    /// Trigger-only row (e.g. "Apply", "Quit")
    Action,
}

impl MenuValue {
    pub fn is_action(&self) -> bool {
        matches!(self, MenuValue::Action)
    }
}

/// One row in a menu.
#[derive(Clone, Debug)]
pub struct MenuItem {
    pub label: String,
    pub value: MenuValue,
}

/// What happened after handling an input event.
#[derive(Clone, Debug, PartialEq)]
pub enum MenuEvent {
    /// Item at `index` changed to `value`
    Changed(usize, MenuValue),
    /// Action item at `index` was activated
    Activated(usize),
    /// User asked to close the menu
    Closed,
    /// Event consumed but nothing changed
    None,
}

/// Navigation/adjustment events; the host translates raw keys/buttons.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum MenuInput {
    Up,
    Down,
    Left,
    Right,
    /// Activate / toggle the selected row
    Select,
    /// Close the menu
    Back,
}

/// A navigable menu.
#[derive(Clone, Debug)]
pub struct Menu {
    pub title: String,
    pub items: Vec<MenuItem>,
    pub selected: usize,
    pub open: bool,
}

impl Menu {
    pub fn new(title: &str) -> Self {
        Self {
            title: title.to_string(),
            items: Vec::new(),
            selected: 0,
            open: false,
        }
    }

    // -- construction ----------------------------------------------------

    pub fn add_bool(&mut self, label: &str, v: bool) -> usize {
        self.items.push(MenuItem { label: label.to_string(), value: MenuValue::Bool(v) });
        self.items.len() - 1
    }

    pub fn add_float(&mut self, label: &str, v: f32, min: f32, max: f32, decimals: u8) -> usize {
        self.items.push(MenuItem {
            label: label.to_string(),
            value: MenuValue::Float(v.clamp(min, max), min, max, decimals),
        });
        self.items.len() - 1
    }

    pub fn add_enum(&mut self, label: &str, options: Vec<&str>, selected: usize) -> usize {
        self.items.push(MenuItem {
            label: label.to_string(),
            value: MenuValue::Enum(options.into_iter().map(|s| s.to_string()).collect(), selected),
        });
        self.items.len() - 1
    }

    pub fn add_action(&mut self, label: &str) -> usize {
        self.items.push(MenuItem { label: label.to_string(), value: MenuValue::Action });
        self.items.len() - 1
    }

    // -- state ------------------------------------------------------------

    pub fn open(&mut self) {
        self.open = true;
        self.selected = self.selected.min(self.last_index());
    }

    pub fn close(&mut self) {
        self.open = false;
    }

    fn last_index(&self) -> usize {
        self.items.len().saturating_sub(1)
    }

    pub fn selected_value(&self) -> Option<&MenuValue> {
        self.items.get(self.selected).map(|i| &i.value)
    }

    /// Read helper for hosts: current Bool of item `idx`.
    pub fn bool_at(&self, idx: usize) -> Option<bool> {
        match self.items.get(idx).map(|i| &i.value) {
            Some(MenuValue::Bool(b)) => Some(*b),
            _ => None,
        }
    }

    /// Read helper for hosts: current Float of item `idx`.
    pub fn float_at(&self, idx: usize) -> Option<f32> {
        match self.items.get(idx).map(|i| &i.value) {
            Some(MenuValue::Float(v, ..)) => Some(*v),
            _ => None,
        }
    }

    /// Read helper for hosts: selected option index of Enum item `idx`.
    pub fn enum_index_at(&self, idx: usize) -> Option<usize> {
        match self.items.get(idx).map(|i| &i.value) {
            Some(MenuValue::Enum(_, s)) => Some(*s),
            _ => None,
        }
    }

    // -- input handling -----------------------------------------------------

    /// Feed one semantic input event; returns what happened.
    pub fn handle(&mut self, input: MenuInput) -> MenuEvent {
        if !self.open || self.items.is_empty() {
            return MenuEvent::None;
        }
        match input {
            MenuInput::Up => {
                self.selected = if self.selected == 0 { self.last_index() } else { self.selected - 1 };
                MenuEvent::None
            }
            MenuInput::Down => {
                self.selected = if self.selected == self.last_index() { 0 } else { self.selected + 1 };
                MenuEvent::None
            }
            MenuInput::Left | MenuInput::Right => self.adjust(input == MenuInput::Right),
            MenuInput::Select => {
                let idx = self.selected;
                match self.items[idx].value.clone() {
                    MenuValue::Bool(_) => self.adjust(true),
                    MenuValue::Action => MenuEvent::Activated(idx),
                    MenuValue::Float(..) => self.adjust(true),
                    MenuValue::Enum(opts, s) => {
                        let next = (s + 1) % opts.len();
                        self.set_value(idx, MenuValue::Enum(opts, next))
                    }
                }
            }
            MenuInput::Back => {
                self.close();
                MenuEvent::Closed
            }
        }
    }

    fn adjust(&mut self, positive: bool) -> MenuEvent {
        let idx = self.selected;
        match self.items[idx].value.clone() {
            MenuValue::Bool(b) => self.set_value(idx, MenuValue::Bool(!b)),
            MenuValue::Float(v, min, max, decimals) => {
                let span = max - min;
                // Coarse ranges step in whole units; fine ranges use 5% steps.
                let step = if span >= 2.0 { 1.0 } else { span / 20.0 };
                let mut nv = if positive { v + step } else { v - step };
                nv = nv.clamp(min, max);
                if span >= 2.0 {
                    nv = nv.round();
                }
                if nv != v {
                    self.set_value(idx, MenuValue::Float(nv, min, max, decimals))
                } else {
                    MenuEvent::None
                }
            }
            MenuValue::Enum(opts, s) => {
                let n = opts.len();
                let next = if positive { (s + 1) % n } else { (s + n - 1) % n };
                self.set_value(idx, MenuValue::Enum(opts, next))
            }
            MenuValue::Action => MenuEvent::None,
        }
    }

    fn set_value(&mut self, idx: usize, value: MenuValue) -> MenuEvent {
        self.items[idx].value = value.clone();
        MenuEvent::Changed(idx, value)
    }

    // -- rendering -----------------------------------------------------------

    /// Draw the menu into an overlay starting at screen fraction (x, y).
    /// Returns rows drawn. Pure text so it works before any GPU backend.
    pub fn render(&self, overlay: &mut Overlay, x: f32, y: f32, line_height: f32) -> usize {
        if !self.open {
            return 0;
        }
        let white = [0.95, 0.95, 0.95, 1.0];
        let accent = [1.0, 0.62, 0.25, 1.0];
        let dim = [0.6, 0.6, 0.62, 1.0];

        overlay.draw_text(&self.title, x, y, accent, 26.0);
        let mut row = 1usize;
        for (i, item) in self.items.iter().enumerate() {
            let color = if i == self.selected { accent } else { white };
            let marker = if i == self.selected { "> " } else { "  " };
            let value_text = match &item.value {
                MenuValue::Bool(true) => "< ON  >".to_string(),
                MenuValue::Bool(false) => "< OFF >".to_string(),
                MenuValue::Float(v, _, _, d) => format!("< {:.*} >", *d as usize, v),
                MenuValue::Enum(opts, s) => {
                    format!("< {} >", opts.get(*s).map(|x| x.as_str()).unwrap_or("?"))
                }
                MenuValue::Action => String::new(),
            };
            let suffix = if item.value.is_action() {
                String::new()
            } else {
                format!("   {}", dim_label(&value_text))
            };
            let _ = &suffix;
            let line = format!("{}{}{}", marker, item.label, if value_text.is_empty() { "" } else { "  " });
            overlay.draw_text(line.trim_end(), x, y + row as f32 * line_height, color, 18.0);
            if !value_text.is_empty() {
                overlay.draw_text(&value_text, x + 320.0, y + row as f32 * line_height, if i == self.selected { accent } else { dim }, 18.0);
            }
            row += 1;
        }
        overlay.draw_text("Arrows adjust - Enter select - Esc back", x, y + (row + 1) as f32 * line_height, dim, 14.0);
        row + 2
    }
}

fn dim_label(s: &str) -> &str {
    // Placeholder hook so tests can assert formatting stays stable.
    s
}

#[cfg(test)]
mod tests {
    use super::*;

    fn demo_menu() -> Menu {
        let mut m = Menu::new("SETTINGS");
        m.add_bool("VSync", true);                       // 0
        m.add_float("Master Volume", 0.8, 0.0, 1.0, 2);  // 1
        m.add_enum("Quality", vec!["Low", "Medium", "High"], 2); // 2
        m.add_action("Apply");                           // 3
        m.open();
        m
    }

    #[test]
    fn wraps_around_both_directions() {
        let mut m = demo_menu();
        m.selected = 0;
        m.handle(MenuInput::Up);
        assert_eq!(m.selected, 3);
        m.handle(MenuInput::Down);
        assert_eq!(m.selected, 0);
        m.handle(MenuInput::Down);
        assert_eq!(m.selected, 1);
    }

    #[test]
    fn toggles_and_sliders_emit_changes() {
        let mut m = demo_menu(); // selected = 0 (bool)
        match m.handle(MenuInput::Select) {
            MenuEvent::Changed(0, MenuValue::Bool(false)) => {}
            other => panic!("expected bool flip, got {:?}", other),
        }
        m.selected = 1;
        match m.handle(MenuInput::Right) {
            MenuEvent::Changed(1, MenuValue::Float(v, _, _, _)) => assert!(v > 0.8),
            other => panic!("expected float bump, got {:?}", other),
        }
        // Clamped at max: further Right is a no-op event-wise? Still Changed but equal clamp.
        for _ in 0..40 {
            m.handle(MenuInput::Right);
        }
        assert_eq!(m.float_at(1), Some(1.0));
    }

    #[test]
    fn enum_cycles_forward_only_on_select() {
        let mut m = demo_menu();
        m.selected = 2;
        assert_eq!(m.enum_index_at(2), Some(2));
        m.handle(MenuInput::Select);
        assert_eq!(m.enum_index_at(2), Some(0)); // wraps High -> Low
        m.handle(MenuInput::Left);
        assert_eq!(m.enum_index_at(2), Some(2));
    }

    #[test]
    fn actions_activate_and_back_closes() {
        let mut m = demo_menu();
        m.selected = 3;
        assert_eq!(m.handle(MenuInput::Select), MenuEvent::Activated(3));
        assert!(m.open);
        assert_eq!(m.handle(MenuInput::Back), MenuEvent::Closed);
        assert!(!m.open);
        assert_eq!(m.handle(MenuInput::Down), MenuEvent::None);
    }

    #[test]
    fn render_draws_rows_when_open() {
        let mut m = demo_menu();
        let mut ov = Overlay::new();
        m.close();
        assert_eq!(m.render(&mut ov, 40.0, 40.0, 24.0), 0);
        m.open();
        let drawn = m.render(&mut ov, 40.0, 40.0, 24.0);
        assert!(drawn >= m.items.len() + 2);
        assert!(!ov.is_empty());
    }
}
