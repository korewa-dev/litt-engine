// Phase 5: Production Systems - Asset Pipeline

#pragma once

#include "litt_math.h"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

namespace litt {

// Asset types
enum class AssetType {
    TEXTURE,
    MESH,
    SHADER,
    MATERIAL,
    AUDIO,
    SCRIPT,
    SCENE,
    PREFAB
};

// Asset handle
struct AssetHandle {
    uint32_t id = 0;
    AssetType type = AssetType::TEXTURE;
    bool loaded = false;
    
    bool is_valid() const { return id != 0 && loaded; }
};

// Asset metadata
struct AssetMetadata {
    std::string path;
    std::string name;
    AssetType type;
    uint64_t size;
    uint64_t last_modified;
    std::string hash;
};

// Asset base class
class Asset {
public:
    virtual ~Asset() = default;
    
    // Get asset type
    virtual AssetType get_type() const = 0;
    
    // Get asset handle
    const AssetHandle& get_handle() const { return handle_; }
    
    // Load from file
    bool load(const std::string& path);
    
    // Unload
    void unload();
    
    // Check if loaded
    bool is_loaded() const { return loaded_; }
    
    // Get path
    const std::string& get_path() const { return path_; }

protected:
    AssetHandle handle_;
    std::string path_;
    bool loaded_ = false;
};

// Asset factory
class AssetFactory {
public:
    static AssetFactory& get_instance() {
        static AssetFactory instance;
        return instance;
    }
    
    // Register asset creator
    void register_creator(AssetType type, std::function<std::unique_ptr<Asset>()> creator);
    
    // Create asset
    std::unique_ptr<Asset> create(AssetType type);
    
    // Load asset from file
    Asset* load_asset(const std::string& path, AssetType type);
    
    // Get asset by handle
    Asset* get_asset(const AssetHandle& handle);
    
    // Unload asset
    void unload_asset(const AssetHandle& handle);
    
    // Unload all assets
    void unload_all();
    
    // Get asset count
    size_t get_asset_count() const { return assets_.size(); }

private:
    AssetFactory() = default;
    std::unordered_map<AssetType, std::function<std::unique_ptr<Asset>()>> creators_;
    std::unordered_map<uint32_t, std::unique_ptr<Asset>> assets_;
    uint32_t next_id_ = 1;
};

// Asset pipeline
class AssetPipeline {
public:
    static AssetPipeline& get_instance() {
        static AssetPipeline instance;
        return instance;
    }
    
    // Import asset
    AssetHandle import(const std::string& path, AssetType type);
    
    // Import with options
    AssetHandle import_with_options(const std::string& path, AssetType type, 
                                    const std::string& options);
    
    // Reimport asset
    bool reimport(const AssetHandle& handle);
    
    // Get metadata
    AssetMetadata get_metadata(const AssetHandle& handle) const;
    
    // Get all assets of type
    std::vector<AssetHandle> get_assets_of_type(AssetType type) const;
    
    // Process asset queue
    void process_queue();
    
    // Set async loading
    void set_async_loading(bool enabled) { async_loading_ = enabled; }
    bool is_async_loading() const { return async_loading_; }

private:
    AssetPipeline() = default;
    bool async_loading_ = true;
    std::vector<AssetHandle> import_queue_;
};

} // namespace litt
