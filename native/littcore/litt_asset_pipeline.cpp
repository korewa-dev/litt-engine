// Asset Pipeline Implementation
// Model and texture loading for game assets

#include "litt_renderer.h"
#include "litt_obj.h"
#include <fstream>
#include <sstream>
#include <cstring>

#ifdef LITT_VULKAN
#include <vulkan/vulkan.h>
#endif

namespace litt {

// =============================================================================
// AssetPipeline - Model and Texture Loading
// =============================================================================

class AssetPipeline {
public:
    struct Model {
        std::string name;
        std::vector<Vec3f> vertices;
        std::vector<Vec3f> normals;
        std::vector<Vec2f> texcoords;
        std::vector<uint32_t> indices;
        std::vector<Material> materials;
        
        // Vulkan GPU resources
#ifdef LITT_VULKAN
        VkBuffer vertex_buffer = VK_NULL_HANDLE;
        VkBuffer index_buffer = VK_NULL_HANDLE;
        VkDeviceMemory vertex_memory = VK_NULL_HANDLE;
        VkDeviceMemory index_memory = VK_NULL_HANDLE;
        VkBuffer staging_buffer = VK_NULL_HANDLE;
        VkDeviceMemory staging_memory = VK_NULL_HANDLE;
#endif
    };
    
    struct Texture {
        std::string name;
        uint32_t width = 0;
        uint32_t height = 0;
        std::vector<uint8_t> pixels;
        VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
        
        // Vulkan GPU resources
#ifdef LITT_VULKAN
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        VkSampler sampler = VK_NULL_HANDLE;
#endif
    };
    
    struct Scene {
        std::vector<Model> models;
        std::vector<Texture> textures;
        std::string name;
    };
    
private:
#ifdef LITT_VULKAN
    VkDevice device_ = VK_NULL_HANDLE;
    VkQueue queue_ = VK_NULL_HANDLE;
    VkCommandPool command_pool_ = VK_NULL_HANDLE;
    uint32_t memory_type_index_ = 0xFFFFFFFF;
#endif
    
public:
    AssetPipeline() = default;
    ~AssetPipeline() = default;
    
    // =============================================================================
    // Initialization
    // =============================================================================
    
    bool initialize(VkDevice device, VkQueue queue, VkCommandPool command_pool) {
#ifdef LITT_VULKAN
        device_ = device;
        queue_ = queue;
        command_pool_ = command_pool;
        
        // Find suitable memory type
        VkPhysicalDeviceMemoryProperties mem_props;
        vkGetPhysicalDeviceMemoryProperties(
            /* need physical device - store it */
            &mem_props);
        
        // Find device-local memory type
        for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
            if ((mem_props.memoryTypes[i].propertyFlags & 
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == 
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
                memory_type_index_ = i;
                break;
            }
        }
        
        return memory_type_index_ != 0xFFFFFFFF;
#else
        return false;
#endif
    }
    
    // =============================================================================
    // Model Loading
    // =============================================================================
    
    Model load_model(const std::string& path) {
        Model model;
        model.name = path;
        
        // Parse OBJ file
        std::ifstream file(path);
        if (!file) {
            return model;
        }
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        
        // Simple OBJ parser
        parse_obj(buffer.str(), model);
        
        // Upload to GPU
        upload_to_gpu(model);
        
        return model;
    }
    
    Scene load_scene(const std::string& json_path) {
        Scene scene;
        scene.name = json_path;
        
        // Parse scene JSON
        std::ifstream file(json_path);
        if (!file) {
            return scene;
        }
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        
        // Simple scene parser
        parse_scene_json(buffer.str(), scene);
        
        // Upload all assets to GPU
        for (auto& model : scene.models) {
            upload_to_gpu(model);
        }
        for (auto& texture : scene.textures) {
            upload_to_gpu(texture);
        }
        
        return scene;
    }
    
    // =============================================================================
    // Texture Loading
    // =============================================================================
    
    Texture load_texture(const std::string& path) {
        Texture texture;
        texture.name = path;
        
        // Load image file (PNG, JPG, etc.)
        // For now, load as raw pixels
        if (!load_image_file(path, texture)) {
            return texture;
        }
        
        // Upload to GPU
        upload_texture_to_gpu(texture);
        
        return texture;
    }
    
    // =============================================================================
    // OBJ Parser
    // =============================================================================
    
