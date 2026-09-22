#include "litt_lighting.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace litt {
namespace {
float saturate(float v) { return std::clamp(v, 0.0f, 1.0f); }
Vec3 component_mul(const Vec3& a,const Vec3& b){return Vec3{a.x*b.x,a.y*b.y,a.z*b.z};}
}

float PBRLighting::distribution_ggx(const Vec3& normal,const Vec3& half_vec,float roughness){
    roughness=std::clamp(roughness,0.04f,1.0f);
    const float a=roughness*roughness,a2=a*a;
    const float ndh=saturate(dot(normal.normalized(),half_vec.normalized()));
    const float denom=ndh*ndh*(a2-1.0f)+1.0f;
    return a2/(MATH_PI*denom*denom+1e-6f);
}
float PBRLighting::geometry_schlick_ggx(float ndv,float roughness){
    roughness=std::clamp(roughness,0.04f,1.0f);ndv=saturate(ndv);
    const float r=roughness+1.0f,k=(r*r)/8.0f;
    return ndv/(ndv*(1.0f-k)+k+1e-6f);
}
float PBRLighting::geometry_smith(const Vec3& normal,const Vec3& view_dir,const Vec3& light_dir,float roughness){
    const Vec3 n=normal.normalized();
    return geometry_schlick_ggx(saturate(dot(n,view_dir.normalized())),roughness)*
           geometry_schlick_ggx(saturate(dot(n,light_dir.normalized())),roughness);
}
Vec3 PBRLighting::fresnel_schlick(float cos_theta,Vec3 f0){
    const float t=std::pow(1.0f-saturate(cos_theta),5.0f);
    return f0+(Vec3::one()-f0)*t;
}
Vec3 PBRLighting::fresnel_schlick_roughness(float cos_theta,Vec3 f0,float roughness){
    roughness=std::clamp(roughness,0.0f,1.0f);
    const Vec3 maxv{std::max(1.0f-roughness,f0.x),std::max(1.0f-roughness,f0.y),std::max(1.0f-roughness,f0.z)};
    const float t=std::pow(1.0f-saturate(cos_theta),5.0f);
    return f0+(maxv-f0)*t;
}
Vec3 PBRLighting::calculate_direct_light(const Light& light,const Vec3& world_pos,const Vec3& normal,
                                         const Vec3& view_dir,const Vec3& albedo,float metallic,float roughness){
    const Vec3 n=normal.normalized(),v=view_dir.normalized();
    Vec3 l=Vec3::zero();float attenuation=1.0f;
    if(light.type==LightType::DIRECTIONAL){
        l=(-light.direction).normalized();
    }else{
        const Vec3 delta=light.position-world_pos;const float distance=delta.length();
        if(!(distance>1e-5f)||!std::isfinite(distance)||light.range<=0.0f||distance>light.range)return Vec3::zero();
        l=delta/distance;const float falloff=1.0f-distance/light.range;attenuation=falloff*falloff;
        if(light.type==LightType::SPOT){
            const Vec3 forward=light.direction.normalized();
            const float cosine=dot(-l,forward);
            const float outer=std::cos(std::clamp(light.spot_angle,1.0f,179.0f)*0.5f*MATH_PI/180.0f);
            const float inner=std::cos(std::clamp(light.spot_angle*(1.0f-std::clamp(light.spot_penumbra,0.0f,0.99f)),1.0f,179.0f)*0.5f*MATH_PI/180.0f);
            attenuation*=saturate((cosine-outer)/std::max(inner-outer,1e-5f));
        }
    }
    const float ndl=saturate(dot(n,l));if(ndl<=0.0f)return Vec3::zero();
    const Vec3 h=(l+v).normalized();metallic=saturate(metallic);roughness=std::clamp(roughness,0.04f,1.0f);
    Vec3 f0{0.04f,0.04f,0.04f};f0=f0+(albedo-f0)*metallic;
    const float d=distribution_ggx(n,h,roughness),g=geometry_smith(n,v,l,roughness);
    const Vec3 f=fresnel_schlick(saturate(dot(h,v)),f0);
    const float denom=std::max(4.0f*saturate(dot(n,v))*ndl,1e-4f);
    const Vec3 spec=f*(d*g/denom);
    const Vec3 kd=(Vec3::one()-f)*(1.0f-metallic);
    const Vec3 diffuse=component_mul(kd,albedo)*(1.0f/MATH_PI);
    const Vec3 radiance=light.color*(std::max(0.0f,light.intensity)*attenuation);
    return component_mul(diffuse+spec,radiance)*ndl;
}
Vec3 PBRLighting::calculate_irradiance(const Vec3& normal,const Vec3& irradiance_map){
    const float sky=0.35f+0.65f*saturate(normal.normalized().y*0.5f+0.5f);
    return irradiance_map*sky;
}
Vec3 PBRLighting::ibl_specular(const Vec3& reflect_dir,float roughness,const Vec3& prefiltered_map){
    const float horizon=0.5f+0.5f*saturate(reflect_dir.normalized().y*0.5f+0.5f);
    const float retention=1.0f-0.75f*saturate(roughness);
    return prefiltered_map*(horizon*retention);
}
Vec3 PBRLighting::ibl_diffuse(const Vec3& normal,const Vec3& irradiance){
    return calculate_irradiance(normal,irradiance)*(1.0f/MATH_PI);
}

