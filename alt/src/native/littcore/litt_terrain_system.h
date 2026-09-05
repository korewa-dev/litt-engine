// Phase 6: Advanced Features - Terrain System

#pragma once

#include "litt_math.h"
#include <vector>
#include <memory>

namespace litt {

// Terrain chunk
class TerrainChunk {
public:
    TerrainChunk(uint32_t size, float resolution);
    ~TerrainChunk();
    
    // Generate heightmap from noise
    void generate_heightmap(uint32_t seed);
    
    // Set height at point
    void set_height(uint32_t x, uint32_t z, float height);
    
    // Get height at point
    float get_height(uint32_t x, uint32_t z) const;
    
    // Get height at world position (interpolated)
    float get_height_at(float x, float z) const;
    
    // Get normal at point
    Vec3 get_normal(uint32_t x, uint32_t z) const;
    
    // Get size
    uint32_t get_size() const { return size_; }
    
    // Get resolution
    float get_resolution() const { return resolution_; }
    
    // Get vertex count
    uint32_t get_vertex_count() const { return size_ * size_; }
    
    // Get index count
    uint32_t get_index_count() const { return (size_ - 1) * (size_ - 1) * 6; }
    
    // Get heightmap data
    const std::vector<float>& get_heightmap() const { return heightmap_; }
    
    // Set heightmap from external data
    void set_heightmap(const std::vector<float>& heights);
    
    // Calculate normals
    void calculate_normals();
    
    // Get normals data
    const std::vector<Vec3>& get_normals() const { return normals_; }

private:
    uint32_t size_;
    float resolution_;
    std::vector<float> heightmap_;
    std::vector<Vec3> normals_;
};

// Terrain system
class TerrainSystem {
public:
    static TerrainSystem& get_instance() {
        static TerrainSystem instance;
        return instance;
    }
    
    // Initialize terrain
    void initialize(uint32_t chunk_size, float resolution, uint32_t chunks_x, uint32_t chunks_z);
    
    // Shutdown terrain
    void shutdown();
    
    // Get chunk
    TerrainChunk* get_chunk(uint32_t x, uint32_t z);
    
    // Get chunk at world position
    TerrainChunk* get_chunk_at(float x, float z);
    
    // Get height at world position
    float get_height_at(float x, float z);
    
    // Get normal at world position
    Vec3 get_normal_at(float x, float z);
    
    // Set height at world position
    void set_height_at(float x, float z, float height);
    
    // Generate terrain
    void generate(uint32_t seed);
    
    // Get terrain size
    uint32_t get_chunks_x() const { return chunks_x_; }
    uint32_t get_chunks_z() const { return chunks_z_; }
    uint32_t get_chunk_size() const { return chunk_size_; }
    float get_resolution() const { return resolution_; }
    
    // Get total vertex count
    uint32_t get_total_vertex_count() const;

private:
    TerrainSystem() = default;
    uint32_t chunk_size_;
    float resolution_;
    uint32_t chunks_x_;
    uint32_t chunks_z_;
    std::vector<std::unique_ptr<TerrainChunk>> chunks_;
};

// Noise generator for terrain
class NoiseGenerator {
public:
    // Generate Perlin noise
    static float perlin(float x, float y, float z);
    
    // Generate fractal noise
    static float fractal(float x, float y, float z, uint32_t octaves, float persistence);
    
    // Generate ridged noise
    static float ridged(float x, float y, float z, uint32_t octaves);
    
    // Set seed
    static void set_seed(uint32_t seed);
    
    // Get seed
    static uint32_t get_seed() { return seed_; }

private:
    static uint32_t seed_;
    static std::vector<uint8_t> permutation_;
    
    // Initialize permutation table
    static void init_permutation();
    
    // Fade function
    static float fade(float t);
    
    // Lerp function
    static float lerp(float a, float b, float t);
    
    // Gradient function
    static float grad(uint32_t hash, float x, float y, float z);
};

} // namespace litt