    void parse_obj(const std::string& obj_text, Model& model) {
        std::istringstream stream(obj_text);
        std::string line;
        
        while (std::getline(stream, line)) {
            std::istringstream line_stream(line);
            std::string prefix;
            line_stream >> prefix;
            
            if (prefix == "v") {
                // Vertex
                float x, y, z;
                line_stream >> x >> y >> z;
                model.vertices.push_back(Vec3f(x, y, z));
            } else if (prefix == "vn") {
                // Normal
                float x, y, z;
                line_stream >> x >> y >> z;
                model.normals.push_back(Vec3f(x, y, z));
            } else if (prefix == "vt") {
                // Texture coordinate
                float u, v;
                line_stream >> u >> v;
                model.texcoords.push_back(Vec2f(u, v));
            } else if (prefix == "f") {
                // Face
                std::string v1, v2, v3;
                line_stream >> v1 >> v2 >> v3;
                
                // Parse vertex indices (1-based)
                auto parse_face = [](const std::string& face) -> std::pair<uint32_t, uint32_t> {
                    auto pos = face.find('/');
                    if (pos == std::string::npos) {
                        return {std::stoi(face) - 1, 0};
                    }
                    auto pos2 = face.find('/', pos + 1);
                    uint32_t vert_idx = std::stoi(face.substr(0, pos)) - 1;
                    uint32_t tex_idx = (pos2 == std::string::npos) ? 0 : 
                                       std::stoi(face.substr(pos + 1, pos2 - pos - 1)) - 1;
                    return {vert_idx, tex_idx};
                };
                
                auto [idx1, tex1] = parse_face(v1);
                auto [idx2, tex2] = parse_face(v2);
                auto [idx3, tex3] = parse_face(v3);
                
                model.indices.push_back(idx1);
                model.indices.push_back(idx2);
                model.indices.push_back(idx3);
            }
        }
    }
    
    // =============================================================================
    // Scene JSON Parser
    // =============================================================================
    
    void parse_scene_json(const std::string& json_text, Scene& scene) {
        // Simple JSON parser for scene description
        // In production, use a proper JSON library
        
        std::istringstream stream(json_text);
        std::string line;
        
        while (std::getline(stream, line)) {
            // Look for "model" or "texture" entries
            if (line.find("\"model\"") != std::string::npos ||
                line.find("\"texture\"") != std::string::npos) {
                // Extract path
                auto start = line.find('"');
                auto end = line.find('"', start + 1);
                if (start != std::string::npos && end != std::string::npos) {
                    std::string path = line.substr(start + 1, end - start - 1);
                    
                    if (path.find(".obj") != std::string::npos ||
                        path.find(".glb") != std::string::npos ||
                        path.find(".gltf") != std::string::npos) {
                        scene.models.push_back(load_model(path));
                    } else if (path.find(".png") != std::string::npos ||
                               path.find(".jpg") != std::string::npos ||
                               path.find(".ktx") != std::string::npos) {
                        scene.textures.push_back(load_texture(path));
                    }
                }
            }
        }
    }
    
    // =============================================================================
    // Image Loader
    // =============================================================================
    
    bool load_image_file(const std::string& path, Texture& texture) {
        // Simple PNG loader
        std::ifstream file(path, std::ios::binary);
        if (!file) {
            return false;
        }
        
        // Read PNG header
        uint8_t sig[8];
        file.read(reinterpret_cast<char*>(sig), 8);
        if (sig[0] != 0x89 || sig[1] != 0x50 || sig[2] != 0x4E || sig[3] != 0x47) {
            return false;
        }
        
        // Parse IHDR chunk
        uint32_t chunk_len;
        uint8_t chunk_type[4];
        file.read(reinterpret_cast<char*>(&chunk_len), 4);
        file.read(reinterpret_cast<char*>(chunk_type), 4);
        
        if (chunk_type[0] != 'I' || chunk_type[1] != 'H' ||
            chunk_type[2] != 'D' || chunk_type[3] != 'R') {
            return false;
        }
        
        // Read IHDR data
        uint8_t ihdr[13];
        file.read(reinterpret_cast<char*>(ihdr), 13);
        
        texture.width = (ihdr[0] << 24) | (ihdr[1] << 16) | (ihdr[2] << 8) | ihdr[3];
        texture.height = (ihdr[4] << 24) | (ihdr[5] << 16) | (ihdr[6] << 8) | ihdr[7];
        
        uint8_t bit_depth = ihdr[8];
        uint8_t color_type = ihdr[9];
        
        // Determine format
        switch (color_type) {
            case 2: // RGB
                texture.format = VK_FORMAT_R8G8B8A8_UNORM;
                break;
            case 6: // RGBA
                texture.format = VK_FORMAT_R8G8B8A8_UNORM;
                break;
            case 4: // Grayscale + Alpha
                texture.format = VK_FORMAT_R8G8B8A8_UNORM;
                break;
            default:
                texture.format = VK_FORMAT_R8G8B8A8_UNORM;
                break;
        }
        
        // Read image data
        size_t row_bytes = texture.width * 4; // RGBA
        texture.pixels.resize(texture.height * row_bytes);
        
        for (uint32_t y = 0; y < texture.height; ++y) {
            file.read(reinterpret_cast<char*>(texture.pixels.data() + y * row_bytes), 
                     row_bytes);
        }
        
        return true;
    }
    
