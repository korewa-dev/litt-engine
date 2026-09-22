#include "litt_texture.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstring>
#include <fstream>
#include <limits>
#include <mutex>
#include <sstream>

namespace litt {
namespace {
struct TextureRecord {
    TextureDesc desc{};
    std::vector<uint8_t> data;
    std::vector<std::vector<uint8_t>> mips;
};
std::atomic<uint32_t> g_next_texture_id{1u};
std::mutex g_texture_mutex;
std::unordered_map<uint32_t,TextureRecord> g_texture_records;
thread_local std::unordered_map<uint32_t,const Texture*> g_bound_textures;

size_t expected_size(const TextureDesc& desc) {
    if(desc.width==0||desc.height==0) return 0;
    const uint64_t pixels=static_cast<uint64_t>(desc.width)*desc.height;
    uint64_t bytes=0;
    switch(desc.format){
        case TextureFormat::RGBA8: bytes=pixels*4u; break;
        case TextureFormat::RGBA16F: bytes=pixels*8u; break;
        case TextureFormat::RGBA32F: bytes=pixels*16u; break;
        case TextureFormat::RGB8: bytes=pixels*3u; break;
        case TextureFormat::RGB16F: bytes=pixels*6u; break;
        case TextureFormat::RGB32F: bytes=pixels*12u; break;
        case TextureFormat::RG8: bytes=pixels*2u; break;
        case TextureFormat::R8: bytes=pixels; break;
        case TextureFormat::DEPTH24_STENCIL8:
        case TextureFormat::DEPTH32: bytes=pixels*4u; break;
        case TextureFormat::BC1: bytes=((desc.width+3u)/4u)*((desc.height+3u)/4u)*8u; break;
        case TextureFormat::BC3:
        case TextureFormat::BC5:
        case TextureFormat::BC7: bytes=((desc.width+3u)/4u)*((desc.height+3u)/4u)*16u; break;
    }
    constexpr uint64_t kMaxTextureBytes=256ull*1024ull*1024ull;
    return bytes==0||bytes>kMaxTextureBytes||bytes>std::numeric_limits<size_t>::max()?0:static_cast<size_t>(bytes);
}
uint32_t channels_8bit(TextureFormat f){
    switch(f){case TextureFormat::RGBA8:return 4;case TextureFormat::RGB8:return 3;case TextureFormat::RG8:return 2;case TextureFormat::R8:return 1;default:return 0;}
}
bool read_ppm(const std::string& path,uint32_t& w,uint32_t& h,std::vector<uint8_t>& rgba){
    std::ifstream in(path,std::ios::binary); if(!in) return false;
    std::string magic; in>>magic; if(magic!="P6") return false;
    auto skip_comments=[&](){while(in>>std::ws && in.peek()=='#'){std::string line;std::getline(in,line);}};
    skip_comments(); uint64_t ww=0,hh=0,maxv=0; in>>ww; skip_comments(); in>>hh; skip_comments(); in>>maxv; in.get();
    if(ww==0||hh==0||ww>8192||hh>8192||maxv!=255||ww*hh>16ull*1024ull*1024ull) return false;
    std::vector<uint8_t> rgb(static_cast<size_t>(ww*hh*3u));
    if(!in.read(reinterpret_cast<char*>(rgb.data()),static_cast<std::streamsize>(rgb.size()))) return false;
    rgba.resize(static_cast<size_t>(ww*hh*4u));
    for(size_t i=0,j=0;i<rgb.size();i+=3,j+=4){rgba[j]=rgb[i];rgba[j+1]=rgb[i+1];rgba[j+2]=rgb[i+2];rgba[j+3]=255;}
    w=static_cast<uint32_t>(ww);h=static_cast<uint32_t>(hh);return true;
}
bool read_tga(const std::string& path,uint32_t& w,uint32_t& h,std::vector<uint8_t>& rgba){
    std::ifstream in(path,std::ios::binary); if(!in) return false;
    uint8_t hdr[18]{}; if(!in.read(reinterpret_cast<char*>(hdr),18)) return false;
    if(hdr[1]!=0||hdr[2]!=2) return false;
    w=static_cast<uint32_t>(hdr[12]|(hdr[13]<<8)); h=static_cast<uint32_t>(hdr[14]|(hdr[15]<<8));
    const uint8_t bpp=hdr[16]; if(w==0||h==0||w>8192||h>8192||(bpp!=24&&bpp!=32)||static_cast<uint64_t>(w)*h>16ull*1024ull*1024ull) return false;
    if(hdr[0]) in.seekg(hdr[0],std::ios::cur);
    const size_t src_bpp=bpp/8u, count=static_cast<size_t>(w)*h;
    std::vector<uint8_t> src(count*src_bpp); if(!in.read(reinterpret_cast<char*>(src.data()),static_cast<std::streamsize>(src.size()))) return false;
    rgba.resize(count*4u); const bool top=(hdr[17]&0x20u)!=0;
    for(uint32_t y=0;y<h;++y){const uint32_t sy=top?y:(h-1u-y);for(uint32_t x=0;x<w;++x){
        const size_t s=(static_cast<size_t>(sy)*w+x)*src_bpp,d=(static_cast<size_t>(y)*w+x)*4u;
        rgba[d]=src[s+2];rgba[d+1]=src[s+1];rgba[d+2]=src[s];rgba[d+3]=src_bpp==4?src[s+3]:255;
    }} return true;
}
}

Texture::Texture(const TextureDesc& desc):desc_(desc){
    const size_t bytes=expected_size(desc_);
    if(!bytes) return;
    uint32_t id=g_next_texture_id.fetch_add(1u);
    if(id==0u) id=g_next_texture_id.fetch_add(1u);
    texture_id_=id;
    TextureRecord record;record.desc=desc_;record.data.resize(bytes);
    std::lock_guard<std::mutex> lock(g_texture_mutex);g_texture_records.emplace(texture_id_,std::move(record));
}
Texture::~Texture(){
    unbind();
    if(texture_id_){std::lock_guard<std::mutex> lock(g_texture_mutex);g_texture_records.erase(texture_id_);}
}
void Texture::bind(uint32_t slot) const {if(texture_id_&&slot<32u)g_bound_textures[slot]=this;}
void Texture::unbind() const {for(auto it=g_bound_textures.begin();it!=g_bound_textures.end();){if(it->second==this)it=g_bound_textures.erase(it);else ++it;}}
const Texture* Texture::bound_texture(uint32_t slot){auto it=g_bound_textures.find(slot);return it==g_bound_textures.end()?nullptr:it->second;}
void Texture::set_data(const void* data,size_t size){
    if(!texture_id_||!data) return; std::lock_guard<std::mutex> lock(g_texture_mutex);
    auto it=g_texture_records.find(texture_id_);if(it==g_texture_records.end()||size!=it->second.data.size())return;
    std::memcpy(it->second.data.data(),data,size);it->second.mips.clear();
}
size_t Texture::get_data_size() const {std::lock_guard<std::mutex> lock(g_texture_mutex);auto it=g_texture_records.find(texture_id_);return it==g_texture_records.end()?0:it->second.data.size();}
size_t Texture::get_mip_count() const {std::lock_guard<std::mutex> lock(g_texture_mutex);auto it=g_texture_records.find(texture_id_);return it==g_texture_records.end()?0:it->second.mips.size()+1u;}
void Texture::generate_mipmaps(){
    const uint32_t channels=channels_8bit(desc_.format);if(!texture_id_||channels==0) return;
    std::lock_guard<std::mutex> lock(g_texture_mutex);auto it=g_texture_records.find(texture_id_);if(it==g_texture_records.end())return;
    it->second.mips.clear();std::vector<uint8_t> prev=it->second.data;uint32_t w=desc_.width,h=desc_.height;
    while(w>1u||h>1u){const uint32_t nw=std::max(1u,w/2u),nh=std::max(1u,h/2u);std::vector<uint8_t> next(static_cast<size_t>(nw)*nh*channels);
        for(uint32_t y=0;y<nh;++y)for(uint32_t x=0;x<nw;++x)for(uint32_t ch=0;ch<channels;++ch){
            uint32_t sum=0,count=0;for(uint32_t oy=0;oy<2;++oy)for(uint32_t ox=0;ox<2;++ox){uint32_t sx=x*2u+ox,sy=y*2u+oy;if(sx<w&&sy<h){sum+=prev[(static_cast<size_t>(sy)*w+sx)*channels+ch];++count;}}
            next[(static_cast<size_t>(y)*nw+x)*channels+ch]=static_cast<uint8_t>(sum/std::max(1u,count));}
        it->second.mips.push_back(next);prev=std::move(next);w=nw;h=nh;}
}
Texture2D::Texture2D(const TextureDesc& desc):Texture(desc){}
std::unique_ptr<Texture2D> Texture2D::create(uint32_t width,uint32_t height,TextureFormat format,const void* data){
    TextureDesc desc;desc.width=width;desc.height=height;desc.format=format;auto texture=std::unique_ptr<Texture2D>(new Texture2D(desc));
    if(texture->get_id()==0) return nullptr;if(data) texture->set_data(data,expected_size(desc));if(desc.generate_mipmaps)texture->generate_mipmaps();return texture;
}
std::unique_ptr<Texture2D> Texture2D::load_from_file(const std::string& path){
    uint32_t w=0,h=0;std::vector<uint8_t> rgba;
    std::string lower=path;std::transform(lower.begin(),lower.end(),lower.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
    bool ok=false;if(lower.size()>=4&&lower.substr(lower.size()-4)==".ppm")ok=read_ppm(path,w,h,rgba);else if(lower.size()>=4&&lower.substr(lower.size()-4)==".tga")ok=read_tga(path,w,h,rgba);
    if(!ok)return nullptr;return create(w,h,TextureFormat::RGBA8,rgba.data());
}
Cubemap::Cubemap(const TextureDesc& desc):Texture(desc){}
std::unique_ptr<Cubemap> Cubemap::load_from_file(const std::string& path){
    auto face=Texture2D::load_from_file(path);if(!face)return nullptr;
    TextureDesc desc;desc.width=face->get_width();desc.height=face->get_height();desc.format=face->get_format();
    auto cube=std::unique_ptr<Cubemap>(new Cubemap(desc));if(cube->get_id()==0)return nullptr;
    std::lock_guard<std::mutex> lock(g_texture_mutex);
    auto src=g_texture_records.find(face->get_id()),dst=g_texture_records.find(cube->get_id());
    if(src==g_texture_records.end()||dst==g_texture_records.end())return nullptr;
    const std::vector<uint8_t> one=src->second.data;dst->second.data.clear();dst->second.data.reserve(one.size()*6u);
    for(int i=0;i<6;++i)dst->second.data.insert(dst->second.data.end(),one.begin(),one.end());
    return cube;
}
std::unique_ptr<Cubemap> Cubemap::create(const std::vector<std::string>& faces){
    if(faces.size()!=6u)return nullptr;std::vector<std::unique_ptr<Texture2D>> loaded;loaded.reserve(6);
    for(const auto& path:faces){auto face=Texture2D::load_from_file(path);if(!face)return nullptr;if(!loaded.empty()&&(face->get_width()!=loaded[0]->get_width()||face->get_height()!=loaded[0]->get_height()||face->get_format()!=loaded[0]->get_format()))return nullptr;loaded.push_back(std::move(face));}
    TextureDesc desc;desc.width=loaded[0]->get_width();desc.height=loaded[0]->get_height();desc.format=loaded[0]->get_format();auto cube=std::unique_ptr<Cubemap>(new Cubemap(desc));if(cube->get_id()==0)return nullptr;
    std::lock_guard<std::mutex> lock(g_texture_mutex);auto dst=g_texture_records.find(cube->get_id());if(dst==g_texture_records.end())return nullptr;
    dst->second.data.clear();for(const auto& face:loaded){auto src=g_texture_records.find(face->get_id());if(src==g_texture_records.end())return nullptr;dst->second.data.insert(dst->second.data.end(),src->second.data.begin(),src->second.data.end());}
    return cube;
}
TextureAtlas::TextureAtlas(uint32_t width,uint32_t height):texture_id_(g_next_texture_id.fetch_add(1u)),width_(width),height_(height){if(width_==0||height_==0){texture_id_=0;width_=height_=0;}}
TextureAtlas::~TextureAtlas()=default;
bool TextureAtlas::add_subtexture(const std::string& name,uint32_t x,uint32_t y,uint32_t width,uint32_t height){
    if(texture_id_==0||name.empty()||width==0||height==0||x>width_||y>height_||width>width_-x||height>height_-y||subtextures_.count(name))return false;
    subtextures_[name]=Vec4{static_cast<float>(x)/width_,static_cast<float>(y)/height_,static_cast<float>(x+width)/width_,static_cast<float>(y+height)/height_};return true;
}
Vec4 TextureAtlas::get_uv_coords(const std::string& name) const {auto it=subtextures_.find(name);return it==subtextures_.end()?Vec4::zero():it->second;}
Texture* TextureManager::load_texture(const std::string& name,const std::string& path){if(name.empty()||textures_.count(name))return nullptr;auto t=Texture2D::load_from_file(path);if(!t)return nullptr;Texture* p=t.get();textures_.emplace(name,std::move(t));return p;}
Texture* TextureManager::create_texture(const std::string& name,const TextureDesc& desc){if(name.empty()||textures_.count(name))return nullptr;auto t=std::unique_ptr<Texture2D>(new Texture2D(desc));if(t->get_id()==0)return nullptr;Texture* p=t.get();textures_.emplace(name,std::move(t));return p;}
Texture* TextureManager::get_texture(const std::string& name){auto it=textures_.find(name);return it==textures_.end()?nullptr:it->second.get();}
void TextureManager::remove_texture(const std::string& name){textures_.erase(name);}
void TextureManager::clear(){textures_.clear();}
} // namespace litt
