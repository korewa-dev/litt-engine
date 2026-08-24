//! Texture loading -- PNG, JPEG, KTX2, DDS support.
//! GPU-friendly texture formats with MIP maps and compression.

use super::handle::{AssetHandle, AssetState};

/// Texture format
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum TextureFormat {
    /// 8-bit RGB
    R8Unorm,
    /// 8-bit RGBA
    R8G8B8A8Unorm,
    /// 16-bit RGBA float
    R16G16B16A16Float,
    /// 32-bit RGBA float
    R32G32B32A32Float,
    /// BC1 (DXTC) compression
    BC1Unorm,
    /// BC3 (DXTC) compression
    BC3Unorm,
    /// BC4 (DXTC) compression
    BC4Unorm,
    /// BC7 (DXTC) compression
    BC7Unorm,
    /// ASTC compression (mobile)
    ASTC_4x4,
    /// ETC2 compression (OpenGL ES)
    ETC2_RGBA,
    /// PVRTC compression (iOS)
    PVRTC_4,
    /// Unknown/raw
    Unknown,
}

impl TextureFormat {
    /// Bits per pixel
    pub fn bpp(&self) -> usize {
        match self {
            Self::R8Unorm => 8,
            Self::R8G8B8A8Unorm => 32,
            Self::R16G16B16A16Float => 64,
            Self::R32G32B32A32Float => 128,
            Self::BC1Unorm => 4,
            Self::BC3Unorm => 8,
            Self::BC4Unorm => 4,
            Self::BC7Unorm => 8,
            Self::ASTC_4x4 => 8,
            Self::ETC2_RGBA => 8,
            Self::PVRTC_4 => 4,
            Self::Unknown => 32,
        }
    }

    /// Block size (for compressed formats)
    pub fn block_size(&self) -> (u32, u32) {
        match self {
            Self::BC1Unorm | Self::BC3Unorm | Self::BC4Unorm | Self::BC7Unorm |
            Self::ASTC_4x4 | Self::ETC2_RGBA | Self::PVRTC_4 => (4, 4),
            _ => (1, 1),
        }
    }
}

/// Texture usage flags
#[derive(Clone, Copy, Debug, Default)]
pub struct TextureUsage {
    pub color_attachment: bool,
    pub depth_attachment: bool,
    pub sampling: bool,
    pub storage: bool,
    pub transfer_src: bool,
    pub transfer_dst: bool,
}

/// A loaded texture
#[derive(Debug)]
pub struct Texture {
    pub handle: AssetHandle,
    pub name: String,
    pub width: u32,
    pub height: u32,
    pub depth: u32,
    pub mip_levels: u32,
    pub array_layers: u32,
    pub format: TextureFormat,
    pub usage: TextureUsage,
    pub state: AssetState,
    /// Raw pixel data (CPU-side)
    pub data: Vec<u8>,
    /// GPU handle (backend-specific)
    pub gpu_handle: Option<u64>,
}

impl Texture {
    /// Create a new empty texture
    pub fn new(handle: AssetHandle, name: &str, width: u32, height: u32, format: TextureFormat) -> Self {
        Self {
            handle,
            name: name.to_string(),
            width,
            height,
            depth: 1,
            mip_levels: 1,
            array_layers: 1,
            format,
            usage: TextureUsage::default(),
            state: AssetState::Pending,
            data: Vec::new(),
            gpu_handle: None,
        }
    }

    /// Compute MIP levels
    pub fn compute_mip_levels(&mut self) {
        let max_dim = self.width.max(self.height).max(self.depth);
        self.mip_levels = (max_dim as f32).log2().floor() as u32 + 1;
    }

    /// Get texture size in bytes (per layer)
    pub fn size_bytes(&self) -> usize {
        let blocks_x = self.width.div_ceil(4) as u64;
        let blocks_y = self.height.div_ceil(4) as u64;
        let blocks_z = self.depth.div_ceil(4) as u64;
        let total = blocks_x * blocks_y * blocks_z
            * self.format.bpp() as u64
            * self.array_layers as u64
            * self.mip_levels as u64
            / 8;
        total as usize
    }
}

/// Image loader -- loads from bytes
pub struct ImageLoader;

impl ImageLoader {
    /// Load a texture from image bytes
    pub fn load_from_bytes(handle: AssetHandle, name: &str, data: &[u8]) -> Result<Texture, String> {
        // Try to decode as image
        let image = image::ImageReader::new(std::io::Cursor::new(data))
            .with_guessed_format()
            .map_err(|e| format!("Failed to decode image: {e}"))?
            .decode()
            .map_err(|e| format!("Failed to decode image '{name}': {e}"))?;

        let width = image.width();
        let height = image.height();
        let pixels = image.to_rgba8();
        let pixel_data = pixels.into_raw();

        // Determine format from pixel data
        let format = if pixel_data.len() == (width * height) as usize * 4 {
            TextureFormat::R8G8B8A8Unorm
        } else if pixel_data.len() == (width * height) as usize * 3 {
            TextureFormat::R8G8B8A8Unorm // Pad to RGBA
        } else {
            TextureFormat::Unknown
        };

        let mut texture = Texture::new(handle, name, width, height, format);
        texture.data = pixel_data;
        texture.state = AssetState::Loaded;
        texture.compute_mip_levels();

        Ok(texture)
    }

