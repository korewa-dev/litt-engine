//! Custom memory allocators for game engines.
//!
//! Provides:
//! - Arena/Stack allocator: O(1) allocations, batch reset every frame
//! - Pool allocator: Fixed-size chunks for game objects
//! - Bump allocator: Sequential allocation without fragmentation

use std::alloc::{alloc, dealloc, Layout};
use std::cell::Cell;
use std::collections::HashMap;
use std::sync::Arc;
use std::sync::atomic::{AtomicUsize, Ordering};

// =============================================================================
// Arena/Stack Allocator
// =============================================================================

/// Chunk of memory for arena allocation
struct ArenaChunk {
    memory: *mut u8,
    size: usize,
    current: Cell<usize>,
}

impl ArenaChunk {
    fn new(size: usize) -> Self {
        let layout = Layout::from_size_align(size, 64).unwrap();
        unsafe {
            let memory = alloc(layout);
            if memory.is_null() {
                panic!("Arena allocation failed");
            }
            Self {
                memory,
                size,
                current: Cell::new(0),
            }
        }
    }

    fn alloc(&self, size: usize, align: usize) -> Option<*mut u8> {
        let current = self.current.get();
        let aligned_current = (current + align - 1) & !(align - 1);
        if aligned_current + size > self.size {
            return None;
        }
        self.current.set(aligned_current + size);
        unsafe { Some(self.memory.add(aligned_current)) }
    }

    fn reset(&self) {
        self.current.set(0);
    }
}

impl Drop for ArenaChunk {
    fn drop(&mut self) {
        let layout = Layout::from_size_align(self.size, 64).unwrap();
        unsafe { dealloc(self.memory, layout) };
    }
}

/// Fast arena allocator for frame-scoped allocations
pub struct Arena {
    chunks: Vec<Arc<ArenaChunk>>,
    chunk_size: usize,
    allocation_count: AtomicUsize,
}

impl Default for Arena {
    fn default() -> Self {
        Self::with_chunk_size(1024 * 1024) // 1MB chunks
    }
}

impl Arena {
    /// Create a new arena with the given chunk size
    pub fn with_chunk_size(chunk_size: usize) -> Self {
        Self {
            chunks: Vec::new(),
            chunk_size,
            allocation_count: AtomicUsize::new(0),
        }
    }

    /// Allocate memory from the arena
    pub fn alloc(&mut self, size: usize, align: usize) -> *mut u8 {
        // Try current chunk first
        if let Some(chunk) = self.chunks.last() {
            if let Some(ptr) = chunk.alloc(size, align) {
                self.allocation_count.fetch_add(1, Ordering::Relaxed);
                return ptr;
            }
        }

        // Create new chunk
        let chunk = Arc::new(ArenaChunk::new(self.chunk_size));
        if let Some(ptr) = chunk.alloc(size, align) {
            self.chunks.push(chunk);
            self.allocation_count.fetch_add(1, Ordering::Relaxed);
            return ptr;
        }

        panic!("Arena allocation failed");
    }

    /// Allocate a typed value
    pub fn alloc_typed<T>(&mut self, value: T) -> *mut T {
        let ptr = self.alloc(std::mem::size_of::<T>(), std::mem::align_of::<T>()) as *mut T;
        unsafe { ptr.write(value) }
        ptr
    }

    /// Reset the arena (release all allocations, keep chunks)
    pub fn reset(&self) {
        for chunk in &self.chunks {
            chunk.reset();
        }
    }

    /// Get total allocated memory
    pub fn allocated_bytes(&self) -> usize {
        self.chunks.iter().map(|c| c.current.get()).sum()
    }

    /// Get allocation count
    pub fn allocation_count(&self) -> usize {
        self.allocation_count.load(Ordering::Relaxed)
    }
}

// =============================================================================
// Pool Allocator
// =============================================================================

/// Block of fixed-size chunks for pool allocation.
/// Free list head is atomic so blocks can live behind Arc.
struct PoolBlock {
    memory: *mut u8,
    free_list: AtomicUsize, // offset of first free chunk, or FREE_END
    chunk_size: usize,
    chunk_count: usize,
}

const FREE_END: usize = usize::MAX;

impl PoolBlock {
    fn new(chunk_size: usize, chunk_count: usize) -> Self {
        let total_size = chunk_size * chunk_count;
        let layout = Layout::from_size_align(total_size, 64).unwrap();
        unsafe {
            let memory = alloc(layout);
            if memory.is_null() {
                panic!("Pool allocation failed");
            }

            // Build free list (next pointer stored inside each chunk)
            let mut free = FREE_END;
            for i in (0..chunk_count).rev() {
                let offset = i * chunk_size;
                *(memory.add(offset) as *mut usize) = free;
                free = offset;
            }

            Self {
                memory,
                free_list: AtomicUsize::new(free),
                chunk_size,
                chunk_count,
            }
        }
    }
}

impl Drop for PoolBlock {
    fn drop(&mut self) {
        let total_size = self.chunk_size * self.chunk_count;
        let layout = Layout::from_size_align(total_size, 64).unwrap();
        unsafe { dealloc(self.memory, layout) };
    }
}

/// Pool allocator for fixed-size allocations
pub struct Pool {
    chunk_size: usize,
    blocks: Vec<Arc<PoolBlock>>,
    allocation_count: AtomicUsize,
}