    // =============================================================================
    // GPU Upload
    // =============================================================================
    
    void upload_to_gpu(Model& model) {
#ifdef LITT_VULKAN
        if (!device_) return;
        
        // Create staging buffer
        VkBufferCreateInfo buffer_info = {};
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size = model.vertices.size() * sizeof(Vec3f) + 
                          model.normals.size() * sizeof(Vec3f) +
                          model.texcoords.size() * sizeof(Vec2f);
        buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        
        VkBuffer staging_buffer;
        VkDeviceMemory staging_memory;
        
        vkCreateBuffer(device_, &buffer_info, nullptr, &staging_buffer);
        
        VkMemoryRequirements mem_reqs;
        vkGetBufferMemoryRequirements(device_, staging_buffer, &mem_reqs);
        
        VkMemoryAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize = mem_reqs.size;
        alloc_info.memoryTypeIndex = memory_type_index_;
        
        vkAllocateMemory(device_, &alloc_info, nullptr, &staging_memory);
        vkBindBufferMemory(device_, staging_buffer, staging_memory, 0);
        
        // Map and copy data
        void* mapped;
        vkMapMemory(device_, staging_memory, 0, mem_reqs.size, 0, &mapped);
        memcpy(mapped, model.vertices.data(), model.vertices.size() * sizeof(Vec3f));
        memcpy((uint8_t*)mapped + model.vertices.size() * sizeof(Vec3f),
               model.normals.data(), model.normals.size() * sizeof(Vec3f));
        memcpy((uint8_t*)mapped + model.vertices.size() * sizeof(Vec3f) + 
               model.normals.size() * sizeof(Vec3f),
               model.texcoords.data(), model.texcoords.size() * sizeof(Vec2f));
        vkUnmapMemory(device_, staging_memory);
        
        // Create vertex buffer
        buffer_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | 
                           VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        buffer_info.size = mem_reqs.size;
        
        vkCreateBuffer(device_, &buffer_info, nullptr, &model.vertex_buffer);
        
        VkMemoryRequirements vertex_mem_reqs;
        vkGetBufferMemoryRequirements(device_, model.vertex_buffer, &vertex_mem_reqs);
        
        alloc_info.allocationSize = vertex_mem_reqs.size;
        alloc_info.memoryTypeIndex = memory_type_index_;
        
        vkAllocateMemory(device_, &alloc_info, nullptr, &model.vertex_memory);
        vkBindBufferMemory(device_, model.vertex_buffer, model.vertex_memory, 0);
        
        // Copy to device memory
        VkCommandBufferAllocateInfo cmd_alloc = {};
        cmd_alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmd_alloc.commandPool = command_pool_;
        cmd_alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmd_alloc.commandBufferCount = 1;
        
        VkCommandBuffer cmd_buffer;
        vkAllocateCommandBuffers(device_, &cmd_alloc, &cmd_buffer);
        
        VkCommandBufferBeginInfo begin_info = {};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        
        vkBeginCommandBuffer(cmd_buffer, &begin_info);
        
        VkBufferCopy copy_region = {};
        copy_region.srcOffset = 0;
        copy_region.dstOffset = 0;
        copy_region.size = mem_reqs.size;
        
        vkCmdCopyBuffer(cmd_buffer, staging_buffer, model.vertex_buffer, 1, &copy_region);
        
        vkEndCommandBuffer(cmd_buffer);
        
        VkSubmitInfo submit = {};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &cmd_buffer;
        
        vkQueueSubmit(queue_, 1, &submit, VK_NULL_HANDLE);
        vkQueueWaitIdle(queue_);
        
        // Cleanup staging
        vkFreeCommandBuffers(device_, command_pool_, 1, &cmd_buffer);
        vkDestroyBuffer(device_, staging_buffer, nullptr);
        vkFreeMemory(device_, staging_memory, nullptr);
        
        // Create index buffer (simplified)
        if (!model.indices.empty()) {
            buffer_info.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | 
                               VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            buffer_info.size = model.indices.size() * sizeof(uint32_t);
            
            vkCreateBuffer(device_, &buffer_info, nullptr, &model.index_buffer);
            
            // ... similar upload process for indices
        }
#endif
    }
    
