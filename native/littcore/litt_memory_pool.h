// Phase 4: Optimization & Performance - Memory Pool & Object Pool

#pragma once

#include "litt_math.h"
#include <vector>
#include <memory>
#include <cstdint>

namespace litt {

// Fixed-size memory pool
class MemoryPool {
public:
    MemoryPool(size_t block_size, uint32_t block_count);
    ~MemoryPool();
    
    // Allocate block
    void* allocate();
    
    // Free block
    void deallocate(void* ptr);
    
    // Get block size
    size_t get_block_size() const { return block_size_; }
    
    // Get free block count
    uint32_t get_free_count() const { return free_count_; }
    
    // Get total block count
    uint32_t get_total_count() const { return total_count_; }
    
    // Is pool empty
    bool is_empty() const { return free_count_ == 0; }
    
    // Clear all allocations
    void clear();

private:
    size_t block_size_;
    uint32_t total_count_;
    uint32_t free_count_;
    std::vector<void*> free_list_;
    std::vector<uint8_t> memory_;
};

// Object pool template
template<typename T>
class ObjectPool {
public:
    ObjectPool(uint32_t count) : count_(count) {
        objects_.resize(count);
        free_list_.reserve(count);
        for (uint32_t i = 0; i < count; i++) {
            free_list_.push_back(&objects_[i]);
        }
    }
    
    // Acquire object
    T* acquire() {
        if (free_list_.empty()) return nullptr;
        T* obj = free_list_.back();
        free_list_.pop_back();
        return obj;
    }
    
    // Release object
    void release(T* obj) {
        if (obj >= objects_.data() && obj < objects_.data() + count_) {
            free_list_.push_back(obj);
        }
    }
    
    // Get free count
    uint32_t get_free_count() const { return static_cast<uint32_t>(free_list_.size()); }
    
    // Get total count
    uint32_t get_total_count() const { return count_; }

private:
    uint32_t count_;
    std::vector<T> objects_;
    std::vector<T*> free_list_;
};

// Pool manager
class PoolManager {
public:
    static PoolManager& get_instance() {
        static PoolManager instance;
        return instance;
    }
    
    // Create memory pool
    MemoryPool* create_pool(const std::string& name, size_t block_size, uint32_t block_count);
    
    // Get pool
    MemoryPool* get_pool(const std::string& name);
    
    // Remove pool
    void remove_pool(const std::string& name);
    
    // Clear all pools
    void clear();

private:
    PoolManager() = default;
    std::unordered_map<std::string, std::unique_ptr<MemoryPool>> pools_;
};

} // namespace litt
