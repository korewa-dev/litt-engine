// LittObjCpp - C++ Wavefront OBJ loader
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

struct ObjVertex {
    Vec3 position;
    Vec3 normal;
    Vec2 texcoord;
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
    Vec3 ambient = Vec3::zero();
    Vec3 diffuse = Vec3::one();
    Vec3 specular = Vec3::zero();
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
    std::string material_name;
    Aabb bounds;
    
    size_t vertex_count() const { return vertices.size(); }
    size_t index_count() const { return indices.size(); }
    size_t face_count() const { return faces.size(); }
};

struct ObjModel {
    std::string filepath;
    std::vector<ObjMesh> meshes;
    std::vector<ObjMaterial> materials;
    Aabb bounds;
    bool loaded = false;
    
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

class ObjLoader {
public:
    ObjLoader() = default;
    
    bool load(const std::string& filepath, ObjModel& model) {
        model.filepath = filepath;
        model.meshes.clear();
        model.materials.clear();
        
        std::ifstream file(filepath);
        if (!file.is_open()) {
            std::cerr << "Failed to open OBJ file: " << filepath << std::endl;
            return false;
        }
        
        std::string mtl_path;
        std::string line;
        std::string current_material;
        std::string current_object;
        
        std::vector<Vec3> raw_positions;
        std::vector<Vec3> raw_normals;
        std::vector<Vec2> raw_texcoords;
        std::vector<ObjFace> raw_faces;
        
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '#') continue;
            
            std::istringstream iss(line);
            std::string prefix;
            iss >> prefix;
            
            if (prefix == "v") {
                float x, y, z;
                if (iss >> x >> y >> z) {
                    raw_positions.push_back({x, y, z});
                }
            }
            else if (prefix == "vn") {
                float x, y, z;
                if (iss >> x >> y >> z) {
                    raw_normals.push_back({x, y, z});
                }
            }
            else if (prefix == "vt") {
                float u, v;
                if (iss >> u >> v) {
                    raw_texcoords.push_back({u, v});
                }
            }
            else if (prefix == "f") {
                ObjFace face;
                std::string token;
                while (iss >> token) {
                    ObjFace::Index idx;
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
                iss >> mtl_path;
            }
            else if (prefix == "usemtl") {
                iss >> current_material;
            }
            else if (prefix == "o" || prefix == "g") {
                iss >> current_object;
            }
        }
        
        if (!raw_positions.empty() && !raw_faces.empty()) {
            ObjMesh mesh;
            mesh.name = current_object.empty() ? "mesh" : current_object;
            
            for (const auto& face : raw_faces) {
                if (face.indices.size() >= 3) {
                    for (size_t i = 1; i < face.indices.size() - 1; i++) {
                        add_face_vertices(mesh, face.indices[0], face.indices[i], face.indices[i + 1],
                                         raw_positions, raw_normals, raw_texcoords);
                    }
                }
            }
            
            compute_bounds(mesh);
            
            if (mesh.vertices.empty() || (mesh.vertices.size() > 0 && mesh.vertices[0].normal == Vec3::zero())) {
                compute_normals(mesh);
            }
            
            model.meshes.push_back(mesh);
            model.loaded = true;
        }
        
        compute_model_bounds(model);
        
        return model.loaded;
    }
    
    static void compute_bounds(ObjMesh& mesh) {
        if (mesh.vertices.empty()) {
            mesh.bounds = Aabb::empty();
            return;
        }
        
        mesh.bounds.min = mesh.vertices[0].position;
        mesh.bounds.max = mesh.vertices[0].position;
        
        for (const auto& v : mesh.vertices) {
            mesh.bounds.min = Vec3(
                std::min(mesh.bounds.min.x, v.position.x),
                std::min(mesh.bounds.min.y, v.position.y),
                std::min(mesh.bounds.min.z, v.position.z)
            );
            mesh.bounds.max = Vec3(
                std::max(mesh.bounds.max.x, v.position.x),
                std::max(mesh.bounds.max.y, v.position.y),
                std::max(mesh.bounds.max.z, v.position.z)
            );
        }
    }
    
    static void compute_model_bounds(ObjModel& model) {
        if (model.meshes.empty()) {
            model.bounds = Aabb::empty();
            return;
        }
        
        model.bounds = model.meshes[0].bounds;
        for (size_t i = 1; i < model.meshes.size(); i++) {
            model.bounds = model.bounds.merge(model.meshes[i].bounds);
        }
    }
    
    static void compute_normals(ObjMesh& mesh) {
        for (auto& v : mesh.vertices) {
            v.normal = Vec3::zero();
        }
        
        for (size_t i = 0; i < mesh.indices.size(); i += 3) {
            if (i + 2 >= mesh.indices.size()) break;
            
            const auto& v0 = mesh.vertices[mesh.indices[i]];
            const auto& v1 = mesh.vertices[mesh.indices[i + 1]];
            const auto& v2 = mesh.vertices[mesh.indices[i + 2]];
            
            Vec3 edge1 = v1.position - v0.position;
            Vec3 edge2 = v2.position - v0.position;
            Vec3 normal = edge1.cross(edge2).normalized();
            
            mesh.vertices[mesh.indices[i]].normal += normal;
            mesh.vertices[mesh.indices[i + 1]].normal += normal;
            mesh.vertices[mesh.indices[i + 2]].normal += normal;
        }
        
        for (auto& v : mesh.vertices) {
            v.normal = v.normal.normalized();
        }
    }

private:
    void add_face_vertices(ObjMesh& mesh, const ObjFace::Index& i0, 
                          const ObjFace::Index& i1, const ObjFace::Index& i2,
                          const std::vector<Vec3>& positions,
                          const std::vector<Vec3>& normals,
                          const std::vector<Vec2>& texcoords) {
        auto add_vertex = [&](const ObjFace::Index& idx) {
            ObjVertex v;
            if (idx.position >= 0 && idx.position < (int)positions.size()) {
                v.position = positions[idx.position];
            }
            if (idx.normal >= 0 && idx.normal < (int)normals.size()) {
                v.normal = normals[idx.normal];
            } else {
                v.normal = Vec3::up();
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
};

} // namespace litt
