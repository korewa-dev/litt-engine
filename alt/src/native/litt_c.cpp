// Litt C API Implementation
#include "litt_c.h"
#include "littcore/litt.h"
#include "littcore/litt_engine.h"
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <unordered_map>
#include <algorithm>
#include <cmath>
#include <new>
#include <string>

struct LittEngine {
    litt::Engine engine;
    litt::EngineConfig config;
    LittEngineState state;
    LittQualityPreset quality;
    LittGpuInfo gpu_info;
    LittRenderStats render_stats;
    LittCamera camera;
    int frame_width;
    int frame_height;
    bool frame_valid;
    LittLogCb log_cb;
    void* log_user;
};

struct LittWorld {
    struct EntityRecord {
        LittEntityDesc desc{};
        uint32_t components = 0;
        std::unordered_map<uint32_t, std::string> component_configs;
    };
    litt::WorldManager world;
    bool running = false;
    char scene_path[1024]{};
    char assets_base[1024]{};
    litt_entity_t next_entity_id = 1;
    std::unordered_map<litt_entity_t, EntityRecord> entities;
};

LittEngine* litt_engine_create(void) {
    LittEngine* eng = new (std::nothrow) LittEngine();
    if (!eng) return nullptr;
    eng->state = LITT_ENGINE_STATE_DISCONNECTED;
    eng->quality = LITT_QUALITY_MEDIUM;
    eng->frame_width = 1920; eng->frame_height = 1080; eng->frame_valid = false;
    eng->log_cb = nullptr; eng->log_user = nullptr;
    eng->camera.pos_x = 0.0f; eng->camera.pos_y = 5.0f; eng->camera.pos_z = 10.0f;
    eng->camera.yaw = 0.0f; eng->camera.pitch = -0.3f; eng->camera.fov = 60.0f;
    eng->camera.exposure = 1.0f; eng->camera.aspect_ratio = 16.0f / 9.0f;
    litt_engine_log(eng, "Engine created");
    return eng;
}
void litt_engine_destroy(LittEngine* eng) { if (eng) { litt_engine_log(eng, "Engine destroyed"); delete eng; } }
LittEngineState litt_get_state(LittEngine* eng) { return eng ? eng->state : LITT_ENGINE_STATE_DISCONNECTED; }
const char* litt_state_name(LittEngineState state) {
    switch (state) {
        case LITT_ENGINE_STATE_DISCONNECTED: return "Disconnected";
        case LITT_ENGINE_STATE_CONNECTING: return "Connecting";
        case LITT_ENGINE_STATE_CONNECTED: return "Connected";
        case LITT_ENGINE_STATE_RUNNING: return "Running";
        case LITT_ENGINE_STATE_PAUSED: return "Paused";
        case LITT_ENGINE_STATE_ERROR: return "Error";
        default: return "Unknown";
    }
}
bool litt_connect(LittEngine* eng, const char* host, int port) {
    if (!eng) return false;
    eng->state = LITT_ENGINE_STATE_ERROR;
    litt_engine_log(eng, "Remote connection unavailable (host=%s port=%d): no release-supported transport backend", host ? host : "(null)", port);
    return false;
}
void litt_disconnect(LittEngine* eng) { if (eng) { eng->state = LITT_ENGINE_STATE_DISCONNECTED; litt_engine_log(eng, "Disconnected"); } }
bool litt_is_connected(LittEngine* eng) { return eng && eng->state == LITT_ENGINE_STATE_CONNECTED; }

LittWorld* litt_world_create(const char* scene_path, const char* assets_base) {
    LittWorld* world = new (std::nothrow) LittWorld(); if (!world) return nullptr;
    if (assets_base) std::snprintf(world->assets_base, sizeof(world->assets_base), "%s", assets_base);
    if (scene_path && scene_path[0] != '\0' && !litt_world_load(world, scene_path)) { delete world; return nullptr; }
    return world;
}
void litt_world_destroy(LittWorld* world) { delete world; }
bool litt_world_load(LittWorld* world, const char* scene_path) {
    if (!world || !scene_path || !scene_path[0] || !world->world.load(scene_path)) return false;
    std::snprintf(world->scene_path, sizeof(world->scene_path), "%s", scene_path); return true;
}
bool litt_world_save(LittWorld* world, const char* scene_path) {
    if (!world || !scene_path || !scene_path[0] || !world->world.save(scene_path)) return false;
    std::snprintf(world->scene_path, sizeof(world->scene_path), "%s", scene_path); return true;
}