ShadowMap::ShadowMap(uint32_t size):size_(std::clamp(size,1u,8192u)),light_view_proj_(Mat4::identity()),
    depth_data_(static_cast<size_t>(size_)*size_,1.0f){}
ShadowMap::~ShadowMap()=default;
void ShadowMap::begin_pass(const Vec3& light_pos,const Vec3& light_dir,float fov){
    std::fill(depth_data_.begin(),depth_data_.end(),1.0f);
    Vec3 dir=light_dir.normalized();if(dir.length()<=MATH_EPS)dir=Vec3{0,-1,0};
    Vec3 up=std::fabs(dot(dir,Vec3::up()))>0.99f?Vec3::unit_x():Vec3::up();
    const Mat4 view=Mat4::look_at(light_pos,light_pos+dir,up);
    const Mat4 projection=Mat4::perspective(std::clamp(fov,1.0f,179.0f),1.0f,0.05f,1000.0f);
    light_view_proj_=projection*view;
}
void ShadowMap::end_pass(){}
float ShadowMap::sample(float x,float y) const{
    if(!std::isfinite(x)||!std::isfinite(y)||x<0.0f||y<0.0f||x>=1.0f||y>=1.0f)return 1.0f;
    const uint32_t ix=std::min(static_cast<uint32_t>(x*size_),size_-1),iy=std::min(static_cast<uint32_t>(y*size_),size_-1);
    return depth_data_[static_cast<size_t>(iy)*size_+ix];
}

uint32_t LightManager::add_light(const Light& light){
    if(lights_.size()>=4096u||next_id_==0u)return 0u;
    auto copy=std::make_unique<Light>(light);copy->id=next_id_++;
    const uint32_t id=copy->id;lights_.push_back(std::move(copy));return id;
}
void LightManager::remove_light(uint32_t id){
    lights_.erase(std::remove_if(lights_.begin(),lights_.end(),[id](const std::unique_ptr<Light>& light){return light&&light->id==id;}),lights_.end());
}
Light* LightManager::get_light(uint32_t id){for(auto& light:lights_)if(light&&light->id==id)return light.get();return nullptr;}
std::vector<Light*> LightManager::get_lights_affecting_point(const Vec3& point,float radius) const{
    std::vector<Light*> result;if(!std::isfinite(radius)||radius<0.0f)return result;
    for(const auto& light:lights_){if(!light)continue;if(light->type==LightType::DIRECTIONAL||(light->position-point).length()<=radius+std::max(0.0f,light->range))result.push_back(light.get());}
    return result;
}
void LightManager::clear(){lights_.clear();}
} // namespace litt
