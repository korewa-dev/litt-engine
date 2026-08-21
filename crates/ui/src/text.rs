//! Text rendering — font atlas and text drawing.
//! Uses stb_truetype for font rasterization.

#[cfg(feature = "stb_truetype")]
pub use stb_text;

#[cfg(not(feature = "stb_truetype"))]
pub mod stb_text {
    use super::*;

    /// Glyph metrics
    #[derive(Debug, Clone)]
    pub struct GlyphMetrics {
        pub advance: f32,
        pub left_side_bearing: f32,
        pub width: i32,
        pub height: i32,
        pub x0: i32,
        pub y0: i32,
        pub x1: i32,
        pub y1: i32,
    }

    /// Text renderer (placeholder)
    pub struct TextRenderer;

    impl TextRenderer {
        pub fn new() -> Self { Self }

        /// Measure text width (placeholder)
        pub fn measure_text(&self, _text: &str, _font_size: f32) -> f32 {
            // Simplified: 0.6 * font_size * chars
            text.len() as f32 * _font_size * 0.6
        }
    }
}

/// Font handle
#[derive(Debug, Clone)]
pub struct Font {
    pub name: String,
    pub size: f32,
    pub data: Vec<u8>,
}

impl Font {
    pub fn new(name: &str, size: f32, data: Vec<u8>) -> Self {
        Self {
            name: name.to_string(),
            size,
            data,
        }
    }
}
