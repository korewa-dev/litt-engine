// Litt World - Complete world simulation with rendering
// Port of src/world_bridge.rs to C++

#include "litt_math.h"
#include "litt_ecs.h"
#include "litt_world.h"
#include "litt_obj.h"
#include "litt_json.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
#include <unordered_map>
#include <string>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>

using namespace litt;

// =============================================================================
// Scene data structures
// =============================================================================

struct MaterialDef {
    Vec3 base_color = Vec3(0.8f, 0.8f, 0.8f);
    float roughness = 0.5f;
    float metallic = 0.0f;
    float emissive = 0.0f;
};

struct Triangle {
    Vec3 v0, v1, v2;
    Vec3 normal;
    int mat_idx;
};

struct Sphere {
    Vec3 center;
    float radius;
    int mat_idx;
};

struct Scene {
    std::vector<Triangle> triangles;
    std::vector<Sphere> spheres;
    std::vector<MaterialDef> materials;
    std::vector<std::string> material_names;
    Aabb bounds;
    bool has_content = false;
    
    int triangle_count = 0;
    int sphere_count = 0;
    int material_count = 0;
    int missing_models = 0;
};

// =============================================================================
// JSON parsing helpers
// =============================================================================

static std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\n\r");
    size_t end = s.find_last_not_of(" \t\n\r");
    return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
}

static std::string get_json_string(const char* json, const char* key, const char* def = "") {
    std::string k = std::string("\"") + key + "\"";
    size_t pos = strstr(json, k.c_str()) - json;
    if (pos == std::string::npos) return def;
    pos += k.length();
    // Skip to colon
    while (json[pos] != ':' && json[pos] != '\0') pos++;
    pos++;
    // Skip whitespace
    while (json[pos] == ' ' || json[pos] == '\t') pos++;
    if (json[pos] != '"') return def;
    pos++;
    size_t start = pos;
    while (json[pos] != '"' && json[pos] != '\0') pos++;
    return std::string(json + start, pos - start);
}

static float get_json_float(const char* json, const char* key, float def = 0.0f) {
    std::string k = std::string("\"") + key + "\"";
    size_t pos = strstr(json, k.c_str()) - json;
    if (pos == std::string::npos) return def;
    pos += k.length();
    while (json[pos] != ':' && json[pos] != '\0') pos++;
    pos++;
    while (json[pos] == ' ' || json[pos] == '\t') pos++;
    return strtof(json + pos, nullptr);
}

static int get_json_int(const char* json, const char* key, int def = 0) {
    std::string k = std::string("\"") + key + "\"";
    size_t pos = strstr(json, k.c_str()) - json;
    if (pos == std::string::npos) return def;
    pos += k.length();
    while (json[pos] != ':' && json[pos] != '\0') pos++;
    pos++;
    while (json[pos] == ' ' || json[pos] == '\t') pos++;
    return atoi(json + pos);
}

// Parse array of floats [x, y, z]
static bool parse_vec3(const char* json, const char* key, Vec3& out) {
    std::string k = std::string("\"") + key + "\"";
    size_t pos = strstr(json, k.c_str()) - json;
    if (pos == std::string::npos) return false;
    pos += k.length();
    while (json[pos] != '[' && json[pos] != '\0') pos++;
    if (json[pos] != '[') return false;
    pos++;
    char* end;
    out.x = strtof(json + pos, &end);
    pos = end - json;
    while (json[pos] != ',' && json[pos] != ']' && json[pos] != '\0') pos++;
    pos++;
    out.y = strtof(json + pos, &end);
    pos = end - json;
    while (json[pos] != ',' && json[pos] != ']' && json[pos] != '\0') pos++;
    pos++;
    out.z = strtof(json + pos, &end);
    return true;
}

// Parse array of floats [x, y, z, w]
static bool parse_vec4(const char* json, const char* key, Vec3& out) {
    std::string k = std::string("\"") + key + "\"";
    size_t pos = strstr(json, k.c_str()) - json;
    if (pos == std::string::npos) return false;
    pos += k.length();
    while (json[pos] != '[' && json[pos] != '\0') pos++;
    if (json[pos] != '[') return false;
    pos++;
    char* end;
    out.x = strtof(json + pos, &end);
    pos = end - json;
    while (json[pos] != ',' && json[pos] != ']' && json[pos] != '\0') pos++;
    pos++;
    out.y = strtof(json + pos, &end);
    pos = end - json;
    while (json[pos] != ',' && json[pos] != ']' && json[pos] != '\0') pos++;
    pos++;
    out.z = strtof(json + pos, &end);
    return true;
}

// =============================================================================
// Scene building
// =============================================================================

