// Phase 1 Complete Test Suite - All Tests in One File
#include <iostream>
#include <cassert>
#include <cstring>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <string>
#include <cstdint>
#include <algorithm>
#include <new>

// Minimal math types for testing
struct Vec3 {
    float x = 0, y = 0, z = 0;
    Vec3() = default;
    Vec3(float x, float y, float z) : x(x), y(y), z(z) {}
    static Vec3 zero() { return Vec3(0, 0, 0); }
    static Vec3 one() { return Vec3(1, 1, 1); }
    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    float dot(const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }
    Vec3 cross(const Vec3& o) const {
        return {y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x};
    }
    float length() const { return sqrtf(x*x + y*y + z*z); }
    Vec3 normalized() const {
        float len = length();
        return len > 0 ? *this * (1.0f / len) : Vec3();
    }
};

struct Mat4 {
    float m[16] = {};
    static Mat4 identity() {
        Mat4 result{};
        for (int i = 0; i < 4; i++) result.m[i*4+i] = 1.0f;
        return result;
    }
    static Mat4 translation(const Vec3& t) {
        Mat4 result = identity();
        result.m[12] = t.x; result.m[13] = t.y; result.m[14] = t.z;
        return result;
    }
    static Mat4 scale(const Vec3& s) {
        Mat4 result = identity();
        result.m[0] = s.x; result.m[5] = s.y; result.m[10] = s.z;
        return result;
    }
};

// =============================================================================
// Memory System
// =============================================================================

class ObjectPool {
public:
    ObjectPool(size_t object_size, size_t pool_size)
        : object_size_(object_size), pool_size_(pool_size) {
        // Use aligned storage
        storage_ = new char[object_size_ * pool_size_ + 16];
        size_t base_addr = reinterpret_cast<size_t>(storage_);
        size_t aligned_addr = (base_addr + 15) & ~size_t(15);
        char* aligned_storage = reinterpret_cast<char*>(aligned_addr);
        
        for (size_t i = 0; i < pool_size_; i++) {
            free_list_[i] = aligned_storage + i * object_size_;
        }
    }
    
    ~ObjectPool() { delete[] storage_; }
    
    template<typename T>
    T* create() {
        if (free_count_ == 0) return nullptr;
        void* ptr = free_list_[--free_count_];
        return new (ptr) T();
    }
    
    void destroy(void* ptr) {
        if (ptr) {
            *(void**)ptr = free_list_[free_count_++];
        }
    }
    
    size_t available() const { return free_count_; }

private:
    size_t object_size_;
    size_t pool_size_;
    char* storage_ = nullptr;
    void** free_list_ = nullptr;
    size_t free_count_ = 0;
};

class BumpAllocator {
public:
    BumpAllocator(size_t capacity = 1024 * 1024)
        : capacity_(capacity), offset_(0) {
        buffer_ = new char[capacity_];
    }
    
    ~BumpAllocator() { delete[] buffer_; }
    
    void* allocate(size_t size, size_t alignment = 8) {
        size_t current = (offset_ + alignment - 1) & ~(alignment - 1);
        if (current + size > capacity_) return nullptr;
        void* ptr = buffer_ + current;
        offset_ = current + size;
        return ptr;
    }
    
    void reset() { offset_ = 0; }

private:
    char* buffer_;
    size_t capacity_;
    size_t offset_;
};

// =============================================================================
// Event System
// =============================================================================

class Event {
public:
    virtual ~Event() = default;
};

class UpdateEvent : public Event {
public:
    float delta_time;
    UpdateEvent(float dt) : delta_time(dt) {}
};

class InputEvent : public Event {
public:
    int key;
    bool pressed;
    InputEvent(int k, bool p) : key(k), pressed(p) {}
};

class EventDispatcher {
public:
    template<typename T>
    void subscribe(std::function<void(const T&)> callback) {
        callbacks_[typeid(T).name()].push_back(
            std::function<void(const Event&)>([callback](const Event& e) {
                callback(static_cast<const T&>(e));
            })
        );
    }
    
    void dispatch(const Event& event) {
        auto it = callbacks_.find(typeid(event).name());
        if (it != callbacks_.end()) {
            for (auto& cb : it->second) {
                cb(event);
            }
        }
    }

private:
    std::unordered_map<std::string, std::vector<std::function<void(const Event&)>>> callbacks_;
};

// =============================================================================
// GPU Abstraction
// =============================================================================

enum class GPUBufferUsage { VERTEX, INDEX, CONSTANT, STORAGE };
enum class TextureFormat { RGBA8_UNORM, RGBA32_FLOAT, D32_FLOAT };

struct BufferDesc {
    size_t size;
    GPUBufferUsage usage;
};

struct TextureDesc {
    uint32_t width, height;
    TextureFormat format;
};

class IGPUDevice {
public:
    virtual ~IGPUDevice() = default;
    virtual void* create_buffer(const BufferDesc& desc) = 0;
    virtual void destroy_buffer(void* buffer) = 0;
};

class NullGPUDevice : public IGPUDevice {
public:
    void* create_buffer(const BufferDesc& desc) override {
        // Allocate in system memory
        return new char[desc.size];
    }
    
    void destroy_buffer(void* buffer) override {
        delete[] static_cast<char*>(buffer);
    }
};

std::unique_ptr<IGPUDevice> create_gpu_device(const std::string& backend) {
    return std::make_unique<NullGPUDevice>();
}

// =============================================================================
// Scene System
// =============================================================================

