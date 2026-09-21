#include "litt_render_pass.h"

#include <algorithm>
#include <cmath>

namespace litt {
ShadowPass::ShadowPass():RenderPass(RenderPassType::SHADOW){name_="shadow";}
void ShadowPass::execute(){if(!enabled_)return;if(shadow_map_)shadow_map_->bind();run_callback();if(shadow_map_)shadow_map_->unbind();}
GeometryPass::GeometryPass():RenderPass(RenderPassType::GEOMETRY){name_="geometry";}
void GeometryPass::execute(){if(!enabled_)return;if(render_target_)render_target_->bind();run_callback();if(render_target_)render_target_->unbind();}
LightingPass::LightingPass():RenderPass(RenderPassType::LIGHTING){name_="lighting";}
void LightingPass::execute(){
    if(!enabled_)return;
    if(output_target_)output_target_->bind();
    if(gbuffer_&&output_target_&&gbuffer_->get_width()==output_target_->get_width()&&gbuffer_->get_height()==output_target_->get_height())
        output_target_->mutable_color_pixels()=gbuffer_->color_pixels();
    run_callback();
    if(output_target_)output_target_->unbind();
}
PostProcessPass::PostProcessPass():RenderPass(RenderPassType::POST_PROCESS){name_="post_process";}
void PostProcessPass::execute(){
    if(!enabled_)return;
    if(output_target_)output_target_->bind();
    RenderTarget* source=render_target_detail::find(input_texture_);
    if(output_target_&&source){
        if(source&&source->get_width()==output_target_->get_width()&&source->get_height()==output_target_->get_height()){
            std::vector<uint8_t> pixels=source->color_pixels();
            const uint32_t width=output_target_->get_width(),height=output_target_->get_height();
            if(tone_mapping_enabled_){
                const float exposure=std::isfinite(exposure_)?std::clamp(exposure_,0.0f,32.0f):1.0f;
                for(size_t i=0;i+3<pixels.size();i+=4)for(int ch=0;ch<3;++ch){
                    const float linear=static_cast<float>(pixels[i+ch])/255.0f;
                    const float mapped=1.0f-std::exp(-linear*exposure);
                    pixels[i+ch]=static_cast<uint8_t>(std::clamp(mapped,0.0f,1.0f)*255.0f+0.5f);
                }
            }
            if(bloom_enabled_&&width>2&&height>2){
                const std::vector<uint8_t> original=pixels;
                for(uint32_t y=1;y+1<height;++y)for(uint32_t x=1;x+1<width;++x){
                    const size_t center=(static_cast<size_t>(y)*width+x)*4u;
                    const float lum=(original[center]+original[center+1]+original[center+2])/(3.0f*255.0f);
                    if(lum<0.75f)continue;
                    for(int oy=-1;oy<=1;++oy)for(int ox=-1;ox<=1;++ox){
                        const size_t idx=(static_cast<size_t>(static_cast<int>(y)+oy)*width+static_cast<size_t>(static_cast<int>(x)+ox))*4u;
                        for(int ch=0;ch<3;++ch)pixels[idx+ch]=static_cast<uint8_t>(std::min(255u,static_cast<unsigned>(pixels[idx+ch])+static_cast<unsigned>(original[center+ch]/18u)));
                    }
                }
            }
            if(fxaa_enabled_&&width>2&&height>2){
                const std::vector<uint8_t> original=pixels;
                for(uint32_t y=1;y+1<height;++y)for(uint32_t x=1;x+1<width;++x){
                    const size_t idx=(static_cast<size_t>(y)*width+x)*4u;
                    int min_l=765,max_l=0;int lum[5];const int offsets[5][2]={{0,0},{-1,0},{1,0},{0,-1},{0,1}};
                    for(int k=0;k<5;++k){const size_t q=(static_cast<size_t>(static_cast<int>(y)+offsets[k][1])*width+static_cast<size_t>(static_cast<int>(x)+offsets[k][0]))*4u;lum[k]=original[q]+original[q+1]+original[q+2];min_l=std::min(min_l,lum[k]);max_l=std::max(max_l,lum[k]);}
                    if(max_l-min_l>96)for(int ch=0;ch<3;++ch){unsigned sum=0;for(int k=0;k<5;++k){const size_t q=(static_cast<size_t>(static_cast<int>(y)+offsets[k][1])*width+static_cast<size_t>(static_cast<int>(x)+offsets[k][0]))*4u;sum+=original[q+ch];}pixels[idx+ch]=static_cast<uint8_t>(sum/5u);}
                }
            }
            output_target_->mutable_color_pixels()=std::move(pixels);
        }
    }
    run_callback();
    if(output_target_)output_target_->unbind();
}
UIPass::UIPass():RenderPass(RenderPassType::UI){name_="ui";}
void UIPass::execute(){if(enabled_)run_callback();}

RenderPipeline::RenderPipeline():width_(0),height_(0){}
RenderPipeline::~RenderPipeline()=default;
void RenderPipeline::initialize(uint32_t width,uint32_t height){if(!render_target_detail::valid_dimensions(width,height))return;width_=width;height_=height;render_targets_.clear();create_render_target("main",width,height);}
void RenderPipeline::add_pass(std::unique_ptr<RenderPass> pass){if(!pass)return;remove_pass(pass->get_name());passes_.push_back(std::move(pass));}
void RenderPipeline::remove_pass(const std::string& name){passes_.erase(std::remove_if(passes_.begin(),passes_.end(),[&](const std::unique_ptr<RenderPass>& p){return p&&p->get_name()==name;}),passes_.end());}
RenderPass* RenderPipeline::get_pass(const std::string& name){for(auto& p:passes_)if(p&&p->get_name()==name)return p.get();return nullptr;}
void RenderPipeline::execute(){for(auto& p:passes_)if(p&&p->is_enabled())p->execute();}
void RenderPipeline::resize(uint32_t width,uint32_t height){if(!valid_dimensions(width,height))return;width_=width;height_=height;for(auto& item:render_targets_)item.second->resize(width,height);}
RenderTarget* RenderPipeline::get_render_target(const std::string& name){auto it=render_targets_.find(name);return it==render_targets_.end()?nullptr:it->second.get();}
RenderTarget* RenderPipeline::create_render_target(const std::string& name,uint32_t width,uint32_t height){if(name.empty()||render_targets_.count(name)||!render_target_detail::valid_dimensions(width,height))return nullptr;auto target=std::make_unique<RenderTarget>(width,height);RenderTarget* raw=target.get();render_targets_.emplace(name,std::move(target));return raw;}
} // namespace litt
