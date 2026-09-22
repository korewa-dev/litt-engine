#include "litt_culling.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace litt {
namespace {
Frustum::Plane make_plane(float a, float b, float c, float d) {
    const float len = std::sqrt(a*a + b*b + c*c);
    if (!(len > 0.0f) || !std::isfinite(len)) return {Vec3::zero(), -std::numeric_limits<float>::infinity()};
    return {Vec3{a/len,b/len,c/len}, d/len};
}
bool project_point(const Mat4& matrix, const Vec3& point, uint32_t width, uint32_t height,
                   int& x, int& y, float& depth) {
    const Vec4 clip = matrix * Vec4{point.x,point.y,point.z,1.0f};
    if (!(clip.w > 1e-6f)) return false;
    const float nx=clip.x/clip.w, ny=clip.y/clip.w, nz=clip.z/clip.w;
    if (!std::isfinite(nx)||!std::isfinite(ny)||!std::isfinite(nz) ||
        nx < -1.0f || nx > 1.0f || ny < -1.0f || ny > 1.0f || nz < -1.0f || nz > 1.0f) return false;
    x=std::clamp(static_cast<int>((nx*0.5f+0.5f)*static_cast<float>(width)),0,static_cast<int>(width)-1);
    y=std::clamp(static_cast<int>((-ny*0.5f+0.5f)*static_cast<float>(height)),0,static_cast<int>(height)-1);
    depth=nz*0.5f+0.5f;
    return true;
}
}

Frustum Frustum::from_matrix(const Mat4& m) {
    Frustum f{};
    // Rows for column-major storage.
    const float r0[4]={m.m[0],m.m[4],m.m[8],m.m[12]};
    const float r1[4]={m.m[1],m.m[5],m.m[9],m.m[13]};
    const float r2[4]={m.m[2],m.m[6],m.m[10],m.m[14]};
    const float r3[4]={m.m[3],m.m[7],m.m[11],m.m[15]};
    f.planes[0]=make_plane(r3[0]+r2[0],r3[1]+r2[1],r3[2]+r2[2],r3[3]+r2[3]);
    f.planes[1]=make_plane(r3[0]-r2[0],r3[1]-r2[1],r3[2]-r2[2],r3[3]-r2[3]);
    f.planes[2]=make_plane(r3[0]+r0[0],r3[1]+r0[1],r3[2]+r0[2],r3[3]+r0[3]);
    f.planes[3]=make_plane(r3[0]-r0[0],r3[1]-r0[1],r3[2]-r0[2],r3[3]-r0[3]);
    f.planes[4]=make_plane(r3[0]-r1[0],r3[1]-r1[1],r3[2]-r1[2],r3[3]-r1[3]);
    f.planes[5]=make_plane(r3[0]+r1[0],r3[1]+r1[1],r3[2]+r1[2],r3[3]+r1[3]);
    return f;
}

bool Frustum::contains_point(const Vec3& point) const {
    for (const auto& plane : planes) if (plane.signed_distance(point) < 0.0f) return false;
    return true;
}
bool Frustum::contains_sphere(const Vec3& center, float radius) const {
    if (!std::isfinite(radius) || radius < 0.0f) return false;
    for (const auto& plane : planes) if (plane.signed_distance(center) < radius) return false;
    return true;
}
bool Frustum::contains_aabb(const Aabb& aabb) const {
    const Vec3 corners[8]={
        {aabb.min.x,aabb.min.y,aabb.min.z},{aabb.max.x,aabb.min.y,aabb.min.z},
        {aabb.min.x,aabb.max.y,aabb.min.z},{aabb.max.x,aabb.max.y,aabb.min.z},
        {aabb.min.x,aabb.min.y,aabb.max.z},{aabb.max.x,aabb.min.y,aabb.max.z},
        {aabb.min.x,aabb.max.y,aabb.max.z},{aabb.max.x,aabb.max.y,aabb.max.z}
    };
    for (const auto& corner : corners) if (!contains_point(corner)) return false;
    return true;
}
bool Frustum::intersects_aabb(const Aabb& aabb) const {
    const Vec3 corners[8]={
        {aabb.min.x,aabb.min.y,aabb.min.z},{aabb.max.x,aabb.min.y,aabb.min.z},
        {aabb.min.x,aabb.max.y,aabb.min.z},{aabb.max.x,aabb.max.y,aabb.min.z},
        {aabb.min.x,aabb.min.y,aabb.max.z},{aabb.max.x,aabb.min.y,aabb.max.z},
        {aabb.min.x,aabb.max.y,aabb.max.z},{aabb.max.x,aabb.max.y,aabb.max.z}
    };
    for (const auto& plane : planes) {
        bool any_inside=false;
        for (const auto& corner : corners) if (plane.signed_distance(corner) >= 0.0f) { any_inside=true; break; }
        if (!any_inside) return false;
    }
    return true;
}

