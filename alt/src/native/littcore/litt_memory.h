// LittMemory - Memory management and object pooling for Litt Engine
// Fixed: P0-001 through P0-006

#pragma once
#include <vector>
#include <memory>
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <new>
#include <type_traits>
#include <cstdlib>
#include <cstring>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <utility>

#ifndef LITT_CACHE_LINE
#define LITT_CACHE_LINE 64
#endif

namespace litt {

namespace memory_detail {

inline bool is_power_of_two(size_t value) {
    return value != 0 && (value & (value - 1)) == 0;
}

inline size_t checked_add(size_t a, size_t b) {
    if (b > std::numeric_limits<size_t>::max() - a) throw std::bad_alloc();
    return a + b;
}

inline size_t checked_mul(size_t a, size_t b) {
    if (a != 0 && b > std::numeric_limits<size_t>::max() / a) throw std::bad_alloc();
    return a * b;
}

inline size_t align_up(size_t value, size_t alignment) {
    if (!is_power_of_two(alignment)) throw std::invalid_argument("alignment must be a power of two");
    const size_t mask = alignment - 1;
    return checked_add(value, mask) & ~mask;
}

inline size_t aligned_offset(const void* base, size_t offset, size_t alignment) {
    if (!is_power_of_two(alignment)) throw std::invalid_argument("alignment must be a power of two");
    const uintptr_t addr = reinterpret_cast<uintptr_t>(base);
    if (offset > std::numeric_limits<uintptr_t>::max() - addr) throw std::bad_alloc();
    const uintptr_t current = addr + offset;
    const uintptr_t mask = static_cast<uintptr_t>(alignment - 1);
    if (current > std::numeric_limits<uintptr_t>::max() - mask) throw std::bad_alloc();
    const uintptr_t aligned = (current + mask) & ~mask;
    return static_cast<size_t>(aligned - addr);
}

} // namespace memory_detail

// =============================================================================
// Object Pool - Fixed: no invalid frees, no double-destruction, no UB
// =============================================================================
template<typename T, size_t InitialCapacity = 128>
class ObjectPool {
public:
    ObjectPool() {
        try {
            for (size_t i = 0; i < InitialCapacity; i++) {
                available.push_back(allocate_slot());
            }
        } catch (...) {
            for (auto& slot : available) std::free(slot.raw);
            throw;
        }
    }

    ~ObjectPool() {
        for (auto& slot : available) {
            // Raw storage only. Do not destruct.
            std::free(slot.raw);
        }
        for (auto& slot : in_use) {
            slot.aligned->~T();
            std::free(slot.raw);
        }
    }

    ObjectPool(const ObjectPool&) = delete;
    ObjectPool& operator=(const ObjectPool&) = delete;

    using Handle = T*;

    static Handle invalid_handle() { return nullptr; }

    Handle acquire() {
        PoolSlot slot;
        if (!available.empty()) {
            slot = available.back();
            available.pop_back();
        } else {
            slot = allocate_slot();
        }

        bool constructed = false;
        try {
            new (slot.aligned) T();
            constructed = true;
            in_use.push_back(slot);
        } catch (...) {
            if (constructed) slot.aligned->~T();
            available.push_back(slot);
            throw;
        }
        return slot.aligned;
    }

    void release(Handle obj) {
        if (!obj) return;

        for (auto it = in_use.begin(); it != in_use.end(); ++it) {
            if (it->aligned == obj) {
                obj->~T();
                available.push_back(*it);
                in_use.erase(it);
                return;
            }
        }
        throw std::invalid_argument("object pool handle is not live");
    }

    bool is_valid(Handle obj) const {
        if (!obj) return false;
        for (const auto& slot : in_use) {
            if (slot.aligned == obj) return true;
        }
        return false;
    }

    size_t live_count() const { return in_use.size(); }
    size_t capacity() const { return in_use.size() + available.size(); }

private:
    struct PoolSlot {
        void* raw = nullptr;
        T* aligned = nullptr;
    };

