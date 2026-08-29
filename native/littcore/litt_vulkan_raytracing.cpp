// Vulkan Ray Tracing Implementation
// BLAS/TLAS building and ray tracing pipeline

#include "litt_renderer.h"
#include <cstring>
#include <algorithm>

#ifdef LITT_VULKAN
#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>
#endif

namespace litt {

// =============================================================================
// RayTracingManager - BLAS/TLAS Building and Management
// =============================================================================

class RayTracingManager {
public:
    struct Context {
#ifdef LITT_VULKAN
        VkDevice device;
        VkPhysicalDevice physical_device;
        VkQueue graphics_queue;
        VkCommandPool command_pool;
        uint32_t queue_family;
        
        // Ray tracing properties
        VkPhysicalDeviceRayTracingPropertiesNV rt_properties;
        VkPhysicalDeviceAccelerationStructurePropertiesNV as_properties;
#endif
    };
    
    RayTracingManager() : context_() {}
    
    bool initialize(const Context& ctx) {
        context_ = ctx;
        
#ifdef LITT_VULKAN
        // Query ray tracing capabilities
        VkPhysicalDeviceRayTracingPipelinePropertiesKHR rt_pipeline_props = {};
        rt_pipeline_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
        
        VkPhysicalDeviceProperties2 props2 = {};
        props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        props2.pNext = &rt_pipeline_props;
        
        vkGetPhysicalDeviceProperties2(ctx.physical_device, &props2);
        
        // Query acceleration structure properties
        VkPhysicalDeviceAccelerationStructurePropertiesNV as_props = {};
        as_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_NV;
        props2.pNext = &as_props;
        
        vkGetPhysicalDeviceProperties2(ctx.physical_device, &props2);
        
        return true;
#else
        return false;
#endif
    }
    
    // =============================================================================
    // BLAS Building
    // =============================================================================
    
