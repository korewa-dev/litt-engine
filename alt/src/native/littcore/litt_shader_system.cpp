#include "litt_shader_system.h"

#include <atomic>
#include <fstream>
#include <sstream>

namespace litt {
namespace {
std::atomic<uint32_t> g_next_program_id{1u};
thread_local const ShaderProgram* g_bound_program=nullptr;

bool valid_source(const std::string& source){
    return !source.empty() && source.size() <= 4u*1024u*1024u &&
           source.find("main") != std::string::npos;
}
bool marker_to_type(const std::string& marker,ShaderType& type){
    if(marker=="vertex"){type=ShaderType::VERTEX;return true;}
    if(marker=="fragment"){type=ShaderType::FRAGMENT;return true;}
    if(marker=="geometry"){type=ShaderType::GEOMETRY;return true;}
    if(marker=="compute"){type=ShaderType::COMPUTE;return true;}
    return false;
}
}

void ShaderProgram::attach_shader(ShaderType type,const std::string& source){
    if(!valid_source(source)){shaders_.erase(type);linked_=false;program_id_=0;return;}
    shaders_[type]=source;linked_=false;program_id_=0;
}
bool ShaderProgram::link(){
    const bool compute=shaders_.count(ShaderType::COMPUTE)!=0;
    const bool graphics=shaders_.count(ShaderType::VERTEX)!=0&&shaders_.count(ShaderType::FRAGMENT)!=0;
    if((compute&&shaders_.size()!=1u)||(!compute&&!graphics)){linked_=false;program_id_=0;return false;}
    for(const auto& item:shaders_)if(!valid_source(item.second)){linked_=false;program_id_=0;return false;}
    uint32_t id=g_next_program_id.fetch_add(1u);if(id==0u)id=g_next_program_id.fetch_add(1u);
    program_id_=id;linked_=true;return true;
}
void ShaderProgram::bind() const {if(linked_)g_bound_program=this;}
void ShaderProgram::unbind() const {if(g_bound_program==this)g_bound_program=nullptr;}
const ShaderProgram* ShaderProgram::bound_program(){return g_bound_program;}
void ShaderProgram::set_float(const std::string& name,float value){if(linked_&&!name.empty())uniform_values_[name]={value};}
void ShaderProgram::set_int(const std::string& name,int value){if(linked_&&!name.empty())uniform_values_[name]={static_cast<float>(value)};}
void ShaderProgram::set_vec2(const std::string& name,const Vec2& value){if(linked_&&!name.empty())uniform_values_[name]={value.x,value.y};}
void ShaderProgram::set_vec3(const std::string& name,const Vec3& value){if(linked_&&!name.empty())uniform_values_[name]={value.x,value.y,value.z};}
void ShaderProgram::set_vec4(const std::string& name,const Vec4& value){if(linked_&&!name.empty())uniform_values_[name]={value.x,value.y,value.z,value.w};}
void ShaderProgram::set_mat4(const std::string& name,const Mat4& value){if(linked_&&!name.empty())uniform_values_[name]=std::vector<float>(value.m,value.m+16);}

ShaderProgram* ShaderLibrary::load_shader(const std::string& name,const std::string& filepath){
    if(name.empty()||filepath.empty()||shaders_.count(name))return nullptr;
    std::ifstream in(filepath,std::ios::binary);if(!in)return nullptr;
    std::string text((std::istreambuf_iterator<char>(in)),std::istreambuf_iterator<char>());
    if(text.empty()||text.size()>8u*1024u*1024u)return nullptr;

    auto program=std::make_unique<ShaderProgram>();
    const std::string token="#type ";
    size_t pos=0;bool found=false;
    while((pos=text.find(token,pos))!=std::string::npos){
        const size_t type_start=pos+token.size();
        const size_t line_end=text.find_first_of("\r\n",type_start);
        if(line_end==std::string::npos)break;
        std::string type_name=text.substr(type_start,line_end-type_start);
        while(!type_name.empty()&&(type_name.back()==' '||type_name.back()=='\t'))type_name.pop_back();
        ShaderType type; if(!marker_to_type(type_name,type)){return nullptr;}
        size_t source_start=text.find_first_not_of("\r\n",line_end);
        if(source_start==std::string::npos)return nullptr;
        const size_t next=text.find(token,source_start);
        const std::string source=text.substr(source_start,next==std::string::npos?std::string::npos:next-source_start);
        program->attach_shader(type,source);found=true;
        if(next==std::string::npos)break;pos=next;
    }
    if(!found){
        ShaderType type;
        if(filepath.size()>=5&&filepath.substr(filepath.size()-5)==".vert")type=ShaderType::VERTEX;
        else if(filepath.size()>=5&&filepath.substr(filepath.size()-5)==".frag")type=ShaderType::FRAGMENT;
        else if(filepath.size()>=5&&filepath.substr(filepath.size()-5)==".comp")type=ShaderType::COMPUTE;
        else return nullptr;
        program->attach_shader(type,text);
    }
    if(!program->link())return nullptr;
    ShaderProgram* result=program.get();shaders_.emplace(name,std::move(program));return result;
}
ShaderProgram* ShaderLibrary::get_shader(const std::string& name){auto it=shaders_.find(name);return it==shaders_.end()?nullptr:it->second.get();}
void ShaderLibrary::remove_shader(const std::string& name){shaders_.erase(name);}
void ShaderLibrary::clear(){shaders_.clear();}

namespace builtin_shaders {
const char* pbr_vertex="void main() { }";
const char* pbr_fragment="void main() { }";
const char* unlit_vertex="void main() { }";
const char* unlit_fragment="void main() { }";
const char* shadow_vertex="void main() { }";
const char* shadow_fragment="void main() { }";
const char* post_process_vertex="void main() { }";
const char* post_process_fragment="void main() { }";
const char* bloom_vertex="void main() { }";
const char* bloom_fragment="void main() { }";
}
} // namespace litt