    static PoolSlot allocate_slot() {
        PoolSlot slot;
        constexpr size_t alignment = alignof(T);
        const size_t alloc_size = memory_detail::checked_add(sizeof(T), alignment - 1);
        slot.raw = std::malloc(alloc_size);
        if (!slot.raw) throw std::bad_alloc();

        const uintptr_t addr = reinterpret_cast<uintptr_t>(slot.raw);
        const uintptr_t mask = static_cast<uintptr_t>(alignment - 1);
        const uintptr_t aligned = (addr + mask) & ~mask;
        slot.aligned = reinterpret_cast<T*>(aligned);
        return slot;
    }

    std::vector<PoolSlot> in_use;
    std::vector<PoolSlot> available;
};

// =============================================================================
// AlignedAllocator - Fixed: actually guarantees alignment
// =============================================================================
template<typename T, size_t Alignment = LITT_CACHE_LINE>
class AlignedAllocator {
public:
    using value_type = T;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    template<typename U>
    struct rebind {
        using other = AlignedAllocator<U, Alignment>;
    };

    AlignedAllocator() noexcept = default;

    template<typename U>
    AlignedAllocator(const AlignedAllocator<U, Alignment>&) noexcept {}

    pointer allocate(size_type n) {
        static_assert(Alignment != 0 && (Alignment & (Alignment - 1)) == 0,
                      "Alignment must be a power of two");
        if (n == 0) return nullptr;

        constexpr size_type type_alignment = alignof(T);
        constexpr size_type pointer_alignment = alignof(void*);
        constexpr size_type effective_alignment =
            Alignment > type_alignment
                ? (Alignment > pointer_alignment ? Alignment : pointer_alignment)
                : (type_alignment > pointer_alignment ? type_alignment : pointer_alignment);

        const size_type bytes = memory_detail::checked_mul(n, sizeof(T));
        void* raw = nullptr;
#ifdef _WIN32
        raw = _aligned_malloc(bytes, effective_alignment);
        if (!raw) throw std::bad_alloc();
#else
        const size_type alloc_size = memory_detail::align_up(bytes, effective_alignment);
        raw = std::aligned_alloc(effective_alignment, alloc_size);
        if (!raw) throw std::bad_alloc();
#endif
        return reinterpret_cast<pointer>(raw);
    }

    void deallocate(pointer p, size_type n) noexcept {
        if (!p) return;
#ifdef _WIN32
        _aligned_free((void*)p);
#else
        std::free((void*)p);
#endif
    }
};

// =============================================================================
// Bump Allocator - Fixed: chunked growth, no realloc, pointers stay valid
// =============================================================================
class BumpAllocator {
public:
    struct Chunk {
        char* buffer;
        size_t capacity;
        Chunk* next;
    };

    struct Checkpoint {
        Chunk* chunk = nullptr;
        size_t offset = 0;
    };

    BumpAllocator(size_t chunk_size = 1024 * 1024)
        : chunk_size_(chunk_size), current_chunk_(nullptr), offset_(0) {
        if (chunk_size_ == 0) throw std::invalid_argument("chunk size must be greater than zero");
        current_chunk_ = allocate_chunk(chunk_size_);
    }

    ~BumpAllocator() {
        Chunk* chunk = current_chunk_;
        while (chunk) {
            Chunk* next = chunk->next;
            std::free(chunk->buffer);
            std::free(chunk);
            chunk = next;
        }
    }

    BumpAllocator(const BumpAllocator&) = delete;
    BumpAllocator& operator=(const BumpAllocator&) = delete;

