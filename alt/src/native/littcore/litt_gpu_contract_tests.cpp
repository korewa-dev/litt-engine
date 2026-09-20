#include "litt_gpu.h"
#include "litt_gpu_software.h"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <stdexcept>

using namespace litt;

int main() {
    const auto vk = gpu_backend_capability("vulkan");
    assert(vk.support == GPUBackendSupport::Unavailable);
    assert(vk.hardware_accelerated);
    assert(!vk.presentation);

    const auto sw_cap = gpu_backend_capability("software");
    assert(sw_cap.support == GPUBackendSupport::Partial);
    assert(!sw_cap.hardware_accelerated);

    bool unavailable_failed = false;
    try {
        (void)create_gpu_device("vulkan");
    } catch (const std::runtime_error&) {
        unavailable_failed = true;
    }
    assert(unavailable_failed);

    bool unknown_failed = false;
    try {
        (void)create_gpu_device("definitely-not-a-backend");
    } catch (const std::invalid_argument&) {
        unknown_failed = true;
    }
    assert(unknown_failed);

    auto null_device = create_gpu_device("null");
    assert(null_device && null_device->initialize("headless"));
    BufferDesc null_desc{};
    null_desc.size = 64;
    null_desc.usage = GPUBufferUsage::INDEX;
    auto null_buffer = null_device->create_buffer(null_desc);
    assert(null_buffer);
    assert(null_buffer->get_size() == 64);
    assert(null_buffer->get_type() == GPUBufferUsage::INDEX);
    assert(null_buffer->map() == nullptr);
    null_device->shutdown();

    auto device = create_gpu_device("software");
    assert(device && device->initialize("headless"));

    BufferDesc desc{};
    desc.size = sizeof(uint32_t) * 4;
    desc.usage = GPUBufferUsage::VERTEX;
    const uint32_t initial[4] = {1, 2, 3, 4};
    desc.data = initial;
    auto buffer = device->create_buffer(desc);
    assert(buffer && buffer->get_size() == sizeof(initial));
    assert(buffer->get_type() == GPUBufferUsage::VERTEX);
    assert(std::memcmp(buffer->map(), initial, sizeof(initial)) == 0);
    buffer->unmap();

    const uint32_t replacement[2] = {9, 8};
    device->update_buffer(buffer.get(), replacement, sizeof(replacement));
    assert(std::memcmp(buffer->map(), replacement, sizeof(replacement)) == 0);
    buffer->unmap();

    TextureDesc rgba{};
    rgba.width = 4;
    rgba.height = 4;
    rgba.format = TextureFormat::RGBA8;
    auto texture = device->create_texture(rgba);
    assert(texture && texture->get_width() == 4 && texture->get_height() == 4);

    TextureDesc unsupported = rgba;
    unsupported.format = TextureFormat::RGBA16F;
    assert(!device->create_texture(unsupported));

    auto* software = dynamic_cast<SoftwareRenderer*>(device.get());
    assert(software);
    software->clear(0x00112233u);
    software->draw_triangle(10, 10, 100, 10, 10, 100, 0x00ff0000u);
    assert(software->get_pixel(20, 20) == 0x00ff0000u);

    for (int frame = 0; frame < 1000; ++frame) {
        software->clear(static_cast<uint32_t>(frame) & 0x00ffffffu);
        software->draw_triangle(20, 20, 120, 20, 20, 120, 0x0000ff00u);
        device->present();
    }
    assert(software->get_pixel(30, 30) == 0x0000ff00u);

    device->shutdown();
    return 0;
}