static uint32_t component_bit(LittComponentType type) {
    const uint32_t value = static_cast<uint32_t>(type);
    return (value >= static_cast<uint32_t>(LITT_COMPONENT_TRANSFORM) && value <= static_cast<uint32_t>(LITT_COMPONENT_UI)) ? (1u << value) : 0u;
}
static LittWorld::EntityRecord* find_entity(LittWorld* world, litt_entity_t id) {
    if (!world) return nullptr; auto it = world->entities.find(id); return it == world->entities.end() ? nullptr : &it->second;
}
static const LittWorld::EntityRecord* find_entity(const LittWorld* world, litt_entity_t id) {
    if (!world) return nullptr; auto it = world->entities.find(id); return it == world->entities.end() ? nullptr : &it->second;
}
litt_entity_t litt_world_create_entity(LittWorld* world, const LittEntityDesc* desc) {
    if (!world || !desc) return UINT32_MAX;
    if (!std::isfinite(desc->position.x)||!std::isfinite(desc->position.y)||!std::isfinite(desc->position.z)||!std::isfinite(desc->rotation.x)||!std::isfinite(desc->rotation.y)||!std::isfinite(desc->rotation.z)||!std::isfinite(desc->scale.x)||!std::isfinite(desc->scale.y)||!std::isfinite(desc->scale.z)) return UINT32_MAX;
    litt_entity_t id = world->next_entity_id++; if (id == UINT32_MAX) id = world->next_entity_id++;
    LittWorld::EntityRecord record; record.desc=*desc; record.desc.name[sizeof(record.desc.name)-1]='\0'; record.components=component_bit(LITT_COMPONENT_TRANSFORM);
    world->entities.emplace(id,std::move(record)); return id;
}
bool litt_world_delete_entity(LittWorld* world, litt_entity_t id) { return world && world->entities.erase(id)==1; }
bool litt_world_get_entity(LittWorld* world,litt_entity_t id,LittEntityDesc* out) { if(!out)return false; auto*r=find_entity(world,id); if(!r)return false; *out=r->desc; return true; }
int litt_world_list_entities(LittWorld* world,litt_entity_t* ids,int max_count) {
    if(!world||max_count<0||(max_count>0&&!ids))return 0; std::vector<litt_entity_t> sorted; sorted.reserve(world->entities.size()); for(const auto&p:world->entities)sorted.push_back(p.first); std::sort(sorted.begin(),sorted.end()); int count=std::min<int>(max_count,static_cast<int>(sorted.size())); for(int i=0;i<count;++i)ids[i]=sorted[static_cast<size_t>(i)]; return count;
}

static bool normalize_component_config(const char* input, std::string& out) {
    const char* text = (input && input[0]) ? input : "{}";
    LvJson* json = lvj_parse_strict(text);
    if (!json || json->kind != LJ_OBJ) { lvj_free(json); return false; }
    lvj_free(json); out = text; return true;
}
bool litt_world_add_component(LittWorld* world,litt_entity_t id,LittComponentType type,const char* config_json) {
    auto*r=find_entity(world,id); uint32_t bit=component_bit(type); if(!r||!bit)return false;
    std::string config; if(!normalize_component_config(config_json,config))return false;
    r->components|=bit; r->component_configs[static_cast<uint32_t>(type)]=std::move(config); return true;
}
bool litt_world_remove_component(LittWorld* world,litt_entity_t id,LittComponentType type) {
    auto*r=find_entity(world,id); uint32_t bit=component_bit(type); if(!r||!bit||type==LITT_COMPONENT_TRANSFORM)return false; bool had=(r->components&bit)!=0; if(had){r->components&=~bit;r->component_configs.erase(static_cast<uint32_t>(type));} return had;
}
bool litt_world_has_component(LittWorld* world,litt_entity_t id,LittComponentType type) { auto*r=find_entity(world,id); uint32_t bit=component_bit(type); return r&&bit&&(r->components&bit)!=0; }
int litt_world_get_component_config(LittWorld* world,litt_entity_t id,LittComponentType type,char* buf,int buf_size) {
    auto*r=find_entity(world,id); uint32_t bit=component_bit(type); if(!r||!bit||(r->components&bit)==0||buf_size<0)return 0;
    auto it=r->component_configs.find(static_cast<uint32_t>(type)); if(it==r->component_configs.end())return 0;
    size_t needed=it->second.size()+1; if(needed>static_cast<size_t>(INT32_MAX))return 0;
    if(buf){if(buf_size<static_cast<int>(needed))return 0; std::memcpy(buf,it->second.c_str(),needed);} return static_cast<int>(needed);
}