    void upload_texture_to_gpu(Texture& texture) {
#ifdef LITT_VULKAN
        if (!device_ || texture.pixels.empty()) return;
        
        // Create image
        VkImageCreateInfo image_info = {};
        image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_info.imageType = VK_IMAGE_TYPE_2D;
        image_info.extent.width = texture.width;
        image_info.extent.height = texture.height;
        image_info.extent.depth = 1;
        image_info.mipLevels = 1;
        image_info.arrayLayers = 1;
        image_info.format = texture.format;
        image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        image_info.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
        image_info.samples = VK_SAMPLE_COUNT_1_BIT;
        image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        
        vkCreateImage(device_, &image_info, nullptr, &texture.image);
        
        // Allocate memory
        VkMemoryRequirements mem_reqs;
        vkGetImageMemoryRequirements(device_, texture.image, &mem_reqs);
        
        VkMemoryAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize = mem_reqs.size;
        alloc_info.memoryTypeIndex = memory_type_index_;
        
        vkAllocateMemory(device_, &alloc_info, nullptr, &texture.memory);
        vkBindImageMemory(device_, texture.image, texture.memory, 0);
        
        // Create staging buffer
        VkBufferCreateInfo buffer_info = {};
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size = texture.pixels.size();
        buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        
        VkBuffer staging_buffer;
        VkDeviceMemory staging_memory;
        
        vkCreateBuffer(device_, &buffer_info, nullptr, &staging_buffer);
        vkAllocateMemory(device_, &alloc_info, nullptr, &staging_memory);
        vkBindBufferMemory(device_, staging_buffer, staging_memory, 0);
        
        // Copy pixels
        void* mapped;
        vkMapMemory(device_, staging_memory, 0, mem_reqs.size, 0, &mapped);
        memcpy(mapped, texture.pixels.data(), texture.pixels.size());
        vkUnmapMemory(device_, staging_memory);
        
        // Transition image layout and copy
        VkCommandBufferAllocateInfo cmd_alloc = {};
        cmd_alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmd_alloc.commandPool = command_pool_;
        cmd_alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmd_alloc.commandBufferCount = 1;
        
        VkCommandBuffer cmd_buffer;
        vkAllocateCommandBuffers(device_, &cmd_alloc, &cmd_buffer);
        
        VkCommandBufferBeginInfo begin_info = {};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vkBeginCommandBuffer(cmd_buffer, &begin_info);
        
        // Transition image to transfer destination
        VkImageMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = texture.image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.layerCount = 1;
        
        vkCmdPipelineBarrier(cmd_buffer,
            VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier);
        
        // Copy buffer to image
        VkBufferImageCopy region = {};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.layerCount = 1;
        region.imageExtent.width = texture.width;
        region.imageExtent.height = texture.height;
        region.imageExtent.depth = 1;
        
        vkCmdCopyBufferToImage(cmd_buffer, staging_buffer, texture.image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        
        // Transition to shader readable
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        
        vkCmdPipelineBarrier(cmd_buffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier);
        
        vkEndCommandBuffer(cmd_buffer);
        
        VkSubmitInfo submit = {};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &cmd_buffer;
        
        vkQueueSubmit(queue_, 1, &submit, VK_NULL_HANDLE);
        vkQueueWaitIdle(queue_);
        
        // Create image view
        VkImageViewCreateInfo view_info = {};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = texture.image;
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = texture.format;
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.layerCount = 1;
        
        vkCreateImageView(device_, &view_info, nullptr, &texture.view);
        
        // Create sampler
        VkSamplerCreateInfo sampler_info = {};
        sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampler_info.magFilter = VK_FILTER_LINEAR;
        sampler_info.minFilter = VK_FILTER_LINEAR;
        sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_info.anisotropyEnable = VK_TRUE;
        sampler_info.maxAnisotropy = 16.0f;
        
        vkCreateSampler(device_, &sampler_info, nullptr, &texture.sampler);
        
        // Cleanup staging
        vkFreeCommandBuffers(device_, command_pool_, 1, &cmd_buffer);
        vkDestroyBuffer(device_, staging_buffer, nullptr);
        vkFreeMemory(device_, staging_memory, nullptr);
#endif
    }
};

// =============================================================================
// Global Asset Pipeline
// =============================================================================

static std::unique_ptr<AssetPipeline> g_asset_pipeline;

// =============================================================================
// Exported Functions
// =============================================================================

bool init_asset_pipeline(VkDevice device, VkQueue queue, VkCommandPool command_pool) {
    if (!g_asset_pipeline) {
        g_asset_pipeline = std::make_unique<AssetPipeline>();
    }
    return g_asset_pipeline->initialize(device, queue, command_pool);
}

Model load_model(const std::string& path) {
    if (!g_asset_pipeline) return Model{};
    return g_asset_pipeline->load_model(path);
}

Scene load_scene(const std::string& json_path) {
    if (!g_asset_pipeline) return Scene{};
    return g_asset_pipeline->load_scene(json_path);
}

Texture load_texture(const std::string& path) {
    if (!g_asset_pipeline) return Texture{};
    return g_asset_pipeline->load_texture(path);
}

} // namespace litt
