// Litt Engine - Game Integration Layer
// Ties all subsystems together for AI-driven game development

#pragma once
#include "litt_math.h"
#include "litt_ecs.h"
#include "litt_gpu.h"
#include "litt_gpu_software.h"
#include "litt_input.h"
#include "litt_audio.h"
#include "litt_physics.h"
#include "litt_world.h"
#include "litt_obj.h"
#include "litt_json.h"

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cmath>

namespace litt {

// =============================================================================
// Scene Node - represents an object in the game world
// =============================================================================
struct SceneNode {
    std::string name;
    std::string model_path;
    Vec3 position = Vec3::zero();
    Quat rotation = Quat::identity();
    Vec3 scale = Vec3::one();
    std::vector<std::string> tags;
    bool solid = false;
    bool interactable = false;
    bool alive = true;
    
    // Triangle data (loaded from OBJ)
    std::vector<Vec3> vertices;
    std::vector<uint32_t> indices;
    uint32_t color = 0xCCCCCC;
};

// =============================================================================
// Game Scene - loads from JSON scene file
// =============================================================================
class GameScene {
public:
    std::vector<SceneNode> nodes;
    Vec3 spawn_point = Vec3(0, 1.2f, 5);
    bool has_spawn_point = false;
    Vec3 light_dir = Vec3(0.45f, 0.78f, 0.32f);
    Vec3 sky_color = Vec3(0.53f, 0.72f, 0.83f);
    
    bool load_from_json(const std::string& path) {
        std::ifstream f(path);
        if (!f.is_open()) return false;
        
        std::string content((std::istreambuf_iterator<char>(f)),
                           std::istreambuf_iterator<char>());
        f.close();
        
        LvJson* root = lvj_parse(content.c_str());
        if (!root) return false;
        
        // Parse nodes
        const LvJson* nodes_arr = lvj_get(root, "nodes");
        if (nodes_arr) {
            for (int i = 0; i < nodes_arr->count; i++) {
                const LvJson* node = lvj_at(nodes_arr, i);
                if (!node) continue;
                
                SceneNode sn;
                
                const LvJson* name_val = lvj_get(node, "name");
                if (name_val && name_val->str) sn.name = name_val->str;
                
                const LvJson* pos = lvj_get(node, "position");
                if (pos && pos->count >= 3) {
                    sn.position.x = lvj_num(lvj_at(pos, 0), 0);
                    sn.position.y = lvj_num(lvj_at(pos, 1), 0);
                    sn.position.z = lvj_num(lvj_at(pos, 2), 0);
                }
                
                const LvJson* rot = lvj_get(node, "rotation");
                if (rot && rot->count >= 4) {
                    sn.rotation = Quat(
                        (float)lvj_num(lvj_at(rot, 0), 0),
                        (float)lvj_num(lvj_at(rot, 1), 0),
                        (float)lvj_num(lvj_at(rot, 2), 0),
                        (float)lvj_num(lvj_at(rot, 3), 1)).normalized();
                } else if (rot && rot->count >= 3) {
                    sn.rotation = Quat::from_euler(Vec3(
                        (float)lvj_num(lvj_at(rot, 0), 0),
                        (float)lvj_num(lvj_at(rot, 1), 0),
                        (float)lvj_num(lvj_at(rot, 2), 0)));
                }

                const LvJson* scale = lvj_get(node, "scale");
                if (scale && scale->count >= 3) {
                    sn.scale = Vec3(
                        (float)lvj_num(lvj_at(scale, 0), 1),
                        (float)lvj_num(lvj_at(scale, 1), 1),
                        (float)lvj_num(lvj_at(scale, 2), 1));
                }
                
                const LvJson* tags = lvj_get(node, "tags");
                if (tags) {
                    for (int t = 0; t < tags->count; t++) {
                        const LvJson* tag = lvj_at(tags, t);
                        if (tag && tag->str) {
                            sn.tags.push_back(tag->str);
                            if (std::string(tag->str) == "floor" || std::string(tag->str) == "terrain" || std::string(tag->str) == "platform")
                                sn.solid = true;
                            if (std::string(tag->str) == "pickup" || std::string(tag->str) == "enemy" || std::string(tag->str) == "goal")
                                sn.interactable = true;
                        }
                    }
                }
                
                // Check for model reference
                for (const auto& tag : sn.tags) {
                    if (tag.rfind("model:", 0) == 0 && tag.size() > 6) {
                        sn.model_path = "assets/models/" + tag.substr(6) + ".obj";
                    }
                }
                
                bool player_start = sn.name == "Player_Start" ||
                                    (sn.name.size() >= 12 &&
                                     sn.name.compare(sn.name.size() - 12, 12, "Player_Start") == 0);
                bool player_tag = false, start_tag = false;
                for (const auto& tag : sn.tags) {
                    player_tag = player_tag || tag == "player";
                    start_tag = start_tag || tag == "start";
                }
                if (player_start || (player_tag && start_tag)) {
                    spawn_point = sn.position;
                    has_spawn_point = true;
                }

                if (!sn.name.empty() && sn.name != "Root") {
                    nodes.push_back(sn);
                }
            }
        }
        
        lvj_free(root);
        return true;
    }
    