    /// Load a texture from file
    pub fn load_from_file(handle: AssetHandle, name: &str, path: &str) -> Result<Texture, String> {
        let data = std::fs::read(path)
            .map_err(|e| format!("Failed to read image file '{path}': {e}"))?;
        Self::load_from_bytes(handle, name, &data)
    }

    /// Load a KTX2 texture (assumes pre-compressed)
    pub fn load_ktx2(handle: AssetHandle, name: &str, data: &[u8]) -> Result<Texture, String> {
        // KTX2 is a container format for compressed textures
        // Parse KTX2 header to extract format and dimensions
        if data.len() < 40 {
            return Err("KTX2 data too small".to_string());
        }

        // KTX2 v2 magic: AB 'K' 'T' 'X' ' ' '2' '0' BB 0D 0A 1A 0A
        let magic = &data[0..12];
        if magic != b"\xABKTX 20\xBB\r\n\x1A\n" {
            return Err("Invalid KTX2 magic".to_string());
        }

        let width = u32::from_le_bytes(data[12..16].try_into().unwrap());
        let height = u32::from_le_bytes(data[16..20].try_into().unwrap());
        let layer_count = u32::from_le_bytes(data[20..24].try_into().unwrap());
        let face_count = u32::from_le_bytes(data[24..28].try_into().unwrap());
        let level_count = u32::from_le_bytes(data[28..32].try_into().unwrap());

        // Determine format from glFormat
        let gl_format = u32::from_le_bytes(data[32..36].try_into().unwrap());
        let format = match gl_format {
            0x1908 => TextureFormat::R8G8B8A8Unorm, // GL_RGBA
            0x8058 => TextureFormat::R8Unorm,       // GL_ALPHA
            0x8C4D => TextureFormat::BC1Unorm,      // GL_COMPRESSED_RGBA_S3TC_DXT1_EXT
            0x8C4F => TextureFormat::BC3Unorm,      // GL_COMPRESSED_RGBA_S3TC_DXT3_EXT
            0x8C4E => TextureFormat::BC3Unorm,      // GL_COMPRESSED_RGBA_S3TC_DXT5_EXT
            0x9390 => TextureFormat::BC7Unorm,      // GL_COMPRESSED_RGBA_BPTC_UNORM
            _ => TextureFormat::Unknown,
        };

        let mut texture = Texture::new(handle, name, width, height.max(1), format);
        texture.depth = 1;
        texture.mip_levels = level_count.max(1);
        texture.array_layers = layer_count.max(1) * face_count.max(1);
        texture.data = data.to_vec();
        texture.state = AssetState::Loaded;

        Ok(texture)
    }
}

/// Mipmap generator
pub struct MipmapGenerator;

impl MipmapGenerator {
    /// Generate mipmaps using box filter
    pub fn generate(data: &mut [u8], width: u32, height: u32, channels: usize) {
        let mut current_width = width;
        let mut current_height = height;
        let mut offset = 0;

        while current_width > 1 || current_height > 1 {
            let next_width = current_width.div_ceil(2);
            let next_height = current_height.div_ceil(2);
            let current_row_size = current_width * channels as u32;
            let next_row_size = next_width * channels as u32;

            // Box filter downsample
            for y in 0..next_height {
                for x in 0..next_width {
                    let mut r = 0.0;
                    let mut g = 0.0;
                    let mut b = 0.0;
                    let mut a = 0.0;
                    let mut count = 0u32;

                    for dy in 0..2 {
                        for dx in 0..2 {
                            let sx = x * 2 + dx;
                            let sy = y * 2 + dy;
                            if sx < current_width && sy < current_height {
                                let src_idx = (sy * current_row_size + sx * channels as u32) as usize;
                                r += data[src_idx] as f32;
                                g += data[src_idx + 1] as f32;
                                b += data[src_idx + 2] as f32;
                                if channels > 3 {
                                    a += data[src_idx + 3] as f32;
                                }
                                count += 1;
                            }
                        }
                    }

                    let dst_idx = (y * next_row_size + x * channels as u32) as usize;
                    data[offset + dst_idx] = (r / count as f32) as u8;
                    data[offset + dst_idx + 1] = (g / count as f32) as u8;
                    data[offset + dst_idx + 2] = (b / count as f32) as u8;
                    if channels > 3 {
                        data[offset + dst_idx + 3] = (a / count as f32) as u8;
                    }
                }
            }

            offset += (next_height * next_row_size) as usize;
            current_width = next_width;
            current_height = next_height;
        }
    }
}