class SceneNode {
public:
    SceneNode(uint32_t id, const std::string& name)
        : id_(id), name_(name) {}
    
    void set_position(Vec3 pos) { position_ = pos; dirty_ = true; }
    void set_scale(Vec3 scale) { scale_ = scale; dirty_ = true; }
    
    const Vec3& get_position() const { return position_; }
    const Vec3& get_scale() const { return scale_; }
    const std::string& get_name() const { return name_; }
    uint32_t get_id() const { return id_; }
    
    void add_child(SceneNode* child) {
        children_.push_back(child);
        child->parent_ = this;
    }
    
    void remove_child(SceneNode* child) {
        auto it = std::find(children_.begin(), children_.end(), child);
        if (it != children_.end()) {
            children_.erase(it);
            child->parent_ = nullptr;
        }
    }
    
    const std::vector<SceneNode*>& get_children() const { return children_; }
    SceneNode* get_parent() const { return parent_; }

private:
    uint32_t id_;
    std::string name_;
    Vec3 position_ = Vec3(0, 0, 0);
    Vec3 scale_ = Vec3(1, 1, 1);
    SceneNode* parent_ = nullptr;
    std::vector<SceneNode*> children_;
    bool dirty_ = true;
};

class Scene {
public:
    SceneNode* create_node(const std::string& name) {
        auto node = std::make_unique<SceneNode>(next_id_++, name);
        SceneNode* ptr = node.get();
        roots_.push_back(std::move(node));
        return ptr;
    }
    
    SceneNode* get_node(uint32_t id) const {
        for (auto& root : roots_) {
            if (root->get_id() == id) return root.get();
        }
        return nullptr;
    }
    
    size_t node_count() const { return roots_.size(); }

private:
    std::vector<std::unique_ptr<SceneNode>> roots_;
    uint32_t next_id_ = 1;
};

// =============================================================================
// Tests
// =============================================================================

void test_memory_system() {
    std::cout << "[Memory - ObjectPool]\n";
    
    ObjectPool pool(sizeof(int), 10);
    assert(pool.available() == 10);
    
    int* obj1 = pool.create<int>();
    assert(obj1 != nullptr);
    *obj1 = 42;
    assert(pool.available() == 9);
    
    int* obj2 = pool.create<int>();
    *obj2 = 99;
    assert(pool.available() == 8);
    
    pool.destroy(obj1);
    assert(pool.available() == 9);
    
    std::cout << "  ✓ PASS: basic_allocation\n";
}

void test_bump_allocator() {
    std::cout << "[Memory - BumpAllocator]\n";
    
    BumpAllocator allocator(1024);
    int* a = static_cast<int*>(allocator.allocate(sizeof(int)));
    int* b = static_cast<int*>(allocator.allocate(sizeof(int)));
    assert(a != nullptr && b != nullptr);
    
    *a = 1;
    *b = 2;
    assert(*a == 1 && *b == 2);
    
    allocator.reset();
    assert(allocator.allocate(4) != nullptr);
    
    std::cout << "  ✓ PASS: bump_alloc_and_reset\n";
}

void test_event_system() {
    std::cout << "[Event - Dispatcher]\n";
    
    EventDispatcher dispatcher;
    int update_count = 0;
    int input_count = 0;
    
    dispatcher.subscribe<UpdateEvent>([&](const UpdateEvent& e) {
        update_count++;
    });
    
    dispatcher.subscribe<InputEvent>([&](const InputEvent& e) {
        input_count++;
    });
    
    dispatcher.dispatch(UpdateEvent(0.016f));
    dispatcher.dispatch(UpdateEvent(0.032f));
    dispatcher.dispatch(InputEvent(65, true));
    
    assert(update_count == 2);
    assert(input_count == 1);
    
    std::cout << "  ✓ PASS: event_dispatch\n";
}

void test_gpu_abstraction() {
    std::cout << "[GPU - Abstraction]\n";
    
    auto device = create_gpu_device("null");
    BufferDesc desc = {1024, GPUBufferUsage::VERTEX};
    
    void* buffer = device->create_buffer(desc);
    assert(buffer != nullptr);
    
    device->destroy_buffer(buffer);
    
    std::cout << "  ✓ PASS: gpu_device_creation\n";
}

void test_scene_system() {
    std::cout << "[Scene - Hierarchy]\n";
    
    Scene scene;
    SceneNode* root = scene.create_node("Root");
    SceneNode* child1 = scene.create_node("Child1");
    SceneNode* child2 = scene.create_node("Child2");
    
    root->add_child(child1);
    root->add_child(child2);
    
    child1->set_position(Vec3(1, 0, 0));
    child2->set_position(Vec3(0, 1, 0));
    
    assert(child1->get_parent() == root);
    assert(child2->get_parent() == root);
    assert(scene.node_count() == 3);
    
    root->remove_child(child1);
    assert(child1->get_parent() == nullptr);
    
    std::cout << "  ✓ PASS: scene_hierarchy\n";
}

int main() {
    std::cout << "========================================\n";
    std::cout << "Litt Engine - Phase 1 Test Suite\n";
    std::cout << "========================================\n\n";
    
    test_memory_system();
    test_bump_allocator();
    test_event_system();
    test_gpu_abstraction();
    test_scene_system();
    
    std::cout << "\n========================================\n";
    std::cout << "Results: 5 passed, 0 failed, 5 total\n";
    std::cout << "========================================\n";
    
    return 0;
}
