// Phase 1 Tests - Memory, Event, GPU, Scene System Tests
#include "litt_memory.h"
#include "litt_event.h"
#include "litt_gpu.h"
#include "litt_scene_graph.h"
#include <cassert>
#include <iostream>
#include <sstream>
#include <thread>
#include <chrono>
#include <cmath>

using namespace litt;

// =============================================================================
// Memory Tests
// =============================================================================

void test_object_pool_basic() {
    std::cout << "  [test_object_pool_basic] ... ";
    
    ObjectPool<int, 8> pool;
    auto p1 = pool.acquire();
    auto p2 = pool.acquire();
    
    assert(p1 != p2);
    assert(p1 != ObjectPool<int, 8>::invalid_handle());
    
    *p1 = 42;
    *p2 = 100;
    
    assert(*p1 == 42);
    assert(*p2 == 100);
    
    pool.release(p2);
    pool.release(p1);
    
    std::cout << "PASS" << std::endl;
}

void test_object_pool_overflow() {
    std::cout << "  [test_object_pool_overflow] ... ";
    
    ObjectPool<float, 2> pool;
    auto p1 = pool.acquire();
    auto p2 = pool.acquire();
    auto p3 = pool.acquire(); // Should fail
    auto p4 = pool.acquire(); // Should fail
    
    assert(p3 == ObjectPool<float, 2>::invalid_handle());
    assert(p4 == ObjectPool<float, 2>::invalid_handle());
    
    pool.release(p1);
    pool.release(p2);
    
    auto p5 = pool.acquire(); // Should succeed now
    assert(p5 != ObjectPool<float, 2>::invalid_handle());
    
    pool.release(p5);
    
    std::cout << "PASS" << std::endl;
}

void test_aligned_allocator() {
    std::cout << "  [test_aligned_allocator] ... ";
    
    AlignedAllocator<float, 16> alloc;
    float* data = alloc.allocate(100);
    
    // Check alignment (16-byte)
    assert(((uintptr_t)data % 16) == 0);
    
    for (int i = 0; i < 100; i++) {
        data[i] = (float)i * 1.5f;
    }
    
    // Verify values
    assert(data[0] == 0.0f);
    assert(data[1] == 1.5f);
    assert(data[2] == 3.0f);
    
    alloc.deallocate(data, 100);
    
    std::cout << "PASS" << std::endl;
}

void test_memory_pool() {
    std::cout << "  [test_memory_pool] ... ";
    
    MemoryPool<1024> pool;
    
    // Allocate chunks
    for (int i = 0; i < 10; i++) {
        auto chunk = pool.allocate(64);
        assert(chunk != nullptr);
        memset(chunk, 0, 64);
    }
    
    // Pool should still have space
    assert(pool.allocated() <= 1024);
    
    std::cout << "PASS" << std::endl;
}

// =============================================================================
// Event System Tests
// =============================================================================

void test_event_dispatcher_basic() {
    std::cout << "  [test_event_dispatcher_basic] ... ";
    
    EventDispatcher<EventKey::LogEvent> dispatcher;
    int call_count = 0;
    
    auto handler = [&](const std::string& msg) {
        call_count++;
        assert(msg == "test");
    };
    
    dispatcher.subscribe(handler);
    dispatcher.dispatch("test");
    
    assert(call_count == 1);
    
    dispatcher.unsubscribe(handler);
    dispatcher.dispatch("test");
    assert(call_count == 1); // Should not have changed
    
    std::cout << "PASS" << std::endl;
}

void test_event_dispatcher_multiple() {
    std::cout << "  [test_event_dispatcher_multiple] ... ";
    
    EventDispatcher<EventKey::LogEvent> dispatcher;
    std::vector<std::string> results;
    
    auto handler1 = [&](const std::string& msg) {
        results.push_back(msg + "_1");
    };
    
    auto handler2 = [&](const std::string& msg) {
        results.push_back(msg + "_2");
    };
    
    dispatcher.subscribe(handler1);
    dispatcher.subscribe(handler2);
    
    dispatcher.dispatch("test");
    
    assert(results.size() == 2);
    assert(results[0] == "test_1");
    assert(results[1] == "test_2");
    
    std::cout << "PASS" << std::endl;
}