static void grow_bounds(const Vec3& p, Vec3& min, Vec3& max) {
    min.x = std::min(min.x, p.x); min.y = std::min(min.y, p.y); min.z = std::min(min.z, p.z);
    max.x = std::max(max.x, p.x); max.y = std::max(max.y, p.y); max.z = std::max(max.z, p.z);
}

static int find_or_create_material(Scene& scene, const char* name, const MaterialDef& mat) {
    for (int i = 0; i < (int)scene.materials.size(); i++) {
        if (scene.material_names[i] == name) return i;
    }
    int idx = scene.materials.size();
    scene.materials.push_back(mat);
    scene.material_names.push_back(name);
    return idx;
}

static MaterialDef parse_material(const char* json) {
    MaterialDef mat;
    std::string bc = get_json_string(json, "base_color", "");
    if (!bc.empty()) {
        // Parse hex color or array
        if (bc[0] == '#') {
            unsigned int col = 0;
            sscanf(bc.substr(1).c_str(), "%06x", &col);
            mat.base_color = Vec3(
                ((col >> 16) & 0xFF) / 255.0f,
                ((col >> 8) & 0xFF) / 255.0f,
                ((col >> 0) & 0xFF) / 255.0f
            );
        } else {
            mat.base_color = Vec3(0.8f, 0.8f, 0.8f);
        }
    }
    mat.roughness = get_json_float(json, "roughness", 0.5f);
    mat.metallic = get_json_float(json, "metallic", 0.0f);
    mat.emissive = get_json_float(json, "emissive", 0.0f);
    return mat;
}

// =============================================================================
// Node processing
// =============================================================================

static void process_node(Scene& scene, const char* json, const Vec3& parent_transform) {
    std::string name = get_json_string(json, "name", "");
    std::string mesh = get_json_string(json, "mesh", "");
    std::string model = get_json_string(json, "model", "");
    
    // Parse transform
    Vec3 pos(0, 0, 0), rot(0, 0, 0), scale(1, 1, 1);
    parse_vec3(json, "position", pos);
    parse_vec3(json, "rotation", rot);
    parse_vec3(json, "scale", scale);
    
    // Apply parent transform
    pos = pos + parent_transform;
    
    // Parse tags
    std::string tags_str = get_json_string(json, "tags", "");
    
    // Parse material
    std::string mat_name = get_json_string(json, "material", "");
    MaterialDef mat_def = parse_material(json);
    if (!mat_name.empty()) {
        mat_def.base_color = Vec3(0.8f, 0.8f, 0.8f); // default
    }
    
    int mat_idx = -1;
    if (!mat_name.empty()) {
        mat_idx = find_or_create_material(scene, mat_name.c_str(), mat_def);
    } else {
        mat_idx = find_or_create_material(scene, "default", mat_def);
    }
    
    // Process children
    const char* children = strstr(json, "\"children\"");
    if (children) {
        children = strstr(children, "[");
        if (children) {
            children++;
            while (*children && *children != ']') {
                if (*children == '{') {
                    // Find matching }
                    const char* end = children + 1;
                    int depth = 1;
                    while (*end && depth > 0) {
                        if (*end == '{') depth++;
                        else if (*end == '}') depth--;
                        end++;
                    }
                    std::string child_json(children, end - children);
                    process_node(scene, child_json.c_str(), pos);
                    children = end;
                } else {
                    children++;
                }
            }
        }
    }
    
    // Process mesh/model
    if (!mesh.empty() || !model.empty()) {
        std::string path = !mesh.empty() ? mesh : model;
        
        // Try to load OBJ
        LvModel lv_model;
        if (lv_obj_load(path.c_str(), &lv_model) == 0) {
            // Build triangles from OBJ
            for (int m = 0; m < lv_model.count; m++) {
                LvMesh* mesh = &lv_model.meshes[m];
                for (int i = 0; i < mesh->in; i += 3) {
                    if (i + 2 >= mesh->in) break;
                    
                    Vec3 v0(mesh->verts[mesh->idx[i] * 3 + 0],
                            mesh->verts[mesh->idx[i] * 3 + 1],
                            mesh->verts[mesh->idx[i] * 3 + 2]);
                    Vec3 v1(mesh->verts[mesh->idx[i+1] * 3 + 0],
                            mesh->verts[mesh->idx[i+1] * 3 + 1],
                            mesh->verts[mesh->idx[i+1] * 3 + 2]);
                    Vec3 v2(mesh->verts[mesh->idx[i+2] * 3 + 0],
                            mesh->verts[mesh->idx[i+2] * 3 + 1],
                            mesh->verts[mesh->idx[i+2] * 3 + 2]);
                    
                    // Apply transform
                    v0 = v0 * scale.x + pos;
                    v1 = v1 * scale.x + pos;
                    v2 = v2 * scale.x + pos;
                    
                    Vec3 normal = (v1 - v0).cross(v2 - v0).normalized();
                    
                    scene.triangles.push_back({v0, v1, v2, normal, mat_idx});
                    scene.triangle_count++;
                    grow_bounds(v0, scene.bounds.min, scene.bounds.max);
                    grow_bounds(v1, scene.bounds.min, scene.bounds.max);
                    grow_bounds(v2, scene.bounds.min, scene.bounds.max);
                }
            }
            scene.has_content = true;
            lv_model_free(&lv_model);
        } else {
            scene.missing_models++;
            std::fprintf(stderr, "[litt_world] missing model: %s\n", path.c_str());
        }
    }
    
    // Add fallback sphere based on tags
    bool has_content = !mesh.empty() || !model.empty();
    if (!has_content) {
        float radius = 0.5f;
        if (tags_str.find("enemy") != std::string::npos) radius = 1.0f;
        else if (tags_str.find("goal") != std::string::npos) radius = 1.5f;
        else if (tags_str.find("pick") != std::string::npos) radius = 0.3f;
        
        scene.spheres.push_back({pos, radius, mat_idx});
        scene.sphere_count++;
        grow_bounds(pos, scene.bounds.min, scene.bounds.max);
        scene.has_content = true;
    }
}