    bool load_models(const std::string& base_path) {
        for (auto& node : nodes) {
            if (node.model_path.empty()) continue;
            
            std::string full_path = base_path + "/" + node.model_path;
            LvModel model = {};
            if (lv_obj_load(full_path.c_str(), &model) == 0 && model.count > 0) {
                // Convert OBJ to triangle list
                for (int m = 0; m < model.count; m++) {
                    LvMesh& mesh = model.meshes[m];
                    uint32_t base = (uint32_t)node.vertices.size();
                    
                    // Add vertices
                    for (int v = 0; v < mesh.vn; v++) {
                        node.vertices.push_back(Vec3(
                            mesh.verts[v*3],
                            mesh.verts[v*3+1],
                            mesh.verts[v*3+2]
                        ));
                    }
                    
                    // Add indices
                    for (int idx = 0; idx < mesh.in; idx++) {
                        node.indices.push_back(base + mesh.idx[idx]);
                    }
                }
                lv_model_free(&model);
            }
        }
        return true;
    }
};

// =============================================================================
// Game class - main entry point for AI-driven game development
// =============================================================================
class Game {
public:
    Game() = default;
    ~Game() { shutdown(); }
    
    // Initialize the game with a project directory
    bool initialize(const std::string& project_dir) {
        project_dir_ = project_dir;
        
        // Load scene
        if (!scene_.load_from_json(project_dir + "/assets/scenes/world.lscn.json")) {
            std::cerr << "[Game] Failed to load scene" << std::endl;
            return false;
        }
        
        // Load models
        scene_.load_models(project_dir);

        // Generated gameplay configuration is authoritative for physics. The
        // semantic Player_Start node remains the preferred spawn source.
        load_project_config(project_dir + "/world_state.json");
        
        // Initialize renderer
        renderer_ = std::make_unique<SoftwareRenderer>();
        if (!renderer_->initialize("headless")) {
            std::cerr << "[Game] Failed to initialize renderer" << std::endl;
            return false;
        }
        
        // Initialize audio
        audio_.init();
        
        // Mirror the canonical Game runtime configuration into the legacy
        // WorldManager facade. WorldManager is no longer ticked independently.
        world_.state.cfg.gravity = gravity_;
        world_.state.cfg.jump = jump_speed_;
        world_.state.cfg.speed = move_speed_;
        
        // Setup player
        player_pos_ = scene_.spawn_point;
        player_vel_ = Vec3::zero();
        camera_pos_ = player_pos_ + Vec3(0, 4.5f, 9);
        camera_yaw_ = 3.14159f;
        grounded_ = false;
        score_ = 0;
        game_over_ = false;
        won_ = false;
        sync_world_facade();
        
        std::cout << "[Game] Initialized with " << scene_.nodes.size() << " nodes" << std::endl;
        return true;
    }
    
    void shutdown() {
        if (renderer_) renderer_->shutdown();
        audio_.shutdown();
    }
    
    // Main game loop - call this every frame
    void update(float dt) {
        // Process input
        input_.update();
        
        // Handle movement
        handle_input(dt);
        
        // Apply physics
        player_vel_.y += gravity_ * dt;
        player_pos_ = player_pos_ + player_vel_ * dt;
        
        // Ground collision
        float ground_y = get_ground_height(player_pos_.x, player_pos_.y, player_pos_.z);
        if (ground_y > -1000.0f && player_pos_.y <= ground_y + 0.05f && player_vel_.y <= 0) {
            player_pos_.y = ground_y;
            player_vel_.y = 0;
            grounded_ = true;
        } else {
            grounded_ = false;
        }
        
        // Fall reset
        if (player_pos_.y < -14.0f) {
            player_pos_ = scene_.spawn_point;
            player_vel_ = Vec3::zero();
        }
        
        // Entity interactions
        handle_interactions();
        
        // Keep the legacy WorldManager view synchronized without running a
        // second, conflicting player simulation.
        sync_world_facade();
    }
    
