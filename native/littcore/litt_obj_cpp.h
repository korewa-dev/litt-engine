// LittObjCpp - C++ Wavefront OBJ loader
// C++ wrapper around C OBJ loader with additional features

#pragma once
#include "litt_math.h"
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

namespace litt {

// =============================================================================
// C++ OBJ Types
// =============================================================================

struct ObjVertex {
    Vec3f position;
    Vec3f normal;
    Vec2f texcoord;
};

struct ObjFace {
    struct Index {
        int position = -1;
        int normal = -1;
        int texcoord = -1;
    };
    std::vector<Index> indices;
};

struct ObjMaterial {
    std::string name;
    Vec3f ambient = Vec3f::zero();
    Vec3f diffuse = Vec3f::one();
    Vec3f specular = Vec3f::zero();
    float shininess = 0.0f;
    float transparency = 1.0f;
    std::string diffuse_map;
    std::string normal_map;
};

struct ObjMesh {
    std::string name;
    std::vector<ObjVertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<ObjFace> faces;
    std::string material;
    Aabbf bounds;
    bool loaded = false;
    
    size_t vertex_count() const { return vertices.size(); }
    size_t index_count() const { return indices.size(); }
    size_t face_count() const { return faces.size(); }
};

struct ObjModel {
    std::string filepath;
    std::vector<ObjMesh> meshes;
    std::unordered_map<std::string, ObjMaterial> materials;
    Aabbf bounds;
    bool loaded = false;
    
    // Find mesh by name
    const ObjMesh* find_mesh(const std::string& name) const {
        for (const auto& mesh : meshes) {
            if (mesh.name == name) return &mesh;
        }
        return nullptr;
    }
    
    ObjMesh* find_mesh(const std::string& name) {
        for (auto& mesh : meshes) {
            if (mesh.name == name) return &mesh;
        }
        return nullptr;
    }
};

// =============================================================================
// OBJ Loader
// =============================================================================

class ObjLoader {
public:
    ObjLoader() = default;
    
    // Load from file
    bool load(const std::string& filepath, ObjModel& model) {
        model.filepath = filepath;
        model.meshes.clear();
        model.materials.clear();
        
        std::ifstream file(filepath);
        if (!file.is_open()) {
            std::cerr << "Failed to open OBJ file: " << filepath << std::endl;
            return false;
        }
        
        // Read MTL file if referenced
        std::string mtl_path;
        std::string line;
        std::string current_material;
        std::string current_object;
        
        // Temporary storage for parsing
        std::vector<Vec3f> raw_positions;
        std::vector<Vec3f> raw_normals;
        std::vector<Vec2f> raw_texcoords;
        std::vector<ObjFace> raw_faces;
        
        while (std::getline(file, line)) {
            // Skip comments and empty lines
            if (line.empty() || line[0] == '#') continue;
            
            std::istringstream iss(line);
            std::string prefix;
            iss >> prefix;
            
            if (prefix == "v") {
                // Vertex position
                float x, y, z;
                if (iss >> x >> y >> z) {
                    raw_positions.push_back({x, y, z});
                }
            }
            else if (prefix == "vn") {
                // Vertex normal
                float x, y, z;
                if (iss >> x >> y >> z) {
                    raw_normals.push_back({x, y, z});
                }
            }
            else if (prefix == "vt") {
                // Vertex texture coordinate
                float u, v;
                if (iss >> u >> v) {
                    raw_texcoords.push_back({u, v});
                }
            }
            else if (prefix == "f") {
                // Face
                ObjFace face;
                std::string token;
                while (iss >> token) {
                    ObjFace::Index idx;
                    idx.position = -1;
                    idx.normal = -1;
                    idx.texcoord = -1;
                    
                    // Parse "v/n/t" or "v//t" or "v/n" format
                    std::istringstream token_stream(token);
                    std::string part;
                    int part_idx = 0;
                    while (std::getline(token_stream, part, '/')) {
                        if (part_idx == 0 && !part.empty()) {
                            idx.position = std::stoi(part) - 1;
                        }
                        else if (part_idx == 1 && !part.empty()) {
                            idx.texcoord = std::stoi(part) - 1;
                        }
                        else if (part_idx == 2 && !part.empty()) {
                            idx.normal = std::stoi(part) - 1;
                        }
                        part_idx++;
                    }
                    face.indices.push_back(idx);
                }
                raw_faces.push_back(face);
            }
            else if (prefix == "mtllib") {
                // Material library
                iss >> mtl_path;
                if (!mtl_path.empty()) {
                    load_materials(filepath, mtl_path, model.materials);
                }
            }
            else if (prefix == "usemtl") {
                // Use material
                iss >> current_material;
            }
            else if (prefix == "o" || prefix == "g") {
                // Object or group name
                iss >> current_object;
            }
        }
        
        // Convert to mesh format
        if (!raw_positions.empty() && !raw_faces.empty()) {
            ObjMesh mesh;
            mesh.name = current_object.empty() ? "mesh" : current_object;
            
            // Build vertices from faces
            for (const auto& face : raw_faces) {
                // Triangle fan for n-gons
                if (face.indices.size() >= 3) {
                    for (size_t i = 1; i < face.indices.size() - 1; i++) {
                        add_face_vertices(mesh, face.indices[0], face.indices[i], face.indices[i + 1],
                                         raw_positions, raw_normals, raw_texcoords);
                    }
                }
            }
            
            // Compute bounds
            compute_bounds(mesh);
            
            // Compute normals if missing
            if (mesh.vertices.empty() || (mesh.vertices.size() > 0 && mesh.vertices[0].normal == Vec3f::zero())) {
                compute_normals(mesh);
            }
            
            model.meshes.push_back(mesh);
            model.loaded = true;
        }
        
        // Update model bounds
        compute_model_bounds(model);
        
        return model.loaded;
    }
    
