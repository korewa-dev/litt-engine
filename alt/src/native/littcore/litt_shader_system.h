// Phase 3: Rendering Pipeline - Shader System

#pragma once

#include "litt_math.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace litt {

// Shader types
enum class ShaderType {
    VERTEX,
    FRAGMENT,
    GEOMETRY,
    COMPUTE
};

// Shader compilation status
struct ShaderCompileResult {
    bool success = false;
    std::string log;
    uint32_t shader_id = 0;
};

// Shader program
class ShaderProgram {
public:
    ShaderProgram() = default;
    ~ShaderProgram() = default;
    
    // Attach shader
    void attach_shader(ShaderType type, const std::string& source);
    
    // Link program
    bool link();
    
    // Use program
    void bind() const;
    void unbind() const;
    
    // Uniform setters
    void set_float(const std::string& name, float value);
    void set_int(const std::string& name, int value);
    void set_vec2(const std::string& name, const Vec2& value);
    void set_vec3(const std::string& name, const Vec3& value);
    void set_vec4(const std::string& name, const Vec4& value);
    void set_mat4(const std::string& name, const Mat4& value);
    
    // Get program ID
    uint32_t get_id() const { return program_id_; }
    
    // Check if linked
    bool is_linked() const { return linked_; }

private:
    uint32_t program_id_ = 0;
    bool linked_ = false;
    std::unordered_map<ShaderType, uint32_t> shaders_;
    std::unordered_map<std::string, int> uniform_locations_;
};

// Shader library
class ShaderLibrary {
public:
    static ShaderLibrary& get_instance() {
        static ShaderLibrary instance;
        return instance;
    }
    
    // Load shader from file
    ShaderProgram* load_shader(const std::string& name, const std::string& filepath);
    
    // Get shader by name
    ShaderProgram* get_shader(const std::string& name);
    
    // Remove shader
    void remove_shader(const std::string& name);
    
    // Clear all shaders
    void clear();

private:
    ShaderLibrary() = default;
    std::unordered_map<std::string, std::unique_ptr<ShaderProgram>> shaders_;
};

// Built-in shaders
namespace builtin_shaders {
    // Simple PBR shader
    extern const char* pbr_vertex;
    extern const char* pbr_fragment;
    
    // Simple unlit shader
    extern const char* unlit_vertex;
    extern const char* unlit_fragment;
    
    // Shadow shader
    extern const char* shadow_vertex;
    extern const char* shadow_fragment;
    
    // Post-processing shaders
    extern const char* post_process_vertex;
    extern const char* post_process_fragment;
    extern const char* bloom_vertex;
    extern const char* bloom_fragment;
}

} // namespace litt