OcclusionCuller::OcclusionCuller(uint32_t width,uint32_t height)
    : width_(std::clamp(width,1u,4096u)),height_(std::clamp(height,1u,4096u)),
      depth_buffer_(static_cast<size_t>(width_)*height_,1.0f),view_proj_(Mat4::identity()) {}
OcclusionCuller::~OcclusionCuller()=default;
void OcclusionCuller::begin_frame(const Mat4& view_proj) {
    view_proj_=view_proj;
    std::fill(depth_buffer_.begin(),depth_buffer_.end(),1.0f);
}
void OcclusionCuller::render_occluder(const Vec3* vertices,uint32_t count) {
    if (!vertices || count==0 || count>1000000u) return;
    for(uint32_t i=0;i<count;++i){
        int x=0,y=0; float depth=1.0f;
        if(project_point(view_proj_,vertices[i],width_,height_,x,y,depth)){
            const size_t idx=static_cast<size_t>(y)*width_+static_cast<size_t>(x);
            depth_buffer_[idx]=std::min(depth_buffer_[idx],depth);
        }
    }
}
bool OcclusionCuller::is_occluded(const Aabb& aabb) const {
    const Vec3 corners[8]={
        {aabb.min.x,aabb.min.y,aabb.min.z},{aabb.max.x,aabb.min.y,aabb.min.z},
        {aabb.min.x,aabb.max.y,aabb.min.z},{aabb.max.x,aabb.max.y,aabb.min.z},
        {aabb.min.x,aabb.min.y,aabb.max.z},{aabb.max.x,aabb.min.y,aabb.max.z},
        {aabb.min.x,aabb.max.y,aabb.max.z},{aabb.max.x,aabb.max.y,aabb.max.z}
    };
    bool projected=false;
    for(const auto& corner:corners){
        int x=0,y=0; float depth=1.0f;
        if(!project_point(view_proj_,corner,width_,height_,x,y,depth)) continue;
        projected=true;
        const float stored=depth_buffer_[static_cast<size_t>(y)*width_+static_cast<size_t>(x)];
        if(stored >= depth-1e-5f) return false;
    }
    return projected;
}
bool OcclusionCuller::is_occluded_sphere(const Vec3& center,float radius) const {
    if(radius<0.0f || !std::isfinite(radius)) return false;
    return is_occluded(Aabb(center-Vec3(radius,radius,radius),center+Vec3(radius,radius,radius)));
}
void OcclusionCuller::end_frame() {}

void CullingSystem::set_frustum(const Frustum& frustum){frustum_=frustum;}
void CullingSystem::set_occlusion_culler(std::shared_ptr<OcclusionCuller> culler){occlusion_culler_=std::move(culler);}
void CullingSystem::frustum_cull(const std::vector<Aabb>& objects,std::vector<uint32_t>& visible){
    visible.clear();
    for(uint32_t i=0;i<objects.size();++i) if(frustum_.intersects_aabb(objects[i])) visible.push_back(i);
    visible_count_=static_cast<uint32_t>(visible.size());
    culled_count_=static_cast<uint32_t>(objects.size())-visible_count_;
}
void CullingSystem::occlusion_cull(const std::vector<Aabb>& objects,std::vector<uint32_t>& visible){
    visible.clear();
    for(uint32_t i=0;i<objects.size();++i) if(!occlusion_culler_ || !occlusion_culler_->is_occluded(objects[i])) visible.push_back(i);
    visible_count_=static_cast<uint32_t>(visible.size());
    culled_count_=static_cast<uint32_t>(objects.size())-visible_count_;
}
void CullingSystem::cull(const std::vector<Aabb>& objects,std::vector<uint32_t>& visible){
    visible.clear();
    for(uint32_t i=0;i<objects.size();++i){
        if(!frustum_.intersects_aabb(objects[i])) continue;
        if(occlusion_culler_ && occlusion_culler_->is_occluded(objects[i])) continue;
        visible.push_back(i);
    }
    visible_count_=static_cast<uint32_t>(visible.size());
    culled_count_=static_cast<uint32_t>(objects.size())-visible_count_;
}
} // namespace litt
