#include "litt_engine.h"
#include "litt_event.h"
#include "litt_gpu_software.h"
#include "litt_serialization.h"

#include <cassert>
#include <cstdint>
#include <random>
#include <set>
#include <string>
#include <thread>
#include <vector>

using namespace litt;

static void scene_property_contract() {
    for (uint32_t seed = 0; seed < 64; ++seed) {
        Scene scene;
        std::mt19937 rng(seed);
        const uint32_t count = 1u + (rng() % 32u);
        for (uint32_t i = 0; i < count; ++i) {
            SceneNode& node = scene.createNode("node_" + std::to_string(seed) + "_" + std::to_string(i));
            node.position = Vec3{
                static_cast<float>(static_cast<int>(rng() % 200u) - 100) / 10.0f,
                static_cast<float>(rng() % 50u) / 10.0f,
                static_cast<float>(static_cast<int>(rng() % 200u) - 100) / 10.0f};
            node.visible = (rng() & 1u) != 0;
            node.cullable = (rng() & 1u) != 0;
            assert(scene.setParent(node.id, scene.root->id));
        }
        scene.update();
        const std::string first = scene.serializeToJson();
        assert(!first.empty());

        Scene restored;
        assert(restored.deserializeFromJson(first));
        const std::string second = restored.serializeToJson();
        assert(first == second);
    }
}

static void malformed_scene_fuzz_smoke() {
    Scene stable;
    stable.createNode("sentinel");
    const std::string baseline = stable.serializeToJson();

    std::mt19937 rng(0x51A7u);
    for (size_t iteration = 0; iteration < 2000u; ++iteration) {
        const size_t len = rng() % 512u;
        std::string input;
        input.reserve(len);
        for (size_t i = 0; i < len; ++i) {
            input.push_back(static_cast<char>(32u + (rng() % 95u)));
        }

        const bool accepted = stable.deserializeFromJson(input);
        if (!accepted) {
            assert(stable.serializeToJson() == baseline);
        } else {
            const std::string valid = stable.serializeToJson();
            assert(!valid.empty());
            assert(stable.deserializeFromJson(baseline));
        }
    }
}

static void allocation_and_overflow_contract() {
    SoftwareRenderer renderer;
    assert(!renderer.set_framebuffer_size(65535u, 65535u));

    auto device = create_gpu_device("software");
    assert(device && device->initialize("hardening"));

    BufferDesc huge_buffer{};
    huge_buffer.size = software_detail::MAX_BUFFER_BYTES + 1u;
    assert(!device->create_buffer(huge_buffer));

    TextureDesc huge_texture{};
    huge_texture.width = 65535u;
    huge_texture.height = 65535u;
    huge_texture.format = TextureFormat::RGBA8;
    assert(!device->create_texture(huge_texture));
    device->shutdown();

    bool name_limit = false;
    try {
        Scene scene;
        scene.createNode(std::string(Scene::MAX_NODE_NAME_BYTES + 1u, 'x'));
    } catch (const std::length_error&) {
        name_limit = true;
    }
    assert(name_limit);

    BinarySerializer binary;
    std::vector<uint8_t> too_large(serialization_detail::kMaxBinaryBytes + 1u, 0u);
    assert(!binary.deserialize_from_buffer(too_large));
}

static void lifecycle_soak() {
    for (int i = 0; i < 250; ++i) {
        Engine engine;
        EngineConfig config;
        config.headless = true;
        config.target_fps = 120.0f;
        assert(engine.initialize(config));
        assert(engine.is_running());
        assert(engine.scene_manager().getActiveScene() != nullptr);
        engine.shutdown();
        assert(!engine.is_running());
    }
}

static void concurrent_event_queue_contract() {
    EventQueue<uint32_t> queue;
    constexpr uint32_t producers = 4u;
    constexpr uint32_t per_producer = 5000u;

    std::vector<std::thread> threads;
    for (uint32_t producer = 0; producer < producers; ++producer) {
        threads.emplace_back([producer, &queue] {
            const uint32_t base = producer * per_producer;
            for (uint32_t i = 0; i < per_producer; ++i) {
                queue.push(base + i);
            }
        });
    }
    for (auto& thread : threads) thread.join();

    assert(queue.size() == static_cast<size_t>(producers) * per_producer);
    std::vector<bool> seen(static_cast<size_t>(producers) * per_producer, false);
    while (!queue.empty()) {
        const uint32_t value = queue.pop();
        assert(value < seen.size());
        assert(!seen[value]);
        seen[value] = true;
    }
    for (bool value : seen) assert(value);
}

int main() {
    scene_property_contract();
    malformed_scene_fuzz_smoke();
    allocation_and_overflow_contract();
    lifecycle_soak();
    concurrent_event_queue_contract();
    return 0;
}