    bool build_blas(const std::vector<Vertex>& vertices,
                    const std::vector<uint32_t>& indices,
                    VkAccelerationStructureNV* out_blas) {
#ifdef LITT_VULKAN
        if (!out_blas) return false;
        
        // Create vertex buffer
        VkBufferCreateInfo buffer_info = {};
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size = vertices.size() * sizeof(Vertex);
        buffer_info.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_HOST_ADDRESS_BIT_NV |
                           VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        
        VkBuffer vertex_buffer;
        if (vkCreateBuffer(context_.device, &buffer_info, nullptr, &vertex_buffer) != VK_SUCCESS) {
            return false;
        }
        
        // Create index buffer
        buffer_info.size = indices.size() * sizeof(uint32_t);
        buffer_info.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_HOST_ADDRESS_BIT_NV |
                           VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        
        VkBuffer index_buffer;
        if (vkCreateBuffer(context_.device, &buffer_info, nullptr, &index_buffer) != VK_SUCCESS) {
            vkDestroyBuffer(context_.device, vertex_buffer, nullptr);
            return false;
        }
        
        // Build BLAS info
        VkAccelerationStructureInfoNV as_info = {};
        as_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV;
        as_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_NV;
        as_info.flags = 0;
        as_info.vertexBuffer = vertex_buffer;
        as_info.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
        as_info.vertexCount = static_cast<uint32_t>(vertices.size());
        as_info.pVertexData = vertices.data();
        as_info.indexBuffer = index_buffer;
        as_info.indexType = VK_INDEX_TYPE_UINT32;
        as_info.pIndexData = indices.data();
        as_info.primitiveCount = static_cast<uint32_t>(indices.size() / 3);
        as_info.pPrimitiveIndices = indices.data();
        
        // Query memory requirements
        VkMemoryRequirements mem_reqs;
        vkGetAccelerationStructureMemoryRequirementsNV(context_.device, &as_info, &mem_reqs);
        
        // Allocate memory
        VkMemoryAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize = mem_reqs.size;
        alloc_info.memoryTypeIndex = find_memory_type(mem_reqs.memoryTypeBits, 
                                                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        
        VkDeviceMemory memory;
        if (vkAllocateMemory(context_.device, &alloc_info, nullptr, &memory) != VK_SUCCESS) {
            vkDestroyBuffer(context_.device, vertex_buffer, nullptr);
            vkDestroyBuffer(context_.device, index_buffer, nullptr);
            return false;
        }
        
        // Bind memory
        vkBindBufferMemory(context_.device, vertex_buffer, memory, 0);
        vkBindBufferMemory(context_.device, index_buffer, memory, 0);
        
        // Create acceleration structure
        VkAccelerationStructureCreateInfoNV as_create_info = {};
        as_create_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_NV;
        as_create_info.info = as_info;
        
        if (vkCreateAccelerationStructureNV(context_.device, &as_create_info, nullptr, out_blas) != VK_SUCCESS) {
            vkFreeMemory(context_.device, memory, nullptr);
            vkDestroyBuffer(context_.device, vertex_buffer, nullptr);
            vkDestroyBuffer(context_.device, index_buffer, nullptr);
            return false;
        }
        
        // Build the acceleration structure
        VkCommandBufferAllocateInfo cmd_alloc = {};
        cmd_alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmd_alloc.commandPool = context_.command_pool;
        cmd_alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmd_alloc.commandBufferCount = 1;
        
        VkCommandBuffer cmd_buffer;
        vkAllocateCommandBuffers(context_.device, &cmd_alloc, &cmd_buffer);
        
        VkBuildAccelerationStructureInfoNV build_info = {};
        build_info.sType = VK_STRUCTURE_TYPE_BUILD_ACCELERATION_STRUCTURE_INFO_NV;
        build_info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_NV;
        build_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_NV;
        build_info.srcAccelerationStructure = VK_NULL_HANDLE;
        build_info.dstAccelerationStructure = *out_blas;
        build_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_NV;
        build_info.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
        build_info.vertexCount = static_cast<uint32_t>(vertices.size());
        build_info.pVertices = vertices.data();
        build_info.primitiveCount = static_cast<uint32_t>(indices.size() / 3);
        build_info.pPrimitives = indices.data();
        build_info.pVertexData = vertices.data();
        build_info.pIndexData = indices.data();
        
        vkCmdBuildAccelerationStructureNV(cmd_buffer, &build_info, VK_NULL_HANDLE, 
                                           VK_FALSE, VK_NULL_HANDLE, VK_NULL_HANDLE);
        
        // Submit command buffer
        VkSubmitInfo submit = {};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &cmd_buffer;
        
        vkQueueSubmit(context_.graphics_queue, 1, &submit, VK_NULL_HANDLE);
        vkQueueWaitIdle(context_.graphics_queue);
        
        // Cleanup
        vkFreeCommandBuffers(context_.device, context_.command_pool, 1, &cmd_buffer);
        vkDestroyBuffer(context_.device, vertex_buffer, nullptr);
        vkDestroyBuffer(context_.device, index_buffer, nullptr);
        vkFreeMemory(context_.device, memory, nullptr);
        
        return true;
#else
        return false;
#endif
    }
    
    // =============================================================================
    // TLAS Building
    // =============================================================================
    
    bool build_tlas(const std::vector<VkAccelerationStructureNV>& blases,
                    const std::vector<Transform>& transforms,
                    VkAccelerationStructureNV* out_tlas) {
#ifdef LITT_VULKAN
        if (!out_tlas || blases.empty()) return false;
        
        // Create TLAS info
        VkAccelerationStructureInfoNV as_info = {};
        as_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV;
        as_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_NV;
        as_info.flags = 0;
        as_info.instanceCount = static_cast<uint32_t>(blases.size());
        as_info.pGeometryKHR = nullptr;
        as_info.primitiveCount = 0;
        as_info.pPrimitiveIndices = nullptr;
        
        // Query memory requirements
        VkMemoryRequirements mem_reqs;
        vkGetAccelerationStructureMemoryRequirementsNV(context_.device, &as_info, &mem_reqs);
        
        // Allocate memory
        VkMemoryAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize = mem_reqs.size;
        alloc_info.memoryTypeIndex = find_memory_type(mem_reqs.memoryTypeBits, 
                                                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        
        VkDeviceMemory memory;
        if (vkAllocateMemory(context_.device, &alloc_info, nullptr, &memory) != VK_SUCCESS) {
            return false;
        }
        
        // Create acceleration structure
        VkAccelerationStructureCreateInfoNV as_create_info = {};
        as_create_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_NV;
        as_create_info.info = as_info;
        
        if (vkCreateAccelerationStructureNV(context_.device, &as_create_info, nullptr, out_tlas) != VK_SUCCESS) {
            vkFreeMemory(context_.device, memory, nullptr);
            return false;
        }
        
        // Build TLAS
        VkCommandBufferAllocateInfo cmd_alloc = {};
        cmd_alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmd_alloc.commandPool = context_.command_pool;
        cmd_alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmd_alloc.commandBufferCount = 1;
        
        VkCommandBuffer cmd_buffer;
        vkAllocateCommandBuffers(context_.device, &cmd_alloc, &cmd_buffer);
        
        // Build TLAS with instances
        std::vector<VkAccelerationStructureInstanceKHR> instances(blases.size());
        for (uint32_t i = 0; i < blases.size(); ++i) {
            instances[i].transform = transform_to_vk(transforms[i]);
            instances[i].instanceCustomIndex = i;
            instances[i].mask = 0xFF;
            instances[i].instanceShaderBindingTableRecordOffset = 0;
            instances[i].flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FAN_CW_BIT_NV;
            instances[i].accelerationStructureHandle = blases[i];
        }
        
        VkBuildAccelerationStructureInfoNV build_info = {};
        build_info.sType = VK_STRUCTURE_TYPE_BUILD_ACCELERATION_STRUCTURE_INFO_NV;
        build_info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_NV;
        build_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_NV;
        build_info.srcAccelerationStructure = VK_NULL_HANDLE;
        build_info.dstAccelerationStructure = *out_tlas;
        build_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_NV;
        build_info.instanceCount = static_cast<uint32_t>(blases.size());
        build_info.pInstances = instances.data();
        
        vkCmdBuildAccelerationStructureNV(cmd_buffer, &build_info, VK_NULL_HANDLE, 
                                           VK_FALSE, VK_NULL_HANDLE, VK_NULL_HANDLE);
        
        // Submit
        VkSubmitInfo submit = {};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &cmd_buffer;
        
        vkQueueSubmit(context_.graphics_queue, 1, &submit, VK_NULL_HANDLE);
        vkQueueWaitIdle(context_.graphics_queue);
        
        // Cleanup
        vkFreeCommandBuffers(context_.device, context_.command_pool, 1, &cmd_buffer);
        vkFreeMemory(context_.device, memory, nullptr);
        
        return true;
#else
        return false;
#endif
    }
    
    // =============================================================================
    // Ray Tracing Pipeline
    // =============================================================================
    
    bool create_ray_tracing_pipeline(
        const std::vector<VkPipelineShaderStageCreateInfo>& stages,
        VkAccelerationStructureNV tlas,
        VkPipeline* out_pipeline) {
#ifdef LITT_VULKAN
        VkPipelineCreateFlags flags = VK_PIPELINE_CREATE_RAY_TRACING_PIPELINE_BIT_KHR;
        
        VkRayTracingPipelineCreateInfoKHR rt_pipeline_info = {};
        rt_pipeline_info.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
        rt_pipeline_info.flags = flags;
        rt_pipeline_info.stageCount = static_cast<uint32_t>(stages.size());
        rt_pipeline_info.pStages = stages.data();
        rt_pipeline_info.pGroupEntries = nullptr;
        rt_pipeline_info.groupCount = 0;
        rt_pipeline_info.maxPipelineRayRecursionDepth = 1;
        rt_pipeline_info.pLibraryInfo = nullptr;
        rt_pipeline_info.pHitShaderGroupTable = nullptr;
        
        if (vkCreateRayTracingPipelinesKHR(context_.device, VK_NULL_HANDLE, 
                                            VK_NULL_HANDLE, 1, &rt_pipeline_info, 
                                            nullptr, out_pipeline) != VK_SUCCESS) {
            return false;
        }
        
        return true;
#else
        return false;
#endif
    }
    
private:
    Context context_;
    
    uint32_t find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags properties) {
#ifdef LITT_VULKAN
        VkPhysicalDeviceMemoryProperties mem_properties;
        vkGetPhysicalDeviceMemoryProperties(context_.physical_device, &mem_properties);
        
        for (uint32_t i = 0; i < mem_properties.memoryTypeCount; ++i) {
            if ((type_filter & (1 << i)) && 
                (mem_properties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }
        
        return 0xFFFFFFFF;
#else
        return 0xFFFFFFFF;
#endif
    }
    
    VkTransformMatrixKHR transform_to_vk(const Transform& transform) {
        VkTransformMatrixKHR matrix = {};
        // Convert our transform to VK matrix
        matrix.matrix[0] = transform.matrix[0];
        matrix.matrix[1] = transform.matrix[1];
        matrix.matrix[2] = transform.matrix[2];
        matrix.matrix[3] = 0.0f;
        matrix.matrix[4] = transform.matrix[4];
        matrix.matrix[5] = transform.matrix[5];
        matrix.matrix[6] = transform.matrix[6];
        matrix.matrix[7] = 0.0f;
        matrix.matrix[8] = transform.matrix[8];
        matrix.matrix[9] = transform.matrix[9];
        matrix.matrix[10] = transform.matrix[10];
        matrix.matrix[11] = 0.0f;
        matrix.matrix[12] = transform.position.x;
        matrix.matrix[13] = transform.position.y;
        matrix.matrix[14] = transform.position.z;
        matrix.matrix[15] = 1.0f;
        return matrix;
    }
};

// =============================================================================
// Global Ray Tracing Manager
// =============================================================================

static std::unique_ptr<RayTracingManager> g_ray_tracing_manager;

bool init_ray_tracing(const RayTracingContext& ctx) {
    if (!g_ray_tracing_manager) {
        g_ray_tracing_manager = std::make_unique<RayTracingManager>();
    }
    
    RayTracingManager::Context rt_ctx;
#ifdef LITT_VULKAN
    rt_ctx.device = ctx.device;
    rt_ctx.physical_device = ctx.physical_device;
    rt_ctx.graphics_queue = ctx.graphics_queue;
    rt_ctx.command_pool = ctx.command_pool;
    rt_ctx.queue_family = ctx.queue_family;
#endif
    
    return g_ray_tracing_manager->initialize(rt_ctx);
}

bool build_blas(const std::vector<Vertex>& vertices,
                const std::vector<uint32_t>& indices,
                VkAccelerationStructureNV* out_blas) {
    if (!g_ray_tracing_manager) return false;
    return g_ray_tracing_manager->build_blas(vertices, indices, out_blas);
}

bool build_tlas(const std::vector<VkAccelerationStructureNV>& blases,
                const std::vector<Transform>& transforms,
                VkAccelerationStructureNV* out_tlas) {
    if (!g_ray_tracing_manager) return false;
    return g_ray_tracing_manager->build_tlas(blases, transforms, out_tlas);
}

bool create_ray_tracing_pipeline(
    const std::vector<VkPipelineShaderStageCreateInfo>& stages,
    VkAccelerationStructureNV tlas,
    VkPipeline* out_pipeline) {
    if (!g_ray_tracing_manager) return false;
    return g_ray_tracing_manager->create_ray_tracing_pipeline(stages, tlas, out_pipeline);
}

} // namespace litt
