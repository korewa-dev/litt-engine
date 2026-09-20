// Litt Engine asset pipeline
#include "litt_asset_pipeline.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace litt {
namespace {
namespace fs = std::filesystem;

std::string fnv1a_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    uint64_t hash = 1469598103934665603ull;
    char buffer[64 * 1024];
    while (in) {
        in.read(buffer, sizeof(buffer));
        const std::streamsize count = in.gcount();
        for (std::streamsize i = 0; i < count; ++i) {
            hash ^= static_cast<unsigned char>(buffer[i]);
            hash *= 1099511628211ull;
        }
    }
    std::ostringstream out;
    out << std::hex << std::setfill('0') << std::setw(16) << hash;
    return out.str();
}

uint64_t modified_time(const fs::path& path) {
    std::error_code ec;
    const auto value = fs::last_write_time(path, ec);
    if (ec) return 0;
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(value.time_since_epoch()).count());
}
} // namespace

bool Asset::load(const std::string& path) {
    std::error_code ec;
    if (path.empty() || !fs::is_regular_file(path, ec) || ec) {
        loaded_ = false;
        handle_.loaded = false;
        path_.clear();
        return false;
    }
    path_ = path;
    loaded_ = true;
    handle_.loaded = true;
    handle_.type = get_type();
    return true;
}

void Asset::unload() {
    loaded_ = false;
    handle_.loaded = false;
    path_.clear();
}

void AssetFactory::register_creator(
    AssetType type, std::function<std::unique_ptr<Asset>()> creator) {
    if (creator) creators_[type] = std::move(creator);
}

std::unique_ptr<Asset> AssetFactory::create(AssetType type) {
    const auto it = creators_.find(type);
    return it == creators_.end() ? nullptr : it->second();
}

Asset* AssetFactory::load_asset(const std::string& path, AssetType type) {
    auto asset = create(type);
    if (!asset || !asset->load(path)) return nullptr;

    const uint32_t id = next_id_++;
    asset->handle_.id = id;
    asset->handle_.type = type;
    asset->handle_.loaded = true;
    Asset* result = asset.get();
    assets_.emplace(id, std::move(asset));
    return result;
}

Asset* AssetFactory::get_asset(const AssetHandle& handle) {
    if (handle.id == 0) return nullptr;
    const auto it = assets_.find(handle.id);
    if (it == assets_.end() || it->second->get_type() != handle.type) return nullptr;
    return it->second.get();
}

void AssetFactory::unload_asset(const AssetHandle& handle) {
    const auto it = assets_.find(handle.id);
    if (it == assets_.end()) return;
    it->second->unload();
    assets_.erase(it);
}

void AssetFactory::unload_all() {
    for (auto& entry : assets_) entry.second->unload();
    assets_.clear();
}

AssetHandle AssetPipeline::import(const std::string& path, AssetType type) {
    return import_with_options(path, type, {});
}

AssetHandle AssetPipeline::import_with_options(
    const std::string& path, AssetType type, const std::string& options) {
    (void)options;
    Asset* asset = AssetFactory::get_instance().load_asset(path, type);
    if (!asset) return {};
    const AssetHandle handle = asset->get_handle();
    if (async_loading_) import_queue_.push_back(handle);
    return handle;
}

bool AssetPipeline::reimport(const AssetHandle& handle) {
    Asset* current = AssetFactory::get_instance().get_asset(handle);
    if (!current) return false;
    const std::string path = current->get_path();
    const AssetType type = current->get_type();
    AssetFactory::get_instance().unload_asset(handle);
    return AssetFactory::get_instance().load_asset(path, type) != nullptr;
}

AssetMetadata AssetPipeline::get_metadata(const AssetHandle& handle) const {
    AssetMetadata metadata{};
    metadata.type = handle.type;
    Asset* asset = AssetFactory::get_instance().get_asset(handle);
    if (!asset) return metadata;

    metadata.path = asset->get_path();
    const fs::path path(metadata.path);
    metadata.name = path.filename().string();

    std::error_code ec;
    metadata.size = fs::file_size(path, ec);
    if (ec) metadata.size = 0;
    metadata.last_modified = modified_time(path);
    metadata.hash = fnv1a_file(metadata.path);
    return metadata;
}

std::vector<AssetHandle> AssetPipeline::get_assets_of_type(AssetType type) const {
    return AssetFactory::get_instance().get_handles(type);
}

void AssetPipeline::process_queue() {
    import_queue_.erase(
        std::remove_if(import_queue_.begin(), import_queue_.end(),
            [](const AssetHandle& handle) {
                Asset* asset = AssetFactory::get_instance().get_asset(handle);
                return !asset || asset->is_loaded();
            }),
        import_queue_.end());
}

} // namespace litt