    // Save to file
    bool save(const std::string& filepath, const ObjModel& model) {
        std::ofstream file(filepath);
        if (!file.is_open()) return false;
        
        // Write objects
        for (const auto& mesh : model.meshes) {
            file << "o " << mesh.name << "\n";
            
            // Write vertices
            for (const auto& v : mesh.vertices) {
                file << "v " << v.position.x << " " << v.position.y << " " << v.position.z << "\n";
            }
            
            // Write normals
            for (const auto& v : mesh.vertices) {
                file << "vn " << v.normal.x << " " << v.normal.y << " " << v.normal.z << "\n";
            }
            
            // Write texture coords
            for (const auto& v : mesh.vertices) {
                file << "vt " << v.texcoord.x << " " << v.texcoord.y << "\n";
            }
            
            // Write faces
            for (size_t i = 0; i < mesh.indices.size(); i += 3) {
                file << "f " 
                     << (mesh.indices[i] + 1) << "//" << (mesh.indices[i] + 1) << " "
                     << (mesh.indices[i + 1] + 1) << "//" << (mesh.indices[i + 1] + 1) << " "
                     << (mesh.indices[i + 2] + 1) << "//" << (mesh.indices[i + 2] + 1) << "\n";
            }
            
            file << "\n";
        }
        
        return true;
    }
    
    // Utility functions
    static void compute_bounds(ObjMesh& mesh) {
        if (mesh.vertices.empty()) {
            mesh.bounds = Aabbf::empty();
            return;
        }
        
        mesh.bounds.min = mesh.vertices[0].position;
        mesh.bounds.max = mesh.vertices[0].position;
        
        for (const auto& v : mesh.vertices) {
            mesh.bounds.min = Vec3f(
                std::min(mesh.bounds.min.x, v.position.x),
                std::min(mesh.bounds.min.y, v.position.y),
                std::min(mesh.bounds.min.z, v.position.z)
            );
            mesh.bounds.max = Vec3f(
                std::max(mesh.bounds.max.x, v.position.x),
                std::max(mesh.bounds.max.y, v.position.y),
                std::max(mesh.bounds.max.z, v.position.z)
            );
        }
    }
    
    static void compute_model_bounds(ObjModel& model) {
        if (model.meshes.empty()) {
            model.bounds = Aabbf::empty();
            return;
        }
        
        model.bounds = model.meshes[0].bounds;
        for (size_t i = 1; i < model.meshes.size(); i++) {
            model.bounds = model.bounds.merge(model.meshes[i].bounds);
        }
    }
    
