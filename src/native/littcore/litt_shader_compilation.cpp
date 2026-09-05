// Shader Compilation Pipeline
// SPIR-V (Vulkan) and DXIL (DX12) compilation support

#include "litt_renderer.h"
#include <fstream>
#include <sstream>
#include <cstring>
#include <algorithm>

#ifdef LITT_SPIRV
#include <spirv_cross/spirv_cross.hpp>
#endif

#ifdef LITT_DXIL
#include <d3d12.h>
#include <d3d12shader.h>
#include <d3d12shader_platform.h>
#endif

namespace litt {

// =============================================================================
// ShaderCompilationPipeline
// =============================================================================

class ShaderCompilationPipeline {
public:
    enum class ShaderType {
        Vertex,
        Fragment,
        Compute,
        RayGen,
        Miss,
        HitGroup,
        Intersection
    };
    
    enum class TargetBackend {
        SPIR_V,
        DXIL,
        GLSL,
        HLSL
    };
    
    struct ShaderSource {
        std::string path;
        std::string source;
        ShaderType type;
        TargetBackend target;
    };
    
    struct CompiledShader {
        std::vector<uint8_t> code;
        ShaderType type;
        TargetBackend backend;
        std::string entry_point;
        std::vector<uint32_t> specialization_constants;
    };
    
private:
    std::vector<ShaderSource> sources_;
    std::vector<CompiledShader> compiled_;
    
public:
    ShaderCompilationPipeline() = default;
    ~ShaderCompilationPipeline() = default;
    
    // =============================================================================
    // Source Loading
    // =============================================================================
    
    bool add_source(const std::string& path, ShaderType type, TargetBackend backend) {
        ShaderSource source;
        source.path = path;
        source.type = type;
        source.target = backend;
        
        // Read source file
        std::ifstream file(path);
        if (!file) {
            return false;
        }
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        source.source = buffer.str();
        
        sources_.push_back(source);
        return true;
    }
    
    // =============================================================================
    // SPIR-V Compilation (Vulkan)
    // =============================================================================
    
#ifdef LITT_SPIRV
    bool compile_to_spirv(const std::string& glsl_source,
                          ShaderType type,
                          const std::string& entry_point,
                          std::vector<uint32_t>* out_spirv) {
        try {
            spirv_cross::CompilerGLSL glsl_compiler(glsl_source);
            
            // Set entry point
            spirv_cross::CompilerOptions options;
            options.entry_point = entry_point;
            
            // Convert to SPIR-V
            spirv_cross::CompilerSPIRV spv_compiler(glsl_compiler);
            spv_compiler.compile(options);
            
            // Get SPIR-V binary
            const uint32_t* spv_data = spv_compiler.get_binary();
            size_t spv_size = spv_compiler.get_binary_size();
            
            out_spirv->assign(spv_data, spv_data + spv_size);
            return true;
        } catch (const std::exception& e) {
            return false;
        }
    }
    
    bool compile_spirv_file(const std::string& glsl_path,
                            ShaderType type,
                            const std::string& entry_point,
                            std::vector<uint32_t>* out_spirv) {
        // Read GLSL source
        std::ifstream file(glsl_path);
        if (!file) {
            return false;
        }
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        
        return compile_to_spirv(buffer.str(), type, entry_point, out_spirv);
    }
#endif
    
    // =============================================================================
    // DXIL Compilation (DX12)
    // =============================================================================
    
#ifdef LITT_DXIL
    bool compile_to_dxil(const std::string& hlsl_source,
                         ShaderType type,
                         const std::string& entry_point,
                         const std::string& target_profile,
                         std::vector<uint8_t>* out_dxil) {
        // Use D3DCompile to compile HLSL to DXIL
        ID3DBlob* shader_blob = nullptr;
        ID3DBlob* error_blob = nullptr;
        
        D3D_SHADER_MACRO macros[] = {
            "TARGET_PROFILE", target_profile.c_str(),
            nullptr, nullptr
        };
        
        HRESULT hr = D3DCompile(
            hlsl_source.c_str(),
            hlsl_source.size(),
            nullptr,
            macros,
            nullptr,
            entry_point.c_str(),
            target_profile.c_str(),
            0,
            0,
            &shader_blob,
            &error_blob
        );
        
        if (FAILED(hr)) {
            if (error_blob) {
                // Output compile errors
                fprintf(stderr, "Shader compilation failed:\n%s\n", 
                        (char*)error_blob->GetBufferPointer());
            }
            if (error_blob) error_blob->Release();
            return false;
        }
        
        // DXIL is the compiled output
        const uint8_t* dxil_data = static_cast<const uint8_t*>(shader_blob->GetBufferPointer());
        size_t dxil_size = shader_blob->GetBufferSize();
        
        out_dxil->assign(dxil_data, dxil_data + dxil_size);
        
        shader_blob->Release();
        return true;
    }
    
