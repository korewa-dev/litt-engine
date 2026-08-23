//! Memory profiler -- tracks GPU memory allocations and usage.
//! Integrates with VMA (Vulkan Memory Allocator) for GPU memory monitoring.

use std::collections::HashMap;

/// Memory allocation
#[derive(Debug, Clone)]
pub struct MemoryAlloc {
    pub name: String,
    pub size_bytes: usize,
    pub allocation_time_ms: f32,
    pub freed: bool,
}

/// Memory pool
#[derive(Debug, Clone)]
pub struct MemoryPool {
    pub name: String,
    pub total_bytes: usize,
    pub used_bytes: usize,
    pub alloc_count: usize,
    pub peak_bytes: usize,
}

impl Default for MemoryPool {
    fn default() -> Self {
        Self {
            name: "Unknown".to_string(),
            total_bytes: 0,
            used_bytes: 0,
            alloc_count: 0,
            peak_bytes: 0,
        }
    }
}

/// GPU memory stats
#[derive(Debug, Default)]
pub struct GpuMemoryStats {
    pub total_allocated: usize,
    pub total_freed: usize,
    pub current_usage: usize,
    pub peak_usage: usize,
    pub alloc_count: usize,
    pub pools: HashMap<String, MemoryPool>,
    pub allocations: Vec<MemoryAlloc>,
}

impl GpuMemoryStats {
    pub fn new() -> Self { Self::default() }

    /// Record a new allocation
    pub fn alloc(&mut self, name: &str, size_bytes: usize, pool: &str) {
        self.allocations.push(MemoryAlloc {
            name: name.to_string(),
            size_bytes,
            allocation_time_ms: 0.0,
            freed: false,
        });
        self.total_allocated += size_bytes;
        self.current_usage += size_bytes;
        self.alloc_count += 1;
        if self.current_usage > self.peak_usage {
            self.peak_usage = self.current_usage;
        }

        let pool = self.pools.entry(pool.to_string()).or_insert_with(|| MemoryPool {
            name: pool.to_string(),
            ..Default::default()
        });
        pool.used_bytes += size_bytes;
        pool.alloc_count += 1;
        if pool.used_bytes > pool.peak_bytes {
            pool.peak_bytes = pool.used_bytes;
        }
    }

    /// Free an allocation
    pub fn free(&mut self, name: &str) {
        if let Some(idx) = self.allocations.iter().position(|a| a.name == name && !a.freed) {
            let size = self.allocations[idx].size_bytes;
            self.total_freed += size;
            self.current_usage = self.current_usage.saturating_sub(size);
            self.allocations[idx].freed = true;
        }
    }

    /// Get usage in MB
    pub fn usage_mb(&self) -> f32 {
        self.current_usage as f32 / (1024.0 * 1024.0)
    }

    /// Get peak usage in MB
    pub fn peak_mb(&self) -> f32 {
        self.peak_usage as f32 / (1024.0 * 1024.0)
    }

    /// Get human-readable report
    pub fn report(&self) -> String {
        format!(
            "GPU Memory: {:.1} MB used / {:.1} MB peak ({:.0} allocs)",
            self.usage_mb(),
            self.peak_mb(),
            self.alloc_count
        )
    }
}

/// Memory pressure indicator
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum MemoryPressure {
    Low,    // < 50%
    Medium, // 50-75%
    High,   // 75-90%
    Critical, // > 90%
}

impl MemoryPressure {
    pub fn from_usage(used_mb: f32, total_mb: f32) -> Self {
        if total_mb <= 0.0 { return Self::Low; }
        let ratio = used_mb / total_mb;
        if ratio < 0.5 { Self::Low }
        else if ratio < 0.75 { Self::Medium }
        else if ratio < 0.9 { Self::High }
        else { Self::Critical }
    }
}
