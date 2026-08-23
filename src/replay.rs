//! Deterministic replay recording.
//!
//! Records a fixed-timestep stream of input snapshots plus a state hash so
//! an agent (or human) can:
//! 1. Re-run a session bit-for-bit on any machine (`InputSnapshot` is POD)
//! 2. Detect desyncs immediately by comparing `state_hash` per frame
//!
//! Wire format (`LITR` v1): magic u32 | version u32 | count u64 |
//! `ReplayFrame` records (fixed stride).
//!
//! ```ignore
//! let mut rec = ReplayRecorder::new();
//! // each fixed tick:
//! rec.record(frame_index, dt_ms, input_snapshot, state_hash);
//! rec.save("session.litr")?;
//! // later / elsewhere:
//! let mut player = ReplayPlayer::load("session.litr")?;
//! while let Some(frame) = player.next_frame() { /* drive sim with frame.input */ }
//! ```

// =============================================================================
// Snapshots
// =============================================================================

/// One fixed-tick input snapshot. Pure POD — same bytes on every platform.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct InputSnapshot {
    /// Bitset of held keys (engine-defined key order)
    pub keys: u64,
    /// Mouse movement since last tick
    pub mouse_dx: i16,
    pub mouse_dy: i16,
    /// Bitset of held mouse buttons
    pub mouse_buttons: u32,
    /// Extra analog axes (triggers, sticks) normalized to [-1, 1]
    pub aux: [f32; 4],
}

impl Default for InputSnapshot {
    fn default() -> Self {
        Self { keys: 0, mouse_dx: 0, mouse_dy: 0, mouse_buttons: 0, aux: [0.0; 4] }
    }
}

impl InputSnapshot {
    const WIRE_SIZE: usize = 8 + 2 + 2 + 4 + 16; // 32

    fn encode(&self, out: &mut Vec<u8>) {
        out.extend_from_slice(&self.keys.to_le_bytes());
        out.extend_from_slice(&self.mouse_dx.to_le_bytes());
        out.extend_from_slice(&self.mouse_dy.to_le_bytes());
        out.extend_from_slice(&self.mouse_buttons.to_le_bytes());
        for v in self.aux {
            out.extend_from_slice(&v.to_bits().to_le_bytes());
        }
    }

    fn decode(buf: &[u8], pos: usize) -> Option<(Self, usize)> {
        if pos + Self::WIRE_SIZE > buf.len() {
            return None;
        }
        let mut p = pos;
        let take4 = |p: &mut usize| -> [u8; 4] {
            let a = [buf[*p], buf[*p + 1], buf[*p + 2], buf[*p + 3]];
            *p += 4;
            a
        };
        let take8 = |p: &mut usize| -> [u8; 8] {
            let mut a = [0u8; 8];
            a.copy_from_slice(&buf[*p..*p + 8]);
            *p += 8;
            a
        };
        let keys = u64::from_le_bytes(take8(&mut p));
        let mouse_dx = i16::from_le_bytes([buf[p], buf[p + 1]]);
        p += 2;
        let mouse_dy = i16::from_le_bytes([buf[p], buf[p + 1]]);
        p += 2;
        let mouse_buttons = u32::from_le_bytes(take4(&mut p));
        let mut aux = [0.0f32; 4];
        for slot in &mut aux {
            *slot = f32::from_bits(u32::from_le_bytes(take4(&mut p)));
        }
        Some((Self { keys, mouse_dx, mouse_dy, mouse_buttons, aux }, p))
    }

    pub fn key_set(&mut self, index: u8) { self.keys |= 1u64 << (index % 64); }
    pub fn key_clear(&mut self, index: u8) { self.keys &= !(1u64 << (index % 64)); }
    pub fn key_held(&self, index: u8) -> bool { self.keys & (1u64 << (index % 64)) != 0 }
}

/// One recorded tick: timing + inputs + world-state fingerprint.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct ReplayFrame {
    pub frame_index: u64,
    /// Fixed timestep duration in milliseconds (e.g. 16.667 at 60 Hz)
    pub dt_ms: f32,
    pub input: InputSnapshot,
    /// FNV-1a hash over serialized sim state this tick (desync detector)
    pub state_hash: u64,
}

