#include "litt_render_pass.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>

namespace litt {
namespace {
std::atomic<uint32_t> g_render_id{1u};
thread_local const RenderTarget* g_bound_target=nullptr;
uint8_t channel(float v){if(!std::isfinite(v))return 0;return static_cast<uint8_t>(std::clamp(v,0.0f,1.0f)*255.0f+0.5f);}
bool valid_dimensions(uint32_t w,uint32_t h){return w>0&&h>0&&static_cast<uint64_t>(w)*h<=16ull*1024ull*1024ull;}
}
RenderTarget::RenderTarget(uint32_t width,uint32_t height):width_(width),height_(height),color_texture_(0),depth_texture_(0),framebuffer_(0){
    if(valid_dimensions(width_,height_)){color_texture_=g_render_id.fetch_add(1u);depth_texture_=g_render_id.fetch_add(1u);framebuffer_=g_render_id.fetch_add(1u);color_pixels_.resize(static_cast<size_t>(width_)*height_*4u);depth_pixels_.resize(static_cast<size_t>(width_)*height_,1.0f);}
    else width_=height_=0;
}
RenderTarget::~RenderTarget(){if(g_bound_target==this)g_bound_target=nullptr;}
void RenderTarget::bind() const {if(framebuffer_)g_bound_target=this;}
void RenderTarget::unbind() const {if(g_bound_target==this)g_bound_target=nullptr;}
void RenderTarget::resize(uint32_t width,uint32_t height){if(!valid_dimensions(width,height))return;width_=width;height_=height;color_pixels_.assign(static_cast<size_t>(width_)*height_*4u,0);depth_pixels_.assign(static_cast<size_t>(width_)*height_,1.0f);}
void RenderTarget::clear(const Vec4& color){const uint8_t r=channel(color.x),g=channel(color.y),b=channel(color.z),a=channel(color.w);for(size_t i=0;i<color_pixels_.size();i+=4){color_pixels_[i]=r;color_pixels_[i+1]=g;color_pixels_[i+2]=b;color_pixels_[i+3]=a;}std::fill(depth_pixels_.begin(),depth_pixels_.end(),1.0f);}

ShadowPass::ShadowPass():RenderPass(RenderPassType::SHADOW){name_="shadow";}
void ShadowPass::execute(){if(!enabled_)return;if(shadow_map_)shadow_map_->bind();run_callback();if(shadow_map_)shadow_map_->unbind();}
GeometryPass::GeometryPass():RenderPass(RenderPassType::GEOMETRY){name_="geometry";}
void GeometryPass::execute(){if(!enabled_)return;if(render_target_)render_target_->bind();run_callback();if(render_target_)render_target_->unbind();}
LightingPass::LightingPass():RenderPass(RenderPassType::LIGHTING){name_="lighting";}
void LightingPass::execute(){if(!enabled_)return;if(output_target_)output_target_->bind();run_callback();if(output_target_)output_target_->unbind();}
PostProcessPass::PostProcessPass():RenderPass(RenderPassType::POST_PROCESS){name_="post_process";}
void PostProcessPass::execute(){if(!enabled_)return;if(output_target_)output_target_->bind();run_callback();if(output_target_)output_target_->unbind();}
UIPass::UIPass():RenderPass(RenderPassType::UI){name_="ui";}
void UIPass::execute(){if(enabled_)run_callback();}

RenderPipeline::RenderPipeline():width_(0),height_(0){}
RenderPipeline::~RenderPipeline()=default;
void RenderPipeline::initialize(uint32_t width,uint32_t height){if(!valid_dimensions(width,height))return;width_=width;height_=height;render_targets_.clear();create_render_target("main",width,height);}
void RenderPipeline::add_pass(std::unique_ptr<RenderPass> pass){if(!pass)return;remove_pass(pass->get_name());passes_.push_back(std::move(pass));}
void RenderPipeline::remove_pass(const std::string& name){passes_.erase(std::remove_if(passes_.begin(),passes_.end(),[&](const std::unique_ptr<RenderPass>& p){return p&&p->get_name()==name;}),passes_.end());}
RenderPass* RenderPipeline::get_pass(const std::string& name){for(auto& p:passes_)if(p&&p->get_name()==name)return p.get();return nullptr;}
void RenderPipeline::execute(){for(auto& p:passes_)if(p&&p->is_enabled())p->execute();}
void RenderPipeline::resize(uint32_t width,uint32_t height){if(!valid_dimensions(width,height))return;width_=width;height_=height;for(auto& item:render_targets_)item.second->resize(width,height);}
RenderTarget* RenderPipeline::get_render_target(const std::string& name){auto it=render_targets_.find(name);return it==render_targets_.end()?nullptr:it->second.get();}
RenderTarget* RenderPipeline::create_render_target(const std::string& name,uint32_t width,uint32_t height){if(name.empty()||render_targets_.count(name)||!valid_dimensions(width,height))return nullptr;auto target=std::make_unique<RenderTarget>(width,height);RenderTarget* raw=target.get();render_targets_.emplace(name,std::move(target));return raw;}
} // namespace litt