    // Render the current frame
    void render(float dt) {
        if (!renderer_) return;
        
        // Clear
        uint32_t bg = vec3_to_color(scene_.sky_color);
        renderer_->clear(bg);
        
        // Smooth camera follow (lerp towards target)
        float cam_speed = 5.0f;
        Vec3 target_eye = player_pos_ + Vec3(
            std::sin(camera_yaw_) * 9,
            4.5f,
            std::cos(camera_yaw_) * 9
        );
        camera_pos_ = camera_pos_ + (target_eye - camera_pos_) * cam_speed * dt;
        Vec3 look = player_pos_ + Vec3(0, 1.4f, 0);
        
        Mat4 view = Mat4::look_at(camera_pos_, look, Vec3(0, 1, 0));
        Mat4 proj = Mat4::perspective(62.0f, 16.0f/9.0f, 0.1f, 100.0f);
        Mat4 view_proj = proj * view;
        
        // Draw grid
        renderer_->draw_grid(2.0f, view_proj, 0x333333);
        
        // Draw player (simple cube at player position)
        renderer_->draw_cube(player_pos_, 0.5f, view_proj, 0x00FF00);
        
        // Draw scene nodes
        for (const auto& node : scene_.nodes) {
            if (node.vertices.empty()) continue;
            
            uint32_t color = node.color;
            bool is_enemy = false;
            for (const auto& tag : node.tags) {
                if (tag == "enemy") is_enemy = true;
            }
            if (is_enemy) color = 0xFF0000;
            
            // Draw triangles
            for (size_t i = 0; i + 2 < node.indices.size(); i += 3) {
                const Mat4 model = node_transform(node);
                Vec3 v0 = model * node.vertices[node.indices[i]];
                Vec3 v1 = model * node.vertices[node.indices[i+1]];
                Vec3 v2 = model * node.vertices[node.indices[i+2]];
                renderer_->draw_triangle_3d(v0, v1, v2, view_proj, color);
            }
        }
        
        renderer_->present();
    }
    
    // Input state queries
    bool is_key_down(Key key) const { return input_.key_down(key); }
    bool is_key_pressed(Key key) const { return input_.key_pressed(key); }
    Vec2 get_mouse_position() const { return Vec2(0, 0); }
    
    // Game state
    Vec3 get_player_position() const { return player_pos_; }
    void set_player_position(const Vec3& pos) { player_pos_ = pos; sync_world_facade(); }
    int get_score() const { return score_; }
    void add_score(int points) { score_ += points; sync_world_facade(); }
    bool is_game_over() const { return game_over_; }
    bool is_won() const { return won_; }
    void set_won(bool w) { won_ = w; sync_world_facade(); }
    
    // Subsystem access
    Input& get_input() { return input_; }
    AudioManager& get_audio() { return audio_; }
    GameScene& get_scene() { return scene_; }
    WorldManager& get_world() { return world_; }
    IGPUDevice& get_renderer() { return *renderer_; }
    
private:
    void handle_input(float dt) {
        Vec3 move(0, 0, 0);
        if (input_.key_down(Key::W) || input_.key_down(Key::Up)) move.z -= 1;
        if (input_.key_down(Key::S) || input_.key_down(Key::Down)) move.z += 1;
        if (input_.key_down(Key::A) || input_.key_down(Key::Left)) move.x -= 1;
        if (input_.key_down(Key::D) || input_.key_down(Key::Right)) move.x += 1;
        
        if (move.length() > 0) {
            move = move.normalized();
            float c = std::cos(camera_yaw_);
            float s = std::sin(camera_yaw_);
            player_pos_.x += (move.x * c - move.z * s) * move_speed_ * dt;
            player_pos_.z += (move.x * s + move.z * c) * move_speed_ * dt;
        }
        
        if ((input_.key_down(Key::Space) || input_.action_pressed("jump")) && grounded_) {
            player_vel_.y = jump_speed_;
            grounded_ = false;
        }
        
        if (input_.key_down(Key::Q)) camera_yaw_ += 2.2f * dt;
        if (input_.key_down(Key::E)) camera_yaw_ -= 2.2f * dt;
    }
    
