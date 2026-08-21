//! Sound asset — loaded audio data.
//! Supports WAV, Ogg Vorbis, and future formats.

#[derive(Debug)]
pub struct Sound {
    pub name: String,
    pub data: Vec<u8>,
    pub sample_rate: u32,
    pub channels: u16,
    pub bits_per_sample: u16,
    pub duration_sec: f32,
}

impl Sound {
    pub fn new(name: &str, data: Vec<u8>, sample_rate: u32, channels: u16, bits_per_sample: u16) -> Self {
        let duration_sec = (data.len() as f32) / (sample_rate as f32 * channels as f32 * (bits_per_sample as f32 / 8.0));
        Self {
            name: name.to_string(),
            data,
            sample_rate,
            channels,
            bits_per_sample,
            duration_sec,
        }
    }

    /// Load a WAV file
    pub fn load_wav(path: &str) -> Result<Self, String> {
        use std::io::Cursor;
        let data = std::fs::read(path)
            .map_err(|e| format!("Failed to read '{}': {}", path, e))?;
        let cursor = Cursor::new(data);
        let mut reader = hound::WavReader::new(cursor)
            .map_err(|e| format!("Failed to parse WAV: {}", e))?;
        let spec = reader.spec();
        let samples: Vec<f32> = reader.samples::<f32>().collect::<Result<_, _>>()
            .map_err(|e| format!("Failed to read WAV samples: {}", e))?;
        let duration_sec = samples.len() as f32 / spec.sample_rate as f32;
        let byte_data: Vec<u8> = samples.iter().flat_map(|&s| s.to_le_bytes()).collect();
        Ok(Self::new(path, byte_data, spec.sample_rate, spec.channels, spec.bits_per_sample))
    }
}