impl ReplayFrame {
    const WIRE_SIZE: usize = 8 + 4 + 8 + InputSnapshot::WIRE_SIZE; // 52

    fn encode(&self, out: &mut Vec<u8>) {
        out.extend_from_slice(&self.frame_index.to_le_bytes());
        out.extend_from_slice(&self.dt_ms.to_bits().to_le_bytes());
        out.extend_from_slice(&self.state_hash.to_le_bytes());
        self.input.encode(out);
    }

    fn decode(buf: &[u8], pos: usize) -> Option<(Self, usize)> {
        if pos + Self::WIRE_SIZE > buf.len() {
            return None;
        }
        let mut p = pos;
        let frame_index = u64::from_le_bytes(buf[p..p + 8].try_into().unwrap());
        p += 8;
        let dt_ms = f32::from_bits(u32::from_le_bytes(buf[p..p + 4].try_into().unwrap()));
        p += 4;
        let state_hash = u64::from_le_bytes(buf[p..p + 8].try_into().unwrap());
        p += 8;
        let (input, np) = InputSnapshot::decode(buf, p)?;
        Some((
            Self { frame_index, dt_ms, state_hash, input },
            np,
        ))
    }
}

// =============================================================================
// State hashing (determinism fingerprint)
// =============================================================================

/// FNV-1a 64-bit over arbitrary state bytes.
///
/// Feed it your deterministic sim state (positions, velocities, RNG state)
/// in a stable byte layout each tick; identical sims produce identical hashes.
pub fn hash_state(bytes: &[u8]) -> u64 {
    let mut hash: u64 = 0xCBF29CE484222325;
    for &b in bytes {
        hash ^= b as u64;
        hash = hash.wrapping_mul(0x100000001B3);
    }
    hash
}

/// Convenience: hash an iterator of f32s in stable little-endian layout.
pub fn hash_f32s<'a>(values: impl IntoIterator<Item = &'a f32>) -> u64 {
    let mut hash: u64 = 0xCBF29CE484222325;
    for v in values {
        for b in v.to_bits().to_le_bytes() {
            hash ^= b as u64;
            hash = hash.wrapping_mul(0x100000001B3);
        }
    }
    hash
}

// =============================================================================
// Recorder
// =============================================================================

const MAGIC: u32 = 0x4C49_5452; // "LITR"
const VERSION: u32 = 1;

/// Records a replay session tick by tick.
#[derive(Default)]
pub struct ReplayRecorder {
    frames: Vec<ReplayFrame>,
    metadata: Vec<(String, String)>,
}

impl ReplayRecorder {
    pub fn new() -> Self { Self::default() }

    /// Attach key=value metadata (map name, seed, engine version…).
    pub fn set_metadata(&mut self, key: &str, value: &str) {
        self.metadata.push((key.to_string(), value.to_string()));
    }

    pub fn metadata(&self) -> &[(String, String)] {
        &self.metadata
    }

    /// Record one fixed tick.
    pub fn record(&mut self, frame_index: u64, dt_ms: f32, input: InputSnapshot, state_hash: u64) {
        self.frames.push(ReplayFrame { frame_index, dt_ms, input, state_hash });
    }

    pub fn len(&self) -> usize { self.frames.len() }

    pub fn is_empty(&self) -> bool { self.frames.is_empty() }

    /// Serialize to bytes.
    /// Layout: magic u32 | version u32 | count u64 | meta_count u32 |
    ///         metadata entries (u16 klen, u16 vlen, bytes) | frames
    pub fn to_bytes(&self) -> Vec<u8> {
        let mut out = Vec::with_capacity(
            24 + self.metadata.iter().map(|(k, v)| 4 + k.len() + v.len()).sum::<usize>()
                + self.frames.len() * ReplayFrame::WIRE_SIZE,
        );
        out.extend_from_slice(&MAGIC.to_le_bytes());
        out.extend_from_slice(&VERSION.to_le_bytes());
        out.extend_from_slice(&(self.frames.len() as u64).to_le_bytes());
        out.extend_from_slice(&(self.metadata.len() as u32).to_le_bytes());
        for (k, v) in &self.metadata {
            out.extend_from_slice(&(k.len() as u16).to_le_bytes());
            out.extend_from_slice(&(v.len() as u16).to_le_bytes());
            out.extend_from_slice(k.as_bytes());
            out.extend_from_slice(v.as_bytes());
        }
        for f in &self.frames {
            f.encode(&mut out);
        }
        out
    }