bool litt_world_set_position(LittWorld*w,litt_entity_t id,const litt_vec3_t*p){auto*r=find_entity(w,id);if(!r||!p||!std::isfinite(p->x)||!std::isfinite(p->y)||!std::isfinite(p->z))return false;r->desc.position=*p;return true;}
bool litt_world_get_position(LittWorld*w,litt_entity_t id,litt_vec3_t*out){auto*r=find_entity(w,id);if(!r||!out)return false;*out=r->desc.position;return true;}
bool litt_world_set_rotation(LittWorld*w,litt_entity_t id,const litt_vec3_t*p){auto*r=find_entity(w,id);if(!r||!p||!std::isfinite(p->x)||!std::isfinite(p->y)||!std::isfinite(p->z))return false;r->desc.rotation=*p;return true;}
bool litt_world_get_rotation(LittWorld*w,litt_entity_t id,litt_vec3_t*out){auto*r=find_entity(w,id);if(!r||!out)return false;*out=r->desc.rotation;return true;}
bool litt_world_set_scale(LittWorld*w,litt_entity_t id,const litt_vec3_t*p){auto*r=find_entity(w,id);if(!r||!p||!std::isfinite(p->x)||!std::isfinite(p->y)||!std::isfinite(p->z))return false;r->desc.scale=*p;return true;}
bool litt_world_get_scale(LittWorld*w,litt_entity_t id,litt_vec3_t*out){auto*r=find_entity(w,id);if(!r||!out)return false;*out=r->desc.scale;return true;}

bool litt_world_start(LittWorld*w){if(!w)return false;w->running=true;return true;}
bool litt_world_stop(LittWorld*w){if(!w)return false;w->running=false;return true;}
bool litt_world_is_running(LittWorld*w){return w&&w->running;}
void litt_world_step(LittWorld*w,float dt){if(w&&w->running&&std::isfinite(dt)&&dt>0.0f)w->world.update(dt);}

void litt_engine_set_quality(LittEngine*e,LittQualityPreset q){if(e)e->quality=q;}
LittQualityPreset litt_engine_get_quality(LittEngine*e){return e?e->quality:LITT_QUALITY_MEDIUM;}
void litt_engine_get_gpu_info(LittEngine*e,LittGpuInfo*i){if(!i)return;std::memset(i,0,sizeof(*i));if(!e)return;std::snprintf(i->name,sizeof(i->name),"%s","Unavailable");std::snprintf(i->vendor,sizeof(i->vendor),"%s","Unavailable");}
void litt_engine_get_render_stats(LittEngine*e,LittRenderStats*s){if(!s)return;std::memset(s,0,sizeof(*s));if(e){s->width=static_cast<uint32_t>(std::max(0,e->frame_width));s->height=static_cast<uint32_t>(std::max(0,e->frame_height));}}
void litt_engine_get_camera(LittEngine*e,LittCamera*c){if(e&&c)*c=e->camera;}
void litt_engine_set_camera(LittEngine*e,const LittCamera*c){if(!e||!c)return;if(!std::isfinite(c->pos_x)||!std::isfinite(c->pos_y)||!std::isfinite(c->pos_z)||!std::isfinite(c->yaw)||!std::isfinite(c->pitch)||!std::isfinite(c->fov)||!std::isfinite(c->exposure)||!std::isfinite(c->aspect_ratio)||c->fov<=0.0f||c->fov>=180.0f||c->aspect_ratio<=0.0f)return;e->camera=*c;}
bool litt_engine_get_framebuffer(LittEngine*e,uint8_t*buf,int*w,int*h){if(w)*w=e?e->frame_width:0;if(h)*h=e?e->frame_height:0;if(!e||!buf||!e->frame_valid)return false;return false;}

void litt_engine_set_log_callback(LittEngine*e,LittLogCb cb,void*u){if(e){e->log_cb=cb;e->log_user=u;}}
void litt_engine_log(LittEngine*e,const char*fmt,...){va_list a;va_start(a,fmt);char b[512];vsnprintf(b,sizeof(b),fmt,a);va_end(a);fprintf(stderr,"[litt] %s\n",b);if(e&&e->log_cb)e->log_cb(b,e->log_user);}
const char* litt_version(void){return "1.0.0";}
