// Litt C bridge contract tests
#include "litt_c.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
static int passed=0,failed=0;
static void check(bool c,const char*n){if(c){++passed;std::printf("  ok %s\n",n);}else{++failed;std::printf("  FAIL %s\n",n);}}
static bool near(float a,float b,float e=1e-5f){return std::fabs(a-b)<=e;}
int main(){
 std::printf("[C bridge contract]\n");
 LittWorld* world=litt_world_create(nullptr,nullptr); check(world!=nullptr,"world_create_empty");
 LittEntityDesc desc{};std::snprintf(desc.name,sizeof(desc.name),"%s","Box");desc.position={1,2,3};desc.rotation={0,.5f,0};desc.scale={1,2,1};desc.color={1,.5f,.25f,1};
 const litt_entity_t id=litt_world_create_entity(world,&desc);check(id!=UINT32_MAX,"entity_create");
 LittEntityDesc out{};check(litt_world_get_entity(world,id,&out),"entity_get");check(std::strcmp(out.name,"Box")==0&&near(out.position.x,1)&&near(out.position.y,2)&&near(out.position.z,3),"entity_roundtrip");
 litt_entity_t ids[4]{};check(litt_world_list_entities(world,ids,4)==1&&ids[0]==id,"entity_list");
 check(litt_world_has_component(world,id,LITT_COMPONENT_TRANSFORM),"transform_component_present");
 const char* mesh_cfg="{\"model\":\"crate.obj\",\"cast_shadow\":true}";
 check(litt_world_add_component(world,id,LITT_COMPONENT_MESH,mesh_cfg),"component_add_with_config");
 check(litt_world_has_component(world,id,LITT_COMPONENT_MESH),"component_has");
 int needed=litt_world_get_component_config(world,id,LITT_COMPONENT_MESH,nullptr,0);check(needed==(int)std::strlen(mesh_cfg)+1,"component_config_size");
 char config[128]{};check(litt_world_get_component_config(world,id,LITT_COMPONENT_MESH,config,sizeof(config))==needed&&std::strcmp(config,mesh_cfg)==0,"component_config_roundtrip");
 char tiny[2]={'x','x'};check(litt_world_get_component_config(world,id,LITT_COMPONENT_MESH,tiny,sizeof(tiny))==0&&tiny[0]=='x',"component_config_small_buffer_rejected");
 check(!litt_world_add_component(world,id,LITT_COMPONENT_AUDIO,"{broken"),"component_malformed_json_rejected");
 check(!litt_world_has_component(world,id,LITT_COMPONENT_AUDIO),"malformed_config_does_not_add");
 check(!litt_world_add_component(world,id,LITT_COMPONENT_AUDIO,"[1,2,3]"),"component_non_object_json_rejected");
 check(!litt_world_add_component(world,id,(LittComponentType)9,"{}"),"unsupported_component_type_rejected");
 check(!litt_world_add_component(world,id+1000,LITT_COMPONENT_MESH,"{}"),"component_missing_entity_rejected");
 check(litt_world_add_component(world,id,LITT_COMPONENT_LIGHT,nullptr),"null_config_normalized");char emptycfg[4]{};check(litt_world_get_component_config(world,id,LITT_COMPONENT_LIGHT,emptycfg,sizeof(emptycfg))==3&&std::strcmp(emptycfg,"{}")==0,"normalized_config_roundtrip");
 check(litt_world_remove_component(world,id,LITT_COMPONENT_MESH),"component_remove");check(!litt_world_has_component(world,id,LITT_COMPONENT_MESH),"component_removed");check(litt_world_get_component_config(world,id,LITT_COMPONENT_MESH,nullptr,0)==0,"component_remove_cleans_config");
 check(!litt_world_remove_component(world,id,LITT_COMPONENT_TRANSFORM),"transform_component_cannot_remove");
 litt_vec3_t pos{4,5,6};check(litt_world_set_position(world,id,&pos),"position_set");litt_vec3_t got{};check(litt_world_get_position(world,id,&got)&&near(got.x,4)&&near(got.y,5)&&near(got.z,6),"position_get");
 litt_vec3_t rot{.1f,.2f,.3f};check(litt_world_set_rotation(world,id,&rot)&&litt_world_get_rotation(world,id,&got)&&near(got.y,.2f),"rotation_roundtrip");
 litt_vec3_t scale{2,3,4};check(litt_world_set_scale(world,id,&scale)&&litt_world_get_scale(world,id,&got)&&near(got.z,4),"scale_roundtrip");
 litt_vec3_t bad{std::numeric_limits<float>::quiet_NaN(),0,0};check(!litt_world_set_position(world,id,&bad),"position_rejects_nan");check(!litt_world_set_rotation(world,id,&bad),"rotation_rejects_nan");check(!litt_world_set_scale(world,id,&bad),"scale_rejects_nan");check(!litt_world_get_position(world,id+1000,&got),"missing_entity_rejected");
 check(litt_world_start(world),"world_start");check(litt_world_is_running(world),"world_running");litt_world_step(world,1.0f/60.0f);check(litt_world_stop(world),"world_stop");check(!litt_world_is_running(world),"world_stopped");
 check(litt_world_delete_entity(world,id),"entity_delete");check(!litt_world_delete_entity(world,id),"entity_double_delete_rejected");litt_world_destroy(world);
 LittEngine* engine=litt_engine_create();check(engine!=nullptr,"engine_create");check(!litt_connect(engine,"127.0.0.1",8080),"unsupported_remote_connect_fails");check(!litt_is_connected(engine),"failed_remote_connect_not_connected");check(litt_get_state(engine)==LITT_ENGINE_STATE_ERROR,"failed_remote_connect_reports_error");litt_disconnect(engine);check(litt_get_state(engine)==LITT_ENGINE_STATE_DISCONNECTED,"disconnect_resets_state");
 int width=-1,height=-1;unsigned char pixel[4]{};check(!litt_engine_get_framebuffer(engine,pixel,&width,&height),"framebuffer_unavailable_is_honest");check(width==1920&&height==1080,"framebuffer_dimensions_reported");LittGpuInfo info{};litt_engine_get_gpu_info(engine,&info);check(std::strcmp(info.name,"Unavailable")==0&&info.memory_total==0,"gpu_info_unavailable_is_honest");litt_engine_destroy(engine);
 std::printf("\nResults: %d passed, %d failed\n",passed,failed);return failed==0?0:1;
}