    /// Write to a `.litr` file.
    pub fn save(&self, path: &str) -> Result<(), String> {
        std::fs::write(path, self.to_bytes())
            .map_err(|e| format!("replay save '{}' failed: {}", path, e))
    }
}

// =============================================================================
// Player
// =============================================================================

/// Plays back a recorded replay, verifying determinism hashes on demand.
pub struct ReplayPlayer {
    frames: Vec<ReplayFrame>,
    cursor: usize,
    metadata: Vec<(String, String)>,
    desyncs: u32,
}

impl Default for ReplayPlayer {
    fn default() -> Self {
        Self { frames: Vec::new(), cursor: 0, metadata: Vec::new(), desyncs: 0 }
    }
}

impl ReplayPlayer {
    /// Parse from bytes.
    pub fn from_bytes(bytes: &[u8]) -> Result<Self, String> {
        if bytes.len() < 20 {
            return Err("replay too short".to_string());
        }
        let magic = u32::from_le_bytes(bytes[0..4].try_into().unwrap());
        if magic != MAGIC {
            return Err(format!("bad replay magic 0x{magic:08X}"));
        }
        let version = u32::from_le_bytes(bytes[4..8].try_into().unwrap());
        if version != VERSION {
            return Err(format!("unsupported replay version {version}"));
        }
        let count = u64::from_le_bytes(bytes[8..16].try_into().unwrap()) as usize;
        let meta_count = u32::from_le_bytes(bytes[16..20].try_into().unwrap()) as usize;

        let mut pos = 20usize;
        let mut metadata = Vec::with_capacity(meta_count.min(1024));
        for _ in 0..meta_count {
            if pos + 4 > bytes.len() {
                return Err("truncated replay metadata".to_string());
            }
            let klen = u16::from_le_bytes([bytes[pos], bytes[pos + 1]]) as usize;
            let vlen = u16::from_le_bytes([bytes[pos + 2], bytes[pos + 3]]) as usize;
            pos += 4;
            if pos + klen + vlen > bytes.len() {
                return Err("truncated replay metadata entry".to_string());
            }
            let key = String::from_utf8_lossy(&bytes[pos..pos + klen]).into_owned();
            pos += klen;
            let value = String::from_utf8_lossy(&bytes[pos..pos + vlen]).into_owned();
            pos += vlen;
            metadata.push((key, value));
        }

        let mut frames = Vec::with_capacity(count);
        for _ in 0..count {
            match ReplayFrame::decode(bytes, pos) {
                Some((f, next)) => {
                    frames.push(f);
                    pos = next;
                }
                None => break,
            }
        }

        Ok(Self { frames, cursor: 0, metadata, desyncs: 0 })
    }

    /// Load from a `.litr` file.
    pub fn load(path: &str) -> Result<Self, String> {
        let bytes = std::fs::read(path)
            .map_err(|e| format!("replay load '{}' failed: {}", path, e))?;
        Self::from_bytes(&bytes)
    }

    /// Next recorded tick, or None at end. Auto-advances the cursor.
    pub fn next_frame(&mut self) -> Option<ReplayFrame> {
        let f = self.frames.get(self.cursor).copied();
        if f.is_some() {
            self.cursor += 1;
        }
        f
    }

    /// Peek without advancing.
    pub fn peek(&self) -> Option<&ReplayFrame> {
        self.frames.get(self.cursor)
    }

    /// Compare the sim's current state hash against the recorded one.
    /// Returns false (and counts a desync) when they diverge.
    pub fn verify_state(&mut self, recorded: &ReplayFrame, actual_hash: u64) -> bool {
        if recorded.state_hash == actual_hash || recorded.state_hash == 0 {
            true
        } else {
            self.desyncs += 1;
            false
        }
    }

