//! Asset cache -- LRU cache for loaded assets.
//! Prevents duplicate loading and manages memory.

use std::collections::HashMap;
use super::handle::AssetHandle;

/// Cache entry with usage timestamp
#[derive(Debug)]
struct CacheEntry {
    pub last_access: u64,
    pub use_count: u32,
    pub size_bytes: usize,
}

/// Asset cache -- LRU eviction policy
#[derive(Debug)]
pub struct AssetCache {
    entries: HashMap<AssetHandle, CacheEntry>,
    max_size_bytes: usize,
    current_size_bytes: usize,
}

impl Default for AssetCache {
    fn default() -> Self {
        Self::new()
    }
}

impl AssetCache {
    /// Create a new asset cache
    pub fn new() -> Self {
        Self {
            entries: HashMap::new(),
            max_size_bytes: 512 * 1024 * 1024, // 512 MB default
            current_size_bytes: 0,
        }
    }

    /// Set maximum cache size
    pub fn with_max_size(mut self, max_size_bytes: usize) -> Self {
        self.max_size_bytes = max_size_bytes;
        self
    }

    /// Record an asset as loaded
    pub fn record(&mut self, handle: AssetHandle, size_bytes: usize) {
        self.entries.insert(handle, CacheEntry {
            last_access: self.tick(),
            use_count: 1,
            size_bytes,
        });
        self.current_size_bytes += size_bytes;
        self.evict();
    }

    /// Access an asset (updates LRU timestamp)
    pub fn access(&mut self, handle: &AssetHandle) {
        let t = self.tick();
        if let Some(entry) = self.entries.get_mut(handle) {
            entry.last_access = t;
            entry.use_count += 1;
        }
    }

    /// Remove bytes from the tracked total (after external unload).
    pub fn release_bytes(&mut self, size_bytes: usize) {
        self.current_size_bytes = self.current_size_bytes.saturating_sub(size_bytes);
    }

    /// Evict least-recently-used assets to fit within size limit
    fn evict(&mut self) {
        while self.current_size_bytes > self.max_size_bytes && !self.entries.is_empty() {
            // Find least recently used entry
            let lru_handle = self.entries.iter()
                .min_by_key(|(_, entry)| entry.last_access)
                .map(|(handle, _)| *handle);

            if let Some(handle) = lru_handle {
                if let Some(entry) = self.entries.remove(&handle) {
                    self.current_size_bytes -= entry.size_bytes;
                }
            } else {
                break;
            }
        }
    }

    /// Get cache statistics
    pub fn stats(&self) -> CacheStats {
        CacheStats {
            entry_count: self.entries.len(),
            current_size_bytes: self.current_size_bytes,
            max_size_bytes: self.max_size_bytes,
        }
    }

    fn tick(&self) -> u64 {
        use std::sync::atomic::{AtomicU64, Ordering};
        static TICK: AtomicU64 = AtomicU64::new(0);
        TICK.fetch_add(1, Ordering::Relaxed)
    }
}

/// Cache statistics
#[derive(Debug)]
pub struct CacheStats {
    pub entry_count: usize,
    pub current_size_bytes: usize,
    pub max_size_bytes: usize,
}

impl CacheStats {
    /// Get human-readable size
    pub fn size_human(&self) -> String {
        format_size(self.current_size_bytes)
    }

    pub fn max_size_human(&self) -> String {
        format_size(self.max_size_bytes)
    }
}

fn format_size(bytes: usize) -> String {
    if bytes < 1024 {
        format!("{} B", bytes)
    } else if bytes < 1024 * 1024 {
        format!("{:.1} KB", bytes as f64 / 1024.0)
    } else if bytes < 1024 * 1024 * 1024 {
        format!("{:.1} MB", bytes as f64 / (1024.0 * 1024.0))
    } else {
        format!("{:.1} GB", bytes as f64 / (1024.0 * 1024.0 * 1024.0))
    }
}

