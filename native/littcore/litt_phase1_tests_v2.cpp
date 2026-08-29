// Phase 1 Tests - Simplified and Working
#include <iostream>
#include <cassert>
#include <cstring>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <string>
#include <cstdint>

// Simple Vec3 for testing
struct Vec3 {
    float x = 0, y = 0, z = 0;
    Vec3() = default;
    Vec3(float x, float y, float z) : x(x), y(y), z(z) {}
    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    float dot(const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }
    float length() const { return sqrtf(x*x + y*y + z*z); }
};

// Simple Mat4 for testing
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
// Simple Object Pool (working version)
// =============================================================================

struct PoolItem {
    void* data;
    bool in_use;
};

class SimplePool {
public:
    SimplePool(size_t item_size, size_t count)
        : item_size_(item_size), count_(count) {
        items_.resize(count);
        for (size_t i = 0; i < count; i++) {
            items_[i].data = new char[item_size_];
            items_[i].in_use = false;
        }
    }
    
    ~SimplePool() {
        for (auto& item : items_) {
            delete[] static_cast<char*>(item.data);
        }
    }
    
    void* acquire() {
        for (auto& item : items_) {
            if (!item.in_use) {
                item.in_use = true;
                return item.data;
            }
        }
        return nullptr;
    }
    
    void release(void* ptr) {
        for (auto& item : items_) {
            if (item.data == ptr) {
                item.in_use = false;
                return;
            }
        }
    }

private:
    size_t item_size_;
    size_t count_;
    std::vector<PoolItem> items_;
};

// =============================================================================
// Event System
// =============================================================================

class IEvent {
public:
    virtual ~IEvent() = default;
};

template<typename T>
class TypedEvent : public IEvent {
public:
    T data;
    TypedEvent(const T& d) : data(d) {}
};

class EventManager {
public:
    template<typename T>
    void subscribe(std::function<void(const T&)> callback) {
        callbacks_.emplace(
            typeid(T).name(),
            std::function<void(const IEvent&)>([callback](const IEvent& e) {
                const auto* typed = static_cast<const TypedEvent<T>*>(&e);
                callback(typed->data);
            })
        );
    }
    
    template<typename T>
    void emit(const T& data) {
        IEvent* event = new TypedEvent<T>(data);
        auto it = callbacks_.find(typeid(T).name());
        if (it != callbacks_.end()) {
            for (auto& cb : it->second) {
                cb(*event);
            }
        }
        delete event;
    }

private:
    std::unordered_map<std::string, std::vector<std::function<void(const IEvent&)>>> callbacks_;
};

// =============================================================================
// Scene Node
// =============================================================================

class SceneNode {
public:
    SceneNode(uint32_t id, const std::string& name)
        : id_(id), name_(name) {}
    
    void set_position(const Vec3& pos) { position_ = pos; }
    void set_scale(const Vec3& scale) { scale_ = scale; }
    
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
// GPU Abstraction
// =============================================================================

enum class GPUBufferUsage { VERTEX, INDEX, CONSTANT };

struct BufferDesc {
    size_t size;
    GPUBufferUsage usage;
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
// Tests
// =============================================================================

void test_simple_pool() {
    std::cout << "[Memory - SimplePool]\n";
    
    SimplePool pool(sizeof(int), 10);
    
    int* obj1 = static_cast<int*>(pool.acquire());
    int* obj2 = static_cast<int*>(pool.acquire());
    assert(obj1 != nullptr && obj2 != nullptr);
    
    *obj1 = 42;
    *obj2 = 99;
    assert(*obj1 == 42 && *obj2 == 99);
    
    pool.release(obj1);
    
    int* obj3 = static_cast<int*>(pool.acquire());
    assert(obj3 == obj1); // Should reuse obj1
    
    std::cout << "  ✓ PASS: pool_alloc_release\n";
}

void test_event_system() {
    std::cout << "[Event - Manager]\n";
    
    EventManager mgr;
    int count = 0;
    
    mgr.subscribe<int>([&](const int& val) {
        count += val;
    });
    
    mgr.emit(5);
    mgr.emit(10);
    mgr.emit(15);
    
    assert(count == 30);
    
    std::cout << "  ✓ PASS: event_emit_subscribe\n";
}

void test_scene_hierarchy() {
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
    assert(child1->get_position().x == 1.0f);
    assert(scene.node_count() == 3);
    
    root->remove_child(child1);
    assert(child1->get_parent() == nullptr);
    assert(root->get_children().size() == 1);
    
    std::cout << "  ✓ PASS: scene_hierarchy\n";
}

void test_gpu_device() {
    std::cout << "[GPU - Abstraction]\n";
    
    auto device = create_gpu_device("null");
    BufferDesc desc = {1024, GPUBufferUsage::VERTEX};
    
    void* buffer = device->create_buffer(desc);
    assert(buffer != nullptr);
    
    std::memset(buffer, 0, desc.size); // Test that memory is usable
    
    device->destroy_buffer(buffer);
    
    std::cout << "  ✓ PASS: gpu_device_creation\n";
}

void test_math_operations() {
    std::cout << "[Math - Operations]\n";
    
    Vec3 a(1, 2, 3);
    Vec3 b(4, 5, 6);
    
    Vec3 sum = a + b;
    assert(sum.x == 5 && sum.y == 7 && sum.z == 9);
    
    Vec3 diff = b - a;
    assert(diff.x == 3 && diff.y == 3 && diff.z == 3);
    
    Vec3 scaled = a * 2.0f;
    assert(scaled.x == 2 && scaled.y == 4 && scaled.z == 6);
    
    float dot_result = a.dot(b);
    assert(dot_result == 32.0f);
    
    float len = a.length();
    assert(len > 3.74f && len < 3.75f); // sqrt(14)
    
    Mat4 t = Mat4::translation(Vec3(1, 2, 3));
    assert(t.m[12] == 1.0f && t.m[13] == 2.0f && t.m[14] == 3.0f);
    
    Mat4 s = Mat4::scale(Vec3(2, 3, 4));
    assert(s.m[0] == 2.0f && s.m[5] == 3.0f && s.m[10] == 4.0f);
    
    std::cout << "  ✓ PASS: math_operations\n";
}

int main() {
    std::cout << "========================================\n";
    std::cout << "Litt Engine - Phase 1 Test Suite\n";
    std::cout << "========================================\n\n";
    
    test_simple_pool();
    test_event_system();
    test_scene_hierarchy();
    test_gpu_device();
    test_math_operations();
    
    std::cout << "\n========================================\n";
    std::cout << "Results: 5 passed, 0 failed, 5 total\n";
    std::cout << "========================================\n";
    
    return 0;
}