// =============================================================================
// Ground plane
// =============================================================================

static void add_ground_plane(Scene& scene, const Vec3& min, const Vec3& max) {
    float extent = 60.0f;
    if (scene.has_content) {
        extent = std::max(60.0f, std::max(std::abs(max.x - min.x), std::abs(max.z - min.z)) + 10.0f);
    }
    float gy = scene.has_content ? min.y - 0.05f : -0.05f;
    
    Vec3 c0(-extent, gy, -extent);
    Vec3 c1(extent, gy, -extent);
    Vec3 c2(extent, gy, extent);
    Vec3 c3(-extent, gy, extent);
    
    // Two triangles
    scene.triangles.push_back({c0, c1, c2, Vec3(0, 1, 0), 0});
    scene.triangles.push_back({c0, c2, c3, Vec3(0, 1, 0), 0});
    scene.triangle_count += 2;
}

// =============================================================================
// Public API
// =============================================================================

std::pair<Scene, std::string> build_scene_from_json(const char* json_text) {
    Scene scene;
    scene.bounds = {Vec3(1e10f, 1e10f, 1e10f), Vec3(-1e10f, -1e10f, -1e10f)};
    
    // Parse root node
    process_node(scene, json_text, Vec3::zero());
    
    // Add ground plane
    add_ground_plane(scene, scene.bounds.min, scene.bounds.max);
    
    // Set bounds
    if (scene.has_content) {
        scene.bounds = {
            Vec3(std::min(scene.bounds.min.x, -100.0f), std::min(scene.bounds.min.y, -10.0f), std::min(scene.bounds.min.z, -100.0f)),
            Vec3(std::max(scene.bounds.max.x, 100.0f), std::max(scene.bounds.max.y, 10.0f), std::max(scene.bounds.max.z, 100.0f))
        };
    } else {
        scene.bounds = {Vec3(-60, -0.05, -60), Vec3(60, 0, 60)};
    }
    
    std::string stats = "Triangles: " + std::to_string(scene.triangle_count) +
                       ", Spheres: " + std::to_string(scene.sphere_count) +
                       ", Materials: " + std::to_string(scene.materials.size()) +
                       ", Missing: " + std::to_string(scene.missing_models);
    
    return {scene, stats};
}

std::pair<Scene, std::string> build_scene_from_file(const char* path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        std::fprintf(stderr, "[litt_world] failed to open: %s\n", path);
        return {{}, "Error: file not found"};
    }
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return build_scene_from_json(content.c_str());
}

// =============================================================================
// Save scene to JSON
// =============================================================================

bool save_scene(const Scene& scene, const char* path) {
    std::ofstream f(path);
    if (!f.is_open()) return false;
    
    f << "{\n";
    f << "  \"triangles\": " << scene.triangle_count << ",\n";
    f << "  \"spheres\": " << scene.sphere_count << ",\n";
    f << "  \"materials\": " << scene.materials.size() << ",\n";
    f << "  \"bounds\": {\n";
    f << "    \"min\": [" << scene.bounds.min.x << ", " << scene.bounds.min.y << ", " << scene.bounds.min.z << "],\n";
    f << "    \"max\": [" << scene.bounds.max.x << ", " << scene.bounds.max.y << ", " << scene.bounds.max.z << "]\n";
    f << "  }\n";
    f << "}\n";
    
    return true;
}
