#ifdef LITT_INSTALLED_SDK
#include <litt/litt.h>
#else
#include "litt.h"
#endif

#include <cassert>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <memory>
#include <vector>

using namespace litt;

static bool nearf(float a,float b,float eps=1e-4f){return std::fabs(a-b)<=eps;}

int main(){
    // UI: interaction plus observable retained draw commands.
    UIManager ui;
    int clicks=0;
    auto window=std::make_shared<UIWindow>("HUD",Vec2{0,0},Vec2{200,100});
    auto button=std::make_shared<UIButton>("Start",Vec2{10,10},Vec2{80,30});
    button->onClick([&]{++clicks;});
    window->addElement(button);ui.addWindow(window);
    ui.onMouseMove(Vec2{20,20});ui.onMouseDown(Vec2{20,20});ui.render();
    assert(clicks==1);assert(ui.draw_commands().size()==2u);
    assert(ui.draw_commands()[1].kind==UIElementKind::Button);
    ui.onMouseUp(Vec2{20,20});assert(!button->isPressed());

    // Particles: bounded emission and simulation.
    ParticleSystem particles(32);
    particles.set_emission_rate(10.0f);
    particles.set_lifetime(1.0f,1.0f);
    particles.set_velocity(Vec3{1,0,0},Vec3{1,0,0});
    particles.set_gravity(Vec3::zero());
    particles.play();particles.update(0.5f);
    assert(particles.get_alive_count()==5u);
    const float x0=particles.get_particles()[0].position.x;
    particles.update(0.1f);
    assert(particles.get_particles()[0].position.x>x0);

    // Terrain: deterministic generation and editing.
    TerrainSystem& terrain=TerrainSystem::get_instance();
    terrain.initialize(8,1.0f,2,2);terrain.generate(1234u);
    assert(terrain.get_total_vertex_count()==256u);
    const float sample=terrain.get_height_at(3.0f,3.0f);
    terrain.generate(1234u);
    assert(nearf(sample,terrain.get_height_at(3.0f,3.0f)));
    terrain.set_height_at(2.0f,2.0f,42.0f);
    assert(nearf(terrain.get_height_at(2.0f,2.0f),42.0f));

    // Frustum culling.
    const Mat4 vp=Mat4::perspective(60.0f,1.0f,0.1f,100.0f)*
                  Mat4::look_at(Vec3::zero(),Vec3{0,0,-1},Vec3::up());
    const Frustum frustum=Frustum::from_matrix(vp);
    assert(frustum.contains_point(Vec3{0,0,-5}));
    assert(!frustum.contains_point(Vec3{100,0,-5}));
    CullingSystem& culling=CullingSystem::get_instance();culling.set_frustum(frustum);
    std::vector<Aabb> boxes={Aabb(Vec3{-1,-1,-6},Vec3{1,1,-4}),Aabb(Vec3{99,-1,-6},Vec3{101,1,-4})};
    std::vector<uint32_t> visible;culling.frustum_cull(boxes,visible);
    assert(visible.size()==1u&&visible[0]==0u);

    // Lighting: stable IDs, lookup/removal, direct and environment contribution.
    LightManager& lights=LightManager::get_instance();
    lights.clear();
    Light sun;sun.type=LightType::DIRECTIONAL;sun.direction=Vec3{0,-1,-1};sun.intensity=2.0f;
    const uint32_t sun_id=lights.add_light(sun);
    assert(sun_id!=0u&&lights.get_light(sun_id)!=nullptr);
    const Vec3 lit=PBRLighting::calculate_direct_light(*lights.get_light(sun_id),Vec3::zero(),Vec3::up(),Vec3{0,1,1}.normalized(),Vec3{0.8f,0.7f,0.6f},0.0f,0.5f);
    assert(lit.x>=0.0f&&lit.y>=0.0f&&lit.z>=0.0f);
    const Vec3 ibl=PBRLighting::ibl_diffuse(Vec3::up(),Vec3{0.5f,0.5f,0.5f});
    assert(ibl.x>0.0f);
    ShadowMap shadow(32);shadow.begin_pass(Vec3{0,10,0},Vec3{0,-1,0});assert(shadow.get_light_view_proj().m[15]!=0.0f||shadow.get_light_view_proj().m[11]!=0.0f);
    lights.remove_light(sun_id);assert(lights.get_light(sun_id)==nullptr);

    // Software GPU device: all release-supported resource constructors work.
    auto gpu=create_gpu_device("software");assert(gpu&&gpu->initialize("headless"));
    BufferDesc buffer_desc;buffer_desc.size=64;buffer_desc.usage=GPUBufferUsage::VERTEX;
    assert(gpu->create_buffer(buffer_desc)!=nullptr);
    TextureDesc gpu_tex_desc;gpu_tex_desc.width=8;gpu_tex_desc.height=8;gpu_tex_desc.format=TextureFormat::RGBA8;
    assert(gpu->create_texture(gpu_tex_desc)!=nullptr);
    assert(gpu->create_shader("void main(){}","void main(){}")!=nullptr);
    assert(gpu->create_render_target(8,8)!=nullptr);
    gpu->shutdown();
    assert(gpu_backend_capability("vulkan").support==GPUBackendSupport::Unavailable);

    // CPU texture storage, mip generation and binding.
    std::vector<uint8_t> texels(4u*4u*4u,128u);
    auto texture=Texture2D::create(4,4,TextureFormat::RGBA8,texels.data());
    assert(texture&&texture->get_data_size()==64u&&texture->get_mip_count()==3u);
    texture->bind(3u);assert(Texture::bound_texture(3u)==texture.get());texture->unbind();
    TextureAtlas atlas(100,100);assert(atlas.add_subtexture("hero",10,20,30,40));
    const Vec4 uv=atlas.get_uv_coords("hero");assert(nearf(uv.x,0.1f)&&nearf(uv.w,0.6f));

    // Render targets and executable pipeline passes.
    auto source=std::make_shared<RenderTarget>(4,4);
    auto output=std::make_shared<RenderTarget>(4,4);
    source->clear(Vec4{1,1,1,1});output->clear(Vec4::zero());
    PostProcessPass post;post.set_input_texture(source->get_texture_id());post.set_output_target(output);
    post.set_tone_mapping_enabled(true);post.set_exposure(1.0f);int post_calls=0;post.set_callback([&]{++post_calls;});
    post.execute();assert(post_calls==1&&post.execution_count()==1u);
    assert(!output->color_pixels().empty()&&output->color_pixels()[0]>0u&&output->color_pixels()[0]<255u);

    RenderPipeline pipeline;pipeline.initialize(64,64);
    auto geometry=std::make_unique<GeometryPass>();int geometry_calls=0;geometry->set_callback([&]{++geometry_calls;});
    geometry->set_render_target(std::shared_ptr<RenderTarget>(new RenderTarget(64,64)));
    pipeline.add_pass(std::move(geometry));pipeline.execute();assert(geometry_calls==1);

    // Shader source registry and observable uniform/binding state.
    ShaderProgram shader;
    shader.attach_shader(ShaderType::VERTEX,builtin_shaders::unlit_vertex);
    shader.attach_shader(ShaderType::FRAGMENT,builtin_shaders::unlit_fragment);
    assert(shader.link()&&shader.get_id()!=0u);shader.bind();assert(ShaderProgram::bound_program()==&shader);
    shader.set_vec3("color",Vec3{1,0,0});assert(shader.has_uniform("color"));shader.unbind();

    const char* combined="litt_combined_shader_test.glsl";
    {std::ofstream out(combined);out<<"#type vertex\nvoid main(){}\n#type fragment\nvoid main(){}\n";}
    ShaderLibrary& library=ShaderLibrary::get_instance();
    assert(library.load_shader("combined",combined)!=nullptr);
    assert(library.get_shader("combined")!=nullptr);library.remove_shader("combined");std::remove(combined);

    terrain.shutdown();
    return 0;
}