void test_subscriber_count() {
    std::cout << "  [test_subscriber_count] ... ";
    
    EventDispatcher<EventKey::LogEvent> dispatcher;
    int call_count = 0;
    
    auto handler1 = [&](const std::string&) { call_count++; };
    auto handler2 = [&](const std::string&) { call_count++; };
    
    assert(dispatcher.subscriber_count() == 0);
    
    dispatcher.subscribe(handler1);
    assert(dispatcher.subscriber_count() == 1);
    
    dispatcher.subscribe(handler2);
    assert(dispatcher.subscriber_count() == 2);
    
    dispatcher.unsubscribe(handler1);
    assert(dispatcher.subscriber_count() == 1);
    
    dispatcher.unsubscribe(handler2);
    assert(dispatcher.subscriber_count() == 0);
    
    std::cout << "PASS" << std::endl;
}

// =============================================================================
// GPU Tests
// =============================================================================

void test_gpu_buffer_creation() {
    std::cout << "  [test_gpu_buffer_creation] ... ";
    
    // Test that we can create GPU descriptors
    BufferDesc desc;
    desc.size = 4096;
    desc.usage = GPUBufferUsage::VERTEX;
    desc.flags = GPUBufferFlags::CPU_VISIBLE;
    
    assert(desc.size > 0);
    assert(desc.usage == GPUBufferUsage::VERTEX);
    
    std::cout << "PASS" << std::endl;
}

void test_gpu_texture_creation() {
    std::cout << "  [test_gpu_texture_creation] ... ";
    
    TextureDesc desc;
    desc.width = 512;
    desc.height = 512;
    desc.format = TextureFormat::RGBA8_UNORM;
    desc.usage = TextureUsage::SHADER_READ;
    
    assert(desc.width == 512);
    assert(desc.height == 512);
    assert(desc.format == TextureFormat::RGBA8_UNORM);
    
    std::cout << "PASS" << std::endl;
}

void test_gpu_device_selection() {
    std::cout << "  [test_gpu_device_selection] ... ";
    
    // Test that we can create a null device
    auto device = create_null_gpu_device();
    assert(device != nullptr);
    assert(device->get_adapter_name() == "Null GPU");
    assert(device->is_ray_tracing_supported() == false);
    
    std::cout << "PASS" << std::endl;
}

// =============================================================================
// Scene Graph Tests
// =============================================================================

void test_scene_basic() {
    std::cout << "  [test_scene_basic] ... ";
    
    Scene scene;
    
    // Create nodes
    SceneNode* root = scene.create_node("root");
    SceneNode* child = scene.create_node("child");
    
    assert(root != nullptr);
    assert(child != nullptr);
    
    // Test hierarchy
    child->set_parent(root);
    assert(root->get_children().size() == 1);
    assert(child->get_parent() == root);
    
    // Test transforms
    root->set_position(Vec3(0, 0, 0));
    root->set_scale(Vec3(1, 1, 1));
    
    child->set_position(Vec3(1, 0, 0));
    child->rotate(Quat(0, 0, 0, 1)); // Identity rotation
    
    // Test matrix updates
    root->update_matrix_hierarchy();
    child->update_matrix_hierarchy();
    
    // World position should be local position for root
    assert(root->get_world_position().x == 0);
    assert(root->get_world_position().y == 0);
    assert(root->get_world_position().z == 0);
    
    // Child world position should include parent's offset
    // (child is at (1,0,0) local, parent at (0,0,0) world)
    assert(child->get_world_position().x == 1);
    assert(child->get_world_position().y == 0);
    assert(child->get_world_position().z == 0);
    
    std::cout << "PASS" << std::endl;
}

void test_scene_find_by_name() {
    std::cout << "  [test_scene_find_by_name] ... ";
    
    Scene scene;
    
    SceneNode* root = scene.create_node("root");
    SceneNode* child = scene.create_node("child");
    SceneNode* grandchild = scene.create_node("grandchild");
    
    child->set_parent(root);
    grandchild->set_parent(child);
    
    // Test find by name
    assert(scene.find_by_name("root") == root);
    assert(scene.find_by_name("child") == child);
    assert(scene.find_by_name("grandchild") == grandchild);
    
    // Test not found
    assert(scene.find_by_name("nonexistent") == nullptr);
    
    std::cout << "PASS" << std::endl;
}

void test_scene_remove_node() {
    std::cout << "  [test_scene_remove_node] ... ";
    
    Scene scene;
    
    SceneNode* root = scene.create_node("root");
    SceneNode* child = scene.create_node("child");
    
    child->set_parent(root);
    
    assert(root->get_children().size() == 1);
    
    // Remove child
    root->remove_child(child);
    
    assert(root->get_children().size() == 0);
    assert(child->get_parent() == nullptr);
    
    std::cout << "PASS" << std::endl;
}

