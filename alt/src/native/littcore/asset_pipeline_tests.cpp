#include "litt_asset_pipeline.h"
#include "litt_pbr_material.h"

#include <cstdio>
#include <fstream>
#include <memory>

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

    factory.unload_all();
    std::remove(path);
    return failures == 0 ? 0 : 1;
}