    pub fn desync_count(&self) -> u32 { self.desyncs }
    pub fn remaining(&self) -> usize { self.frames.len() - self.cursor.min(self.frames.len()) }
    pub fn total_frames(&self) -> usize { self.frames.len() }
    pub fn metadata(&self) -> &[(String, String)] { &self.metadata }
    pub fn reset(&mut self) { self.cursor = 0; }
}

// =============================================================================
// Tests
// =============================================================================

#[cfg(test)]
mod tests {
    use super::*;

    fn sample_input(tick: u64) -> InputSnapshot {
        let mut input = InputSnapshot::default();
        if tick % 2 == 0 { input.key_set(3); }
        input.mouse_dx = (tick as i16 % 7) - 3;
        input.mouse_dy = ((tick * 5) as i16 % 11) - 5;
        input.aux = [0.25, -0.5, 1.0, 0.0];
        input
    }

    #[test]
    fn recorder_player_roundtrip() {
        let mut rec = ReplayRecorder::new();
        rec.set_metadata("seed", "1337");
        for t in 0..120u64 {
            let hash = hash_f32s(&[(t as f32) * 0.5, 1.0]);
            rec.record(t, 1000.0 / 60.0, sample_input(t), hash);
        }
        assert_eq!(rec.len(), 120);

        let bytes = rec.to_bytes();
        let mut player = ReplayPlayer::from_bytes(&bytes).unwrap();
        assert_eq!(player.total_frames(), 120);

        for t in 0..120u64 {
            let f = player.next_frame().unwrap();
            assert_eq!(f.frame_index, t);
            assert!((f.dt_ms - 1000.0 / 60.0).abs() < 1e-6);
            assert_eq!(f.input, sample_input(t));
            assert!(player.verify_state(&f, hash_f32s(&[(t as f32) * 0.5, 1.0])));
        }
        assert!(player.next_frame().is_none());
        assert_eq!(player.desync_count(), 0);
    }

    #[test]
    fn file_save_load_roundtrip() {
        let dir = std::env::temp_dir().join("litt_replay_test");
        std::fs::create_dir_all(&dir).unwrap();
        let path = dir.join("s.litr");
        let path = path.to_str().unwrap();

        let mut rec = ReplayRecorder::new();
        rec.record(0, 16.667, sample_input(0), 42);
        rec.save(path).unwrap();

        let mut player = ReplayPlayer::load(path).unwrap();
        let f = player.next_frame().unwrap();
        assert_eq!(f.state_hash, 42);
        assert!(f.input.key_held(3));
        let _ = std::fs::remove_file(path);
    }

    #[test]
    fn detects_desync() {
        let mut rec = ReplayRecorder::new();
        rec.record(0, 16.667, InputSnapshot::default(), 111);
        rec.record(1, 16.667, InputSnapshot::default(), 222);

        let mut player = ReplayPlayer::from_bytes(&rec.to_bytes()).unwrap();
        let f0 = player.next_frame().unwrap();
        assert!(player.verify_state(&f0, 111));
        let f1 = player.next_frame().unwrap();
        assert!(!player.verify_state(&f1, 999)); // diverged!
        assert_eq!(player.desync_count(), 1);
    }

    #[test]
    fn metadata_roundtrip() {
        let mut rec = ReplayRecorder::new();
        rec.set_metadata("map", "arena_01");
        rec.set_metadata("seed", "1337");
        rec.record(0, 16.667, InputSnapshot::default(), 1);

        let player = ReplayPlayer::from_bytes(&rec.to_bytes()).unwrap();
        assert_eq!(player.metadata(), &[("map".to_string(), "arena_01".to_string()), ("seed".to_string(), "1337".to_string())]);
    }

    #[test]
    fn rejects_bad_magic() {
        let mut bytes = vec![0u8; 32];
        bytes[0] = 0xAB;
        assert!(ReplayPlayer::from_bytes(&bytes).is_err());
    }

    #[test]
    fn hash_is_stable() {
        let a = hash_state(b"litt engine");
        let b = hash_state(b"litt engine");
        let c = hash_state(b"litt enginE");
        assert_eq!(a, b);
        assert_ne!(a, c);
    }
}