    static void compute_normals(ObjMesh& mesh) {
        // Reset normals
        for (auto& v : mesh.vertices) {
            v.normal = Vec3f::zero();
        }
        
        // Accumulate face normals
        for (size_t i = 0; i < mesh.indices.size(); i += 3) {
            if (i + 2 >= mesh.indices.size()) break;
            
            const auto& v0 = mesh.vertices[mesh.indices[i]];
            const auto& v1 = mesh.vertices[mesh.indices[i + 1]];
            const auto& v2 = mesh.vertices[mesh.indices[i + 2]];
            
            Vec3f edge1 = v1.position - v0.position;
            Vec3f edge2 = v2.position - v0.position;
            Vec3f normal = edge1.cross(edge2).normalized();
            
            mesh.vertices[mesh.indices[i]].normal += normal;
            mesh.vertices[mesh.indices[i + 1]].normal += normal;
            mesh.vertices[mesh.indices[i + 2]].normal += normal;
        }
        
        // Normalize
        for (auto& v : mesh.vertices) {
            v.normal = v.normal.normalized();
        }
    }
    
private:
    void add_face_vertices(ObjMesh& mesh, const ObjFace::Index& i0, 
                          const ObjFace::Index& i1, const ObjFace::Index& i2,
                          const std::vector<Vec3f>& positions,
                          const std::vector<Vec3f>& normals,
                          const std::vector<Vec2f>& texcoords) {
        // First vertex
        auto add_vertex = [&](const ObjFace::Index& idx) {
            ObjVertex v;
            if (idx.position >= 0 && idx.position < (int)positions.size()) {
                v.position = positions[idx.position];
            }
            if (idx.normal >= 0 && idx.normal < (int)normals.size()) {
                v.normal = normals[idx.normal];
            } else {
                v.normal = Vec3f::unit_y();
            }
            if (idx.texcoord >= 0 && idx.texcoord < (int)texcoords.size()) {
                v.texcoord = texcoords[idx.texcoord];
            }
            mesh.vertices.push_back(v);
            mesh.indices.push_back(mesh.vertices.size() - 1);
        };
        
        add_vertex(i0);
        add_vertex(i1);
        add_vertex(i2);
    }
    
    void load_materials(const std::string& obj_path, const std::string& mtl_path,
                       std::unordered_map<std::string, ObjMaterial>& materials) {
        // Resolve MTL path relative to OBJ path
        std::string full_path = mtl_path;
        if (mtl_path[0] != '/' && mtl_path.find(':') == std::string::npos) {
            full_path = obj_path.substr(0, obj_path.find_last_of('/')) + "/" + mtl_path;
        }
        
        std::ifstream file(full_path);
        if (!file.is_open()) return;
        
        ObjMaterial current_mat;
        std::string line;
        
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '#') continue;
            
            std::istringstream iss(line);
            std::string prefix;
            iss >> prefix;
            
            if (prefix == "newmtl") {
                // Save previous material
                if (!current_mat.name.empty()) {
                    materials[current_mat.name] = current_mat;
                }
                // Reset FIRST, then read the new name. The old order read the
                // name and then wiped it with the reset, so every material
                // after the first was silently stored under an empty key
                // (i.e., dropped).
                current_mat = ObjMaterial();
                iss >> current_mat.name;
            }
            else if (prefix == "Ka") {
                float r, g, b;
                iss >> r >> g >> b;
                current_mat.ambient = {r, g, b};
            }
            else if (prefix == "Kd") {
                float r, g, b;
                iss >> r >> g >> b;
                current_mat.diffuse = {r, g, b};
            }
            else if (prefix == "Ks") {
                float r, g, b;
                iss >> r >> g >> b;
                current_mat.specular = {r, g, b};
            }
            else if (prefix == "Ns") {
                iss >> current_mat.shininess;
            }
            else if (prefix == "d") {
                iss >> current_mat.transparency;
            }
            else if (prefix == "map_Kd") {
                iss >> current_mat.diffuse_map;
            }
            else if (prefix == "map_Nm") {
                iss >> current_mat.normal_map;
            }
        }
        
        // Save last material
        if (!current_mat.name.empty()) {
            materials[current_mat.name] = current_mat;
        }
    }
};

} // namespace litt