    void* allocate(size_t size, size_t alignment = 16) {
        if (!memory_detail::is_power_of_two(alignment)) {
            throw std::invalid_argument("alignment must be a power of two");
        }

        size_t aligned_offset = memory_detail::aligned_offset(
            current_chunk_->buffer, offset_, alignment);

        if (aligned_offset > current_chunk_->capacity ||
            size > current_chunk_->capacity - aligned_offset) {
            const size_t minimum = memory_detail::checked_add(size, alignment - 1);
            Chunk* new_chunk = allocate_chunk(std::max(minimum, chunk_size_));
            new_chunk->next = current_chunk_;
            current_chunk_ = new_chunk;
            aligned_offset = memory_detail::aligned_offset(
                current_chunk_->buffer, 0, alignment);
        }

        if (aligned_offset > current_chunk_->capacity ||
            size > current_chunk_->capacity - aligned_offset) {
            throw std::bad_alloc();
        }

        void* ptr = current_chunk_->buffer + aligned_offset;
        offset_ = aligned_offset + size;
        return ptr;
    }

    template<typename T>
    T* allocate() {
        return static_cast<T*>(allocate(sizeof(T), alignof(T)));
    }

    template<typename T, typename... Args>
    T* construct(Args&&... args) {
        void* mem = allocate(sizeof(T), alignof(T));
        return new (mem) T(std::forward<Args>(args)...);
    }

    void reset() {
        // New chunks are pushed in front of the original chunk.  Drop those
        // newer chunks and keep the oldest backing chunk for reuse.
        while (current_chunk_ && current_chunk_->next) {
            Chunk* old_head = current_chunk_;
            current_chunk_ = current_chunk_->next;
            std::free(old_head->buffer);
            std::free(old_head);
        }
        offset_ = 0;
    }

    Checkpoint checkpoint() const { return {current_chunk_, offset_}; }

    void rollback(Checkpoint cp) {
        if (!cp.chunk || cp.offset > cp.chunk->capacity) {
            throw std::invalid_argument("invalid bump allocator checkpoint");
        }

        bool found = false;
        for (Chunk* chunk = current_chunk_; chunk; chunk = chunk->next) {
            if (chunk == cp.chunk) {
                found = true;
                break;
            }
        }
        if (!found) throw std::invalid_argument("checkpoint does not belong to allocator");

        while (current_chunk_ != cp.chunk) {
            Chunk* newer = current_chunk_;
            current_chunk_ = current_chunk_->next;
            std::free(newer->buffer);
            std::free(newer);
        }
        offset_ = cp.offset;
    }

    size_t chunk_size() const { return chunk_size_; }
    size_t offset() const { return offset_; }

private:
    static Chunk* allocate_chunk(size_t size) {
        Chunk* chunk = static_cast<Chunk*>(std::malloc(sizeof(Chunk)));
        if (!chunk) throw std::bad_alloc();
        chunk->buffer = static_cast<char*>(std::malloc(size));
        if (!chunk->buffer) { std::free(chunk); throw std::bad_alloc(); }
        chunk->capacity = size;
        chunk->next = nullptr;
        return chunk;
    }

    size_t chunk_size_;
    Chunk* current_chunk_;
    size_t offset_;
};

// =============================================================================
// Arena Allocator - Fixed: checkpoint actually restores state
// =============================================================================
class ArenaAllocator {
public:
    explicit ArenaAllocator(size_t chunk_size = 1024 * 1024)
        : bump_(chunk_size) {
    }

    ~ArenaAllocator() = default;

    ArenaAllocator(const ArenaAllocator&) = delete;
    ArenaAllocator& operator=(const ArenaAllocator&) = delete;

    void* allocate(size_t size, size_t alignment = 16) {
        return bump_.allocate(size, alignment);
    }

    template<typename T>
    T* allocate() {
        return bump_.template allocate<T>();
    }

    template<typename T, typename... Args>
    T* construct(Args&&... args) {
        return bump_.template construct<T>(std::forward<Args>(args)...);
    }

    using Checkpoint = BumpAllocator::Checkpoint;

    Checkpoint checkpoint() const {
        return bump_.checkpoint();
    }

    void rollback(Checkpoint cp) {
        bump_.rollback(cp);
    }