    void handle_interactions() {
        for (auto& node : scene_.nodes) {
            if (!node.alive || !node.interactable) continue;
            
            float dist = (node.position - player_pos_).length();
            
            bool is_enemy = false;
            bool is_pickup = false;
            bool is_goal = false;
            for (const auto& tag : node.tags) {
                if (tag == "enemy") is_enemy = true;
                if (tag == "pickup" || tag == "score") is_pickup = true;
                if (tag == "goal" || tag == "win") is_goal = true;
            }
            
            if (is_enemy && dist < 1.1f) {
                player_pos_ = scene_.spawn_point;
                player_vel_ = Vec3::zero();
            }
            
            if (is_pickup && dist < 1.6f) {
                node.alive = false;
                score_ += 10;
            }
            
            if (is_goal && dist < 2.0f) {
                won_ = true;
            }
        }
    }
    
    float get_ground_height(float x, float y, float z) {
        float best = -1000.0f;
        for (const auto& node : scene_.nodes) {
            if (!node.solid) continue;
            if (node.vertices.empty()) continue;
            
            float min_x = 1e10f, max_x = -1e10f;
            float min_z = 1e10f, max_z = -1e10f;
            float max_y = -1e10f;
            
            const Mat4 model = node_transform(node);
            for (const auto& v : node.vertices) {
                const Vec3 world_v = model * v;
                min_x = std::min(min_x, world_v.x);
                max_x = std::max(max_x, world_v.x);
                min_z = std::min(min_z, world_v.z);
                max_z = std::max(max_z, world_v.z);
                max_y = std::max(max_y, world_v.y);
            }
            
            if (min_x - 0.3f <= x && x <= max_x + 0.3f &&
                min_z - 0.3f <= z && z <= max_z + 0.3f) {
                if (max_y <= y + 0.6f && max_y > best) {
                    best = max_y;
                }
            }
        }
        return best;
    }
    
    static Mat4 node_transform(const SceneNode& node) {
        return Mat4::translation(node.position) *
               node.rotation.to_mat4() *
               Mat4::scale(node.scale);
    }

    bool load_project_config(const std::string& path) {
        std::ifstream f(path);
        if (!f.is_open()) return false;
        std::string content((std::istreambuf_iterator<char>(f)),
                            std::istreambuf_iterator<char>());
        LvJson* root = lvj_parse(content.c_str());
        if (!root) return false;

        const LvJson* gameplay = lvj_get(root, "gameplay");
        const LvJson* physics = gameplay ? lvj_get(gameplay, "physics") : nullptr;
        if (physics) {
            const float gravity_mag = (float)lvj_num(
                lvj_get(physics, "gravity"), std::abs(gravity_));
            gravity_ = -std::abs(gravity_mag);
            jump_speed_ = (float)lvj_num(
                lvj_get(physics, "jump_velocity"), jump_speed_);
            move_speed_ = (float)lvj_num(
                lvj_get(physics, "run_speed"), move_speed_);
        }

        // Older generated projects without a semantic Player_Start may fall
        // back to gameplay.spawn, but when the scene contains Player_Start it
        // is the single authoritative spawn.
        if (!scene_.has_spawn_point && gameplay) {
            const LvJson* spawn = lvj_get(gameplay, "spawn");
            if (spawn && spawn->count >= 3) {
                scene_.spawn_point = Vec3(
                    (float)lvj_num(lvj_at(spawn, 0), scene_.spawn_point.x),
                    (float)lvj_num(lvj_at(spawn, 1), scene_.spawn_point.y),
                    (float)lvj_num(lvj_at(spawn, 2), scene_.spawn_point.z));
            }
        }

        lvj_free(root);
        return true;
    }

    void sync_world_facade() {
        world_.state.pos = player_pos_;
        world_.state.vel = player_vel_;
        world_.state.grounded = grounded_ ? 1 : 0;
        world_.state.score = static_cast<unsigned>(std::max(score_, 0));
        world_.state.won = won_ ? 1 : 0;
        world_.state.game_over = game_over_ ? 1 : 0;
    }

    uint32_t vec3_to_color(const Vec3& c) {
        uint8_t r = (uint8_t)(c.x * 255);
        uint8_t g = (uint8_t)(c.y * 255);
        uint8_t b = (uint8_t)(c.z * 255);
        return (r << 16) | (g << 8) | b;
    }
    
    std::string project_dir_;
    GameScene scene_;
    std::unique_ptr<SoftwareRenderer> renderer_;
    AudioManager audio_;
    Input input_;
    WorldManager world_;
    float gravity_ = -22.0f;
    float move_speed_ = 7.0f;
    float jump_speed_ = 8.0f;
    
    Vec3 player_pos_;
    Vec3 player_vel_;
    Vec3 camera_pos_;
    float camera_yaw_ = 0;
    bool grounded_ = false;
    int score_ = 0;
    bool game_over_ = false;
    bool won_ = false;
};

} // namespace litt
