#include "litt_asset_pipeline.h"
#include "litt_asset.h"
#include "litt_pbr_material.h"
#include "litt_renderer.h"

#include <cstdio>
#include <fstream>
#include <memory>
#include <vector>
#include <cstdint>

using namespace litt;

namespace {
class TestMeshAsset final : public Asset {
public:
    AssetType get_type() const override { return AssetType::MESH; }
};

int failures = 0;
void check(bool condition, const char* name) {
    std::printf("%s %s\n", condition ? "[PASS]" : "[FAIL]", name);
    if (!condition) ++failures;
}
}

static void write_test_tga(const char* path) {
    const uint8_t header[18] = {
        0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        2, 0, 1, 0, 24, 0x20
    };
    const uint8_t pixels[6] = {
        0, 0, 255,
        0, 255, 0
    };
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(header), sizeof(header));
    out.write(reinterpret_cast<const char*>(pixels), sizeof(pixels));
}

int main() {
    const char* path = "asset_pipeline_test.tmp";
    { std::ofstream out(path, std::ios::binary); out << "litt-asset"; }

    auto& factory = AssetFactory::get_instance();
    factory.unload_all();
    factory.register_creator(AssetType::MESH, [] { return std::make_unique<TestMeshAsset>(); });

    auto& pipeline = AssetPipeline::get_instance();
    pipeline.set_async_loading(false);

    const AssetHandle handle = pipeline.import(path, AssetType::MESH);
    check(handle.is_valid(), "import creates a valid handle");
    check(factory.get_asset(handle) != nullptr, "handle resolves to owned asset");
    AssetHandle pendingIdentity = handle;
    pendingIdentity.loaded = false;
    check(pendingIdentity.is_valid() && !pendingIdentity.is_loaded(),
          "handle identity is independent from load state");

    const AssetMetadata metadata = pipeline.get_metadata(handle);
    check(metadata.name == path, "metadata reports filename");
    check(metadata.size == 10, "metadata reports file size");
    check(metadata.hash.size() == 16, "metadata has deterministic content hash");

    const auto meshes = pipeline.get_assets_of_type(AssetType::MESH);
    check(meshes.size() == 1 && meshes[0].id == handle.id, "typed asset enumeration");

    check(pipeline.reimport(handle), "reimport succeeds");
    check(factory.get_asset(handle)->get_handle().id == handle.id,
          "reimport keeps asset identity");
    check(factory.get_asset(handle) != nullptr, "reimport preserves stable handle");

    const AssetHandle missing = pipeline.import("does-not-exist.asset", AssetType::MESH);
    check(!missing.is_valid(), "missing file import fails cleanly");

    auto metal = MaterialFactory::create_metallic(Vec3(0.2f, 0.3f, 0.4f), 0.25f);
    check(metal && metal->metallic == 1.0f && metal->roughness == 0.25f, "metallic material factory");

    auto glow = MaterialFactory::create_emissive(Vec3(1.0f, 0.5f, 0.25f), 2.0f);
    check(glow && glow->emission.x == 2.0f && glow->emission.y == 1.0f, "emissive material factory");

    PBRMaterial pbr;
    pbr.albedo = Vec3(0.3f, 0.4f, 0.5f);
    pbr.metallic = 1.5f;
    pbr.roughness = -1.0f;
    pbr.opacity = 0.5f;
    pbr.emission = Vec3(1.0f, 0.0f, 0.0f);
    const RenderMaterial rendered = RenderMaterial::from_pbr(pbr, "contract");
    check(rendered.name == "contract" && rendered.metalness == 1.0f &&
          rendered.roughness == 0.0f && rendered.transparent &&
          rendered.type == MaterialType::Transparent,
          "pbr_to_render_material_contract");

    const char* obj_path = "asset_manager_test.obj";
    {
        std::ofstream out(obj_path, std::ios::binary | std::ios::trunc);
        out << "v 0 0 0\n"
               "v 1 0 0\n"
               "v 0 1 0\n"
               "vt 0 0\n"
               "vt 1 0\n"
               "vt 0 1\n"
               "vn 0 0 1\n"
               "f 1/1/1 2/2/1 3/3/1\n";
    }
    AssetManager manager;
    const auto model = manager.loadModel(obj_path);
    check(model && model->vertices.size() == 3 && model->indices.size() == 3,
          "asset_manager_obj_load");
    check(manager.loadModel(obj_path) == model, "asset_manager_obj_cache");
    check(!manager.loadModel("asset_manager_test.txt"), "asset_manager_rejects_non_obj");

    const char* bad_obj = "asset_manager_bad.obj";
    {
        std::ofstream out(bad_obj, std::ios::binary | std::ios::trunc);
        out << "v nan 0 0\n"
               "v 1 0 0\n"
               "v 0 1 0\n"
               "f 1 2 3\n";
    }
    check(!manager.loadModel(bad_obj), "asset_manager_rejects_nonfinite_obj");

    const char* tga_path = "asset_manager_test.tga";
    write_test_tga(tga_path);
    const auto texture = manager.loadTexture(tga_path);
    check(texture && texture->width == 2 && texture->height == 1 &&
          texture->channels == 3 && texture->data.size() == 6,
          "asset_manager_tga_load");
    check(texture && texture->data[0] == 255 && texture->data[1] == 0 &&
          texture->data[2] == 0, "asset_manager_tga_bgr_to_rgb");

    const char* bad_tga = "asset_manager_bad.tga";
    { std::ofstream out(bad_tga, std::ios::binary | std::ios::trunc); out << "TGA"; }
    check(!manager.loadTexture(bad_tga), "asset_manager_rejects_truncated_tga");
    check(!manager.loadShader("missing.vert", "missing.frag"),
          "asset_manager_shader_stub_fails_explicitly");

    factory.unload_all();
    std::remove(path);
    std::remove("asset_manager_test.obj");
    std::remove("asset_manager_bad.obj");
    std::remove("asset_manager_test.tga");
    std::remove("asset_manager_bad.tga");
    return failures == 0 ? 0 : 1;
}