impl Pool {
    /// Create a new pool
    pub fn new(chunk_size: usize) -> Self {
        assert!(chunk_size >= 8, "Chunk size must be at least 8 bytes");
        assert!(chunk_size.is_power_of_two(), "Chunk size must be power of 2 for alignment");
        Self {
            chunk_size,
            blocks: Vec::new(),
            allocation_count: AtomicUsize::new(0),
        }
    }

    /// Allocate a chunk from the pool
    pub fn alloc(&mut self) -> *mut u8 {
        // Try to pop from the current block's free list
        if let Some(block) = self.blocks.last() {
            let head = block.free_list.load(Ordering::Relaxed);
            if head != FREE_END {
                let next = unsafe { *(block.memory.add(head) as *const usize) };
                block.free_list.store(next, Ordering::Relaxed);
                self.allocation_count.fetch_add(1, Ordering::Relaxed);
                return unsafe { block.memory.add(head) };
            }
        }

        // Create new block
        let block = Arc::new(PoolBlock::new(self.chunk_size, 256));
        self.blocks.push(block.clone());
        self.allocation_count.fetch_add(1, Ordering::Relaxed);
        let head = block.free_list.load(Ordering::Relaxed);
        if head != FREE_END {
            let next = unsafe { *(block.memory.add(head) as *const usize) };
            block.free_list.store(next, Ordering::Relaxed);
            return unsafe { block.memory.add(head) };
        }

        panic!("Pool allocation failed");
    }

    /// Free a chunk back to the pool
    pub fn free(&mut self, ptr: *mut u8) {
        for block in &self.blocks {
            let block_start = block.memory as usize;
            let block_end = block_start + block.chunk_size * block.chunk_count;
            let ptr_addr = ptr as usize;

            if ptr_addr >= block_start && ptr_addr < block_end {
                let offset = ptr_addr - block_start;
                assert_eq!(offset % block.chunk_size, 0, "Invalid pool pointer");

                unsafe {
                    *(ptr as *mut usize) = block.free_list.load(Ordering::Relaxed);
                }
                block.free_list.store(offset, Ordering::Relaxed);
                self.allocation_count.fetch_sub(1, Ordering::Relaxed);
                return;
            }
        }

        panic!("Invalid pool pointer");
    }

    /// Allocate and initialize a typed value
    pub fn alloc_typed<T>(&mut self, value: T) -> *mut T {
        let ptr = self.alloc() as *mut T;
        unsafe { ptr.write(value) }
        ptr
    }

    /// Free and drop a typed value
    pub fn free_typed<T>(&mut self, ptr: *mut T) {
        unsafe { ptr.drop_in_place() }
        self.free(ptr as *mut u8);
    }

    /// Get pool statistics
    pub fn stats(&self) -> PoolStats {
        PoolStats {
            chunk_size: self.chunk_size,
            allocation_count: self.allocation_count.load(Ordering::Relaxed),
            block_count: self.blocks.len(),
        }
    }
}

/// Pool statistics
#[derive(Debug)]
pub struct PoolStats {
    pub chunk_size: usize,
    pub allocation_count: usize,
    pub block_count: usize,
}

impl PoolStats {
    pub fn total_memory_bytes(&self) -> usize {
        self.block_count * self.chunk_size * 256
    }
}

// =============================================================================
// Bump Allocator
// =============================================================================

/// Simple bump allocator for sequential allocations
pub struct BumpAllocator {
    memory: *mut u8,
    size: usize,
    current: Cell<usize>,
}

impl BumpAllocator {
    /// Create a new bump allocator
    pub fn new(size: usize) -> Self {
        let layout = Layout::from_size_align(size, 64).unwrap();
        unsafe {
            let memory = alloc(layout);
            if memory.is_null() {
                panic!("Bump allocator allocation failed");
            }
            Self {
                memory,
                size,
                current: Cell::new(0),
            }
        }
    }

    /// Allocate from the bump allocator
    pub fn alloc(&self, size: usize) -> Option<*mut u8> {
        let current = self.current.get();
        if current + size > self.size {
            return None;
        }
        self.current.set(current + size);
        unsafe { Some(self.memory.add(current)) }
    }

    /// Reset the allocator
    pub fn reset(&self) {
        self.current.set(0);
    }
}

impl Drop for BumpAllocator {
    fn drop(&mut self) {
        let layout = Layout::from_size_align(self.size, 64).unwrap();
        unsafe { dealloc(self.memory, layout) };
    }
}

// =============================================================================
// Frame Allocator (Arena that resets every frame)
// =============================================================================

/// Frame-scoped allocator that resets automatically
pub struct FrameAllocator {
    arena: Arena,
}

impl Default for FrameAllocator {
    fn default() -> Self {
        Self::new()
    }
}

impl FrameAllocator {
    /// Create a new frame allocator
    pub fn new() -> Self {
        Self {
            arena: Arena::with_chunk_size(4 * 1024 * 1024), // 4MB
        }
    }

    /// Allocate memory
    pub fn alloc(&mut self, size: usize, align: usize) -> *mut u8 {
        self.arena.alloc(size, align)
    }

    /// Reset for next frame
    pub fn reset(&mut self) {
        self.arena.reset();
    }

    /// Get allocation stats
    pub fn stats(&self) -> ArenaStats {
        self.arena.stats()
    }
}

/// Arena statistics
#[derive(Debug, Clone)]
pub struct ArenaStats {
    pub allocated_bytes: usize,
    pub allocation_count: usize,
}

impl Arena {
    pub fn stats(&self) -> ArenaStats {
        ArenaStats {
            allocated_bytes: self.allocated_bytes(),
            allocation_count: self.allocation_count.load(Ordering::Relaxed),
        }
    }
}
