#include "litt_terrain_system.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numeric>
#include <random>

namespace litt {

uint32_t NoiseGenerator::seed_=0u;
std::vector<uint8_t> NoiseGenerator::permutation_;

TerrainChunk::TerrainChunk(uint32_t size,float resolution)
    : size_(std::clamp(size,2u,2048u)),resolution_(std::isfinite(resolution)&&resolution>0.0f?resolution:1.0f),
      heightmap_(static_cast<size_t>(size_)*size_,0.0f),normals_(static_cast<size_t>(size_)*size_,Vec3::up()) {}
TerrainChunk::~TerrainChunk()=default;
void TerrainChunk::generate_heightmap(uint32_t seed){
    NoiseGenerator::set_seed(seed);
    for(uint32_t z=0;z<size_;++z) for(uint32_t x=0;x<size_;++x)
        heightmap_[static_cast<size_t>(z)*size_+x]=NoiseGenerator::fractal(x*0.04f,0.0f,z*0.04f,5u,0.5f)*8.0f;
    calculate_normals();
}
void TerrainChunk::set_height(uint32_t x,uint32_t z,float height){
    if(x>=size_||z>=size_||!std::isfinite(height)) return;
    heightmap_[static_cast<size_t>(z)*size_+x]=height;
}
float TerrainChunk::get_height(uint32_t x,uint32_t z) const {
    if(x>=size_||z>=size_) return 0.0f;
    return heightmap_[static_cast<size_t>(z)*size_+x];
}
float TerrainChunk::get_height_at(float x,float z) const {
    if(!std::isfinite(x)||!std::isfinite(z)) return 0.0f;
    const float gx=x/resolution_, gz=z/resolution_;
    if(gx<0.0f||gz<0.0f||gx>static_cast<float>(size_-1)||gz>static_cast<float>(size_-1)) return 0.0f;
    const uint32_t x0=static_cast<uint32_t>(std::floor(gx)), z0=static_cast<uint32_t>(std::floor(gz));
    const uint32_t x1=std::min(x0+1,size_-1), z1=std::min(z0+1,size_-1);
    const float tx=gx-x0,tz=gz-z0;
    const float a=get_height(x0,z0)*(1.0f-tx)+get_height(x1,z0)*tx;
    const float b=get_height(x0,z1)*(1.0f-tx)+get_height(x1,z1)*tx;
    return a*(1.0f-tz)+b*tz;
}
Vec3 TerrainChunk::get_normal(uint32_t x,uint32_t z) const {
    if(x>=size_||z>=size_) return Vec3::up();
    return normals_[static_cast<size_t>(z)*size_+x];
}
void TerrainChunk::set_heightmap(const std::vector<float>& heights){
    if(heights.size()!=heightmap_.size()) return;
    for(float h:heights) if(!std::isfinite(h)) return;
    heightmap_=heights; calculate_normals();
}
void TerrainChunk::calculate_normals(){
    for(uint32_t z=0;z<size_;++z) for(uint32_t x=0;x<size_;++x){
        const float l=get_height(x>0?x-1:x,z), r=get_height(std::min(x+1,size_-1),z);
        const float d=get_height(x,z>0?z-1:z), u=get_height(x,std::min(z+1,size_-1));
        normals_[static_cast<size_t>(z)*size_+x]=Vec3{l-r,2.0f*resolution_,d-u}.normalized();
    }
}

void TerrainSystem::initialize(uint32_t chunk_size,float resolution,uint32_t chunks_x,uint32_t chunks_z){
    shutdown();
    if(chunk_size<2||chunk_size>2048||!std::isfinite(resolution)||resolution<=0.0f||
       chunks_x==0||chunks_z==0||static_cast<uint64_t>(chunks_x)*chunks_z>4096u) return;
    chunk_size_=chunk_size; resolution_=resolution; chunks_x_=chunks_x; chunks_z_=chunks_z;
    chunks_.reserve(static_cast<size_t>(chunks_x_)*chunks_z_);
    for(uint32_t z=0;z<chunks_z_;++z) for(uint32_t x=0;x<chunks_x_;++x)
        chunks_.push_back(std::make_unique<TerrainChunk>(chunk_size_,resolution_));
}
void TerrainSystem::shutdown(){chunks_.clear();chunk_size_=0;resolution_=0.0f;chunks_x_=chunks_z_=0;}
TerrainChunk* TerrainSystem::get_chunk(uint32_t x,uint32_t z){
    if(x>=chunks_x_||z>=chunks_z_) return nullptr;
    return chunks_[static_cast<size_t>(z)*chunks_x_+x].get();
}
TerrainChunk* TerrainSystem::get_chunk_at(float x,float z){
    if(!std::isfinite(x)||!std::isfinite(z)||x<0.0f||z<0.0f||chunk_size_<2||resolution_<=0.0f) return nullptr;
    const float span=static_cast<float>(chunk_size_-1)*resolution_;
    if(!(span>0.0f)) return nullptr;
    return get_chunk(static_cast<uint32_t>(x/span),static_cast<uint32_t>(z/span));
}
float TerrainSystem::get_height_at(float x,float z){
    TerrainChunk* chunk=get_chunk_at(x,z); if(!chunk) return 0.0f;
    const float span=static_cast<float>(chunk_size_-1)*resolution_;
    return chunk->get_height_at(std::fmod(x,span),std::fmod(z,span));
}
Vec3 TerrainSystem::get_normal_at(float x,float z){
    TerrainChunk* chunk=get_chunk_at(x,z); if(!chunk) return Vec3::up();
    const float span=static_cast<float>(chunk_size_-1)*resolution_;
    const float lx=std::fmod(x,span)/resolution_, lz=std::fmod(z,span)/resolution_;
    return chunk->get_normal(std::min(static_cast<uint32_t>(lx),chunk_size_-1),std::min(static_cast<uint32_t>(lz),chunk_size_-1));
}
void TerrainSystem::set_height_at(float x,float z,float height){
    TerrainChunk* chunk=get_chunk_at(x,z); if(!chunk||!std::isfinite(height)) return;
    const float span=static_cast<float>(chunk_size_-1)*resolution_;
    const uint32_t lx=std::min(static_cast<uint32_t>(std::fmod(x,span)/resolution_),chunk_size_-1);
    const uint32_t lz=std::min(static_cast<uint32_t>(std::fmod(z,span)/resolution_),chunk_size_-1);
    chunk->set_height(lx,lz,height); chunk->calculate_normals();
}
void TerrainSystem::generate(uint32_t seed){
    for(size_t i=0;i<chunks_.size();++i) chunks_[i]->generate_heightmap(seed+static_cast<uint32_t>(i)*0x9e3779b9u);
}
uint32_t TerrainSystem::get_total_vertex_count() const {
    const uint64_t total=static_cast<uint64_t>(chunk_size_)*chunk_size_*chunks_.size();
    return total>std::numeric_limits<uint32_t>::max()?std::numeric_limits<uint32_t>::max():static_cast<uint32_t>(total);
}

void NoiseGenerator::set_seed(uint32_t seed){seed_=seed;init_permutation();}
void NoiseGenerator::init_permutation(){
    std::array<uint8_t,256> values{}; std::iota(values.begin(),values.end(),uint8_t{0});
    std::mt19937 rng(seed_); std::shuffle(values.begin(),values.end(),rng);
    permutation_.resize(512); for(size_t i=0;i<512;++i) permutation_[i]=values[i&255u];
}
float NoiseGenerator::fade(float t){return t*t*t*(t*(t*6.0f-15.0f)+10.0f);}
float NoiseGenerator::lerp(float a,float b,float t){return a+(b-a)*t;}
float NoiseGenerator::grad(uint32_t hash,float x,float y,float z){
    const uint32_t h=hash&15u; const float u=h<8u?x:y; const float v=h<4u?y:(h==12u||h==14u?x:z);
    return ((h&1u)?-u:u)+((h&2u)?-v:v);
}
float NoiseGenerator::perlin(float x,float y,float z){
    if(permutation_.empty()) init_permutation();
    const int xi=static_cast<int>(std::floor(x))&255, yi=static_cast<int>(std::floor(y))&255, zi=static_cast<int>(std::floor(z))&255;
    x-=std::floor(x);y-=std::floor(y);z-=std::floor(z);
    const float u=fade(x),v=fade(y),w=fade(z);
    const auto p=[&](int i)->uint8_t{return permutation_[static_cast<size_t>(i)&511u];};
    const int A=p(xi)+yi,AA=p(A)+zi,AB=p(A+1)+zi,B=p(xi+1)+yi,BA=p(B)+zi,BB=p(B+1)+zi;
    return lerp(lerp(lerp(grad(p(AA),x,y,z),grad(p(BA),x-1,y,z),u),
                     lerp(grad(p(AB),x,y-1,z),grad(p(BB),x-1,y-1,z),u),v),
                lerp(lerp(grad(p(AA+1),x,y,z-1),grad(p(BA+1),x-1,y,z-1),u),
                     lerp(grad(p(AB+1),x,y-1,z-1),grad(p(BB+1),x-1,y-1,z-1),u),v),w);
}
float NoiseGenerator::fractal(float x,float y,float z,uint32_t octaves,float persistence){
    if(octaves==0||octaves>16||!std::isfinite(persistence)||persistence<0.0f||persistence>1.0f) return 0.0f;
    float sum=0.0f,amp=1.0f,freq=1.0f,norm=0.0f;
    for(uint32_t i=0;i<octaves;++i){sum+=perlin(x*freq,y*freq,z*freq)*amp;norm+=amp;amp*=persistence;freq*=2.0f;}
    return norm>0.0f?sum/norm:0.0f;
}
float NoiseGenerator::ridged(float x,float y,float z,uint32_t octaves){
    if(octaves==0||octaves>16) return 0.0f;
    float sum=0.0f,amp=0.5f,freq=1.0f,norm=0.0f;
    for(uint32_t i=0;i<octaves;++i){sum+=(1.0f-std::fabs(perlin(x*freq,y*freq,z*freq)))*amp;norm+=amp;amp*=0.5f;freq*=2.0f;}
    return norm>0.0f?sum/norm:0.0f;
}
} // namespace litt