    size_t allocated() const { return bump_.offset(); }
    size_t chunk_size() const { return bump_.chunk_size(); }

private:
    BumpAllocator& bump() { return bump_; }
    BumpAllocator bump_;
};

// =============================================================================
// Free List Allocator - Fixed: validates block size, tracks slabs
// =============================================================================
class FreeListAllocator {
public:
    struct FreeBlock {
        FreeBlock* next;
    };

private:
    struct Slab {
        FreeBlock* blocks;
        size_t block_count;
        Slab* next;
    };

public:
    explicit FreeListAllocator(size_t block_size, size_t initial_blocks = 256)
        : block_size_(memory_detail::align_up(
              std::max(block_size, sizeof(FreeBlock)), alignof(std::max_align_t)))
        , initial_blocks_(std::max<size_t>(initial_blocks, 1)) {
        allocate_slab(initial_blocks_);
    }

    ~FreeListAllocator() {
        for (Slab* slab = slabs_; slab;) {
            Slab* next = slab->next;
            std::free(slab->blocks);
            std::free(slab);
            slab = next;
        }
    }

    FreeListAllocator(const FreeListAllocator&) = delete;
    FreeListAllocator& operator=(const FreeListAllocator&) = delete;

    void* allocate() {
        if (!free_list_) {
            // Growth: allocate a new slab
            size_t count = std::max(size_t(32), initial_blocks_);
            allocate_slab(count);
        }
        FreeBlock* block = free_list_;
        free_list_ = block->next;
        return block;
    }

    void deallocate(void* ptr) {
        if (!ptr) return;
        if (!owns(ptr)) throw std::invalid_argument("pointer does not belong to free list allocator");
        for (FreeBlock* b = free_list_; b; b = b->next) {
            if (b == ptr) throw std::invalid_argument("double free in free list allocator");
        }
        FreeBlock* block = static_cast<FreeBlock*>(ptr);
        block->next = free_list_;
        free_list_ = block;
    }

    size_t free_count() const {
        size_t count = 0;
        for (FreeBlock* b = free_list_; b; b = b->next) ++count;
        return count;
    }

private:
    bool owns(const void* ptr) const {
        const auto address = reinterpret_cast<uintptr_t>(ptr);
        for (Slab* slab = slabs_; slab; slab = slab->next) {
            const auto begin = reinterpret_cast<uintptr_t>(slab->blocks);
            const size_t bytes = memory_detail::checked_mul(block_size_, slab->block_count);
            if (address >= begin) {
                const uintptr_t offset = address - begin;
                if (offset < bytes && (offset % block_size_) == 0) return true;
            }
        }
        return false;
    }

    static Slab* create_slab(size_t block_size, size_t count) {
        const size_t total = memory_detail::checked_mul(block_size, count);
        void* raw = std::malloc(total);
        if (!raw) throw std::bad_alloc();

        auto* slab = static_cast<Slab*>(std::malloc(sizeof(Slab)));
        if (!slab) { std::free(raw); throw std::bad_alloc(); }

        slab->blocks = static_cast<FreeBlock*>(raw);
        slab->block_count = count;
        slab->next = nullptr;

        // Chain all blocks in the slab
        auto* bytes = static_cast<std::byte*>(raw);
        for (size_t i = 0; i < count; i++) {
            auto* block = reinterpret_cast<FreeBlock*>(bytes + i * block_size);
            auto* next = (i + 1 < count) ? reinterpret_cast<FreeBlock*>(bytes + (i + 1) * block_size) : nullptr;
            block->next = next;
        }
        return slab;
    }

    void allocate_slab(size_t count) {
        Slab* slab = create_slab(block_size_, count);
        slab->next = slabs_;
        slabs_ = slab;
        free_list_ = slab->blocks;
    }

    size_t block_size_;
    size_t initial_blocks_;
    Slab* slabs_ = nullptr;
    FreeBlock* free_list_ = nullptr;
};

} // namespace litt