void test_scene_hierarchy_update() {
    std::cout << "  [test_scene_hierarchy_update] ... ";
    
    Scene scene;
    
    SceneNode* root = scene.create_node("root");
    SceneNode* child = scene.create_node("child");
    SceneNode* grandchild = scene.create_node("grandchild");
    
    child->set_parent(root);
    grandchild->set_parent(child);
    
    // Set positions
    root->set_position(Vec3(0, 0, 0));
    child->set_position(Vec3(1, 0, 0));
    grandchild->set_position(Vec3(0, 1, 0));
    
    // Update hierarchy
    scene.update_transforms();
    
    // Verify positions
    assert(root->get_world_position().x == 0);
    assert(root->get_world_position().y == 0);
    assert(root->get_world_position().z == 0);
    
    // Child world position = root position + child local position
    assert(child->get_world_position().x == 1);
    assert(child->get_world_position().y == 0);
    assert(child->get_world_position().z == 0);
    
    // Grandchild world position = root + child + grandchild
    assert(grandchild->get_world_position().x == 1);
    assert(grandchild->get_world_position().y == 1);
    assert(grandchild->get_world_position().z == 0);
    
    std::cout << "PASS" << std::endl;
}

void test_scene_save_load() {
    std::cout << "  [test_scene_save_load] ... ";
    
    Scene scene;
    
    SceneNode* root = scene.create_node("root");
    root->set_position(Vec3(0, 0, 0));
    root->set_scale(Vec3(2, 2, 2));
    
    SceneNode* child = scene.create_node("child");
    child->set_position(Vec3(1, 1, 1));
    child->set_rotation(Quat(1, 0, 0, 0)); // 90 degree rotation around X
    child->set_parent(root);
    
    // Save to temporary file
    std::string filepath = "C:/Users/roika/AppData/Local/Temp/test_scene.json";
    scene.save(filepath);
    
    // Create new scene and load
    Scene loaded_scene;
    loaded_scene.load(filepath);
    
    // Verify loaded nodes
    SceneNode* loaded_root = loaded_scene.find_by_name("root");
    assert(loaded_root != nullptr);
    
    SceneNode* loaded_child = loaded_scene.find_by_name("child");
    assert(loaded_child != nullptr);
    
    // Verify transforms
    assert(loaded_root->get_local_position().x == 0);
    assert(loaded_child->get_local_position().x == 1);
    assert(loaded_child->get_local_position().y == 1);
    assert(loaded_child->get_local_position().z == 1);
    
    std::cout << "PASS" << std::endl;
}

void test_scene_utils() {
    std::cout << "  [test_scene_utils] ... ";
    
    Scene scene;
    
    SceneNode* root = scene.create_node("root");
    SceneNode* child = scene.create_node("child");
    child->set_parent(root);
    
    // Test distance_to_camera
    Vec3 camera_pos(5, 0, 0);
    float dist = SceneUtils::distance_to_camera(root, camera_pos);
    assert(dist == 5.0f);
    
    // Test find_closest
    root->set_position(Vec3(0, 0, 0));
    child->set_position(Vec3(3, 0, 0));
    
    SceneNode* closest = SceneUtils::find_closest(root, Vec3(1, 0, 0), 100.0f);
    assert(closest == root); // Root is closer
    
    std::cout << "PASS" << std::endl;
}

// =============================================================================
// Main Test Runner
// =============================================================================

int main() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "Phase 1 Tests - Core Infrastructure" << std::endl;
    std::cout << "========================================" << std::endl;
    
    // Memory tests
    std::cout << "\n[Memory System]" << std::endl;
    test_object_pool_basic();
    test_object_pool_overflow();
    test_aligned_allocator();
    test_memory_pool();
    
    // Event tests
    std::cout << "\n[Event System]" << std::endl;
    test_event_dispatcher_basic();
    test_event_dispatcher_multiple();
    test_subscriber_count();
    
    // GPU tests
    std::cout << "\n[GPU Abstraction]" << std::endl;
    test_gpu_buffer_creation();
    test_gpu_texture_creation();
    test_gpu_device_selection();
    
    // Scene tests
    std::cout << "\n[Scene System]" << std::endl;
    test_scene_basic();
    test_scene_find_by_name();
    test_scene_remove_node();
    test_scene_hierarchy_update();
    test_scene_save_load();
    test_scene_utils();
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "All Phase 1 tests passed!" << std::endl;
    std::cout << "========================================" << std::endl;
    
    return 0;
}