    bool compile_dxil_file(const std::string& hlsl_path,
                           ShaderType type,
                           const std::string& entry_point,
                           const std::string& target_profile,
                           std::vector<uint8_t>* out_dxil) {
        // Read HLSL source
        std::ifstream file(hlsl_path);
        if (!file) {
            return false;
        }
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        
        return compile_to_dxil(buffer.str(), type, entry_point, target_profile, out_dxil);
    }
#endif
    
    // =============================================================================
    // Pipeline Compilation
    // =============================================================================
    
    bool compile_all() {
        compiled_.clear();
        
        for (auto& source : sources_) {
            CompiledShader compiled;
            compiled.type = source.type;
            compiled.backend = source.target;
            compiled.entry_point = "main"; // Default entry point
            
            switch (source.target) {
#ifdef LITT_SPIRV
                case TargetBackend::SPIR_V:
                    if (compile_to_spirv(source.source, source.type, compiled.entry_point,
                                         &compiled.code)) {
                        compiled_.push_back(compiled);
                    }
                    break;
#endif
#ifdef LITT_DXIL
                case TargetBackend::DXIL:
                    if (compile_to_dxil(source.source, source.type, compiled.entry_point,
                                        "lib_6_0", &compiled.code)) {
                        compiled_.push_back(compiled);
                    }
                    break;
#endif
                default:
                    // Unsupported backend
                    break;
            }
        }
        
        return !compiled_.empty();
    }
    
    const std::vector<CompiledShader>& get_compiled() const {
        return compiled_;
    }
    
    CompiledShader* find_shader(ShaderType type, TargetBackend backend) {
        for (auto& shader : compiled_) {
            if (shader.type == type && shader.backend == backend) {
                return &shader;
            }
        }
        return nullptr;
    }
    
    const CompiledShader* find_shader(ShaderType type, TargetBackend backend) const {
        for (const auto& shader : compiled_) {
            if (shader.type == type && shader.backend == backend) {
                return &shader;
            }
        }
        return nullptr;
    }
};

// =============================================================================
// Global Pipeline
// =============================================================================

static std::unique_ptr<ShaderCompilationPipeline> g_shader_pipeline;

// =============================================================================
// Exported Functions
// =============================================================================

bool init_shader_pipeline() {
    if (!g_shader_pipeline) {
        g_shader_pipeline = std::make_unique<ShaderCompilationPipeline>();
    }
    return true;
}

bool add_shader_source(const std::string& path, 
                       ShaderCompilationPipeline::ShaderType type,
                       ShaderCompilationPipeline::TargetBackend backend) {
    if (!g_shader_pipeline) return false;
    return g_shader_pipeline->add_source(path, type, backend);
}

bool compile_shaders() {
    if (!g_shader_pipeline) return false;
    return g_shader_pipeline->compile_all();
}

const std::vector<ShaderCompilationPipeline::CompiledShader>& get_compiled_shaders() {
    static std::vector<ShaderCompilationPipeline::CompiledShader> empty;
    if (!g_shader_pipeline) return empty;
    return g_shader_pipeline->get_compiled();
}

ShaderCompilationPipeline::CompiledShader* find_shader(
    ShaderCompilationPipeline::ShaderType type,
    ShaderCompilationPipeline::TargetBackend backend) {
    if (!g_shader_pipeline) return nullptr;
    return g_shader_pipeline->find_shader(type, backend);
}

// =============================================================================
// Utility Functions
// =============================================================================

bool load_spirv_from_file(const std::string& path, std::vector<uint32_t>* out_spirv) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    
    // Read SPIR-V binary
    out_spirv->assign(
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>()
    );
    
    // Convert to uint32_t
    std::vector<uint32_t> result(out_spirv->size() / sizeof(uint32_t));
    std::memcpy(result.data(), out_spirv->data(), out_spirv->size());
    
    *out_spirv = result;
    return true;
}

bool load_dxil_from_file(const std::string& path, std::vector<uint8_t>* out_dxil) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    
    // Read DXIL binary
    out_dxil->assign(
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>()
    );
    
    return true;
}

} // namespace litt
