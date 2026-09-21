#include "litt_gpu.h"
#include "litt_gpu_software.h"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <stdexcept>

using namespace litt;

static void assert_unavailable_backend_contract(const char* backend_name) {
    const auto capability = gpu_backend_capability(backend_name);
    assert(capability.name != nullptr);
    assert(std::strcmp(capability.name, backend_name) == 0);
    assert(capability.support == GPUBackendSupport::Unavailable);
    assert(capability.hardware_accelerated);
    assert(!capability.presentation);

    bool refused = false;
    try {
        auto device = create_gpu_device(backend_name);
        assert(device);
        refused = !device->initialize("contract-probe");
        device->shutdown();
    } catch (const std::runtime_error&) {
        refused = true;
    }
    assert(refused);
}

int main() {
    assert_unavailable_backend_contract("vulkan");
    assert_unavailable_backend_contract("dx12");
    assert_unavailable_backend_contract("opengl");
    assert_unavailable_backend_contract("metal");

    for (const char* backend : {"software", "vulkan", "dx12", "opengl", "metal"}) {
        const auto rt = gpu_ray_tracing_capability(backend);
        assert(rt.backend != nullptr);
        assert(std::strcmp(rt.backend, backend) == 0);
        assert(rt.support == GPUBackendSupport::Unavailable);
        assert(rt.reason != nullptr && rt.reason[0] != '\0');
    }
    const auto unknown_rt = gpu_ray_tracing_capability("definitely-not-a-backend");
    assert(unknown_rt.backend == nullptr);
    assert(unknown_rt.support == GPUBackendSupport::Unavailable);

    const auto sw_cap = gpu_backend_capability("software");
    assert(sw_cap.support == GPUBackendSupport::Supported);
    assert(!sw_cap.hardware_accelerated);

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

    BufferDesc empty_desc{};
    assert(!device->create_buffer(empty_desc));
    BufferDesc huge_desc{};
    huge_desc.size = software_detail::MAX_BUFFER_BYTES + 1u;
    assert(!device->create_buffer(huge_desc));

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

    TextureDesc zero_texture = rgba;
    zero_texture.width = 0;
    assert(!device->create_texture(zero_texture));
    TextureDesc huge_texture = rgba;
    huge_texture.width = 65535;
    huge_texture.height = 65535;
    assert(!device->create_texture(huge_texture));

    auto* software = dynamic_cast<SoftwareRenderer*>(device.get());
    assert(software);
    software->clear(0x00112233u);
    assert(software->get_pixel(-1, 0) == 0 && software->get_pixel(800, 0) == 0);
    software->draw_triangle(10, 10, 100, 10, 10, 100, 0x00ff0000u);
    assert(software->get_pixel(20, 20) == 0x00ff0000u);

    software->clear(0);
    software->draw_pixel_depth(25, 25, 0.75f, 0x000000ffu);
    software->draw_pixel_depth(25, 25, 0.25f, 0x00ff0000u);
    software->draw_pixel_depth(25, 25, 0.5f, 0x0000ff00u);
    assert(software->get_pixel(25, 25) == 0x00ff0000u);

    const uint32_t before_bad_mesh = software->get_pixel(300, 300);
    software->draw_mesh({Vec3(0, 0, 0)}, {0, 1, 2}, Mat4::identity(), 0x00ffffffu);
    software->draw_grid(0.0f, Mat4::identity(), 0x00ffffffu);
    software->draw_grid(std::nanf(""), Mat4::identity(), 0x00ffffffu);
    assert(software->get_pixel(300, 300) == before_bad_mesh);

    for (int frame = 0; frame < 1000; ++frame) {
        software->clear(static_cast<uint32_t>(frame) & 0x00ffffffu);
        software->draw_triangle(20, 20, 120, 20, 20, 120, 0x0000ff00u);
        device->present();
    }
    assert(software->get_pixel(30, 30) == 0x0000ff00u);

    device->shutdown();
    return 0;
}
