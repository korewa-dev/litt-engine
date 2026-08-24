//! Font loading -- TTF, OTF support.
//! Uses stb_truetype for rasterization.

use super::handle::AssetHandle;

/// A loaded font
#[derive(Debug)]
pub struct Font {
    pub handle: AssetHandle,
    pub name: String,
    pub data: Vec<u8>,
    pub scale: f32,
    pub ascent: f32,
    pub descent: f32,
    pub line_gap: f32,
    pub glyph_count: u32,
}

impl Font {
    /// Load a font from file
    pub fn load_from_file(path: &str, scale: f32) -> Result<Self, String> {
        let data = std::fs::read(path)
            .map_err(|e| format!("Failed to read font file '{path}': {e}"))?;

        // Parse basic font metrics from TTF/OTF
        // This is a simplified parser -- real implementation would use a proper font library
        let mut font = Self {
            handle: AssetHandle::from_path(path, super::handle::AssetType::Font),
            name: path.to_string(),
            data,
            scale,
            ascent: 0.0,
            descent: 0.0,
            line_gap: 0.0,
            glyph_count: 0,
        };

        font.parse_metrics();
        Ok(font)
    }

    /// Parse font metrics from TTF header
    fn parse_metrics(&mut self) {
        if self.data.len() < 34 {
            return;
        }

        // Check for TrueType or OpenType
        let is_tt = &self.data[0..4] == b"\x00\x01\x00\x00"
            || &self.data[0..4] == b"true"
            || &self.data[0..4] == b"typ1";

        if !is_tt {
            return;
        }

        // Parse 'hhea' table for metrics
        // Offset to 'hhea' table
        let num_tables = u16::from_be_bytes([self.data[4], self.data[5]]) as usize;
        let search_range = u16::from_be_bytes([self.data[6], self.data[7]]) as usize;
        let entry_selector = u16::from_be_bytes([self.data[8], self.data[9]]) as usize;
        let range_shift = u16::from_be_bytes([self.data[10], self.data[11]]) as usize;

        // Find 'hhea' table
        let mut hhea_offset = 0u32;
        for i in 0..num_tables.min(100) {
            let table_offset = 12 + i * 16;
            if table_offset + 16 > self.data.len() {
                break;
            }

            let tag = &self.data[table_offset..table_offset + 4];
            if tag == b"hhea" {
                hhea_offset = u32::from_be_bytes([
                    self.data[table_offset + 4],
                    self.data[table_offset + 5],
                    self.data[table_offset + 6],
                    self.data[table_offset + 7],
                ]);
                break;
            }
        }

        if hhea_offset > 0 && hhea_offset as usize + 36 <= self.data.len() {
            self.ascent = i16::from_be_bytes([self.data[hhea_offset as usize], self.data[hhea_offset as usize + 1]]) as f32;
            self.descent = i16::from_be_bytes([self.data[hhea_offset as usize + 2], self.data[hhea_offset as usize + 3]]) as f32;
            self.line_gap = i16::from_be_bytes([self.data[hhea_offset as usize + 4], self.data[hhea_offset as usize + 5]]) as f32;
        }

        // Scale metrics by font scale
        let scale_factor = self.scale / 100.0;
        self.ascent *= scale_factor;
        self.descent *= scale_factor;
        self.line_gap *= scale_factor;
    }

    /// Get the font height in pixels
    pub fn height(&self) -> f32 {
        (self.ascent - self.descent + self.line_gap).abs()
    }
}
