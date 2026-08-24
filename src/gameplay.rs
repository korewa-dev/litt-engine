//! Gameplay session -- THE native game rules, ported from the reference
//! runtime contract (template/tools/runtime/runtime.js) so the engine is the
//! single source of play. Data-driven entirely from a generated world:
//!
//!   identity.movement / identity.camera substrings -> camera mode
//!     "platformer" movement or "side" camera -> 2D5 (A/D strafe, W jump)
//!     "top_down"/"isometric" camera          -> TOP (W pushes away)
//!     otherwise                              -> 3D orbit + mouse look
//!   gameplay.physics.{gravity,jump_velocity,run_speed,
//!                     coyote_time_s,jump_buffer_s} (defaults like the JS)
//!   tags: floor/level/board/track/hub/terrain/platform = solid AABBs
//!         pickup|score|token|dice|objective = score once (+25 coins/+10)
//!         goal|win = victory, checkpoint = move spawn, poi = notice once
//!         hazard = touch death, enemy = plane-homing chaser
//!   gameplay.{lives,score_goal,interact_radius_m,kill_radius_m,
//!             enemy_aggro_m,corpse_run,objective}
//!
//! No serde: worlds are read through a tiny hand-rolled JSON scanner that is
//! unit-tested against the shipped generator output shape.

use std::collections::HashMap;

use litt_asset::ObjLoader;
use litt_math::Vec3;
use litt_scene::SceneGraph;

// ---------------------------------------------------------------- mini JSON

/// Minimal JSON value -- just enough for generated world_state files.
#[derive(Debug, Clone, PartialEq)]
pub enum Json {
    Null,
    Bool(bool),
    Num(f64),
    Str(String),
    Arr(Vec<Json>),
    Obj(Vec<(String, Json)>),
}

impl Json {
    pub fn parse(s: &str) -> Result<Json, String> {
        let b = s.as_bytes();
        let mut i = 0usize;
        let v = Json::_parse(b, &mut i)?;
        Json::_ws(b, &mut i);
        if i != b.len() {
            return Err(format!("trailing json data at byte {}", i));
        }
        Ok(v)
    }

    pub fn get(&self, key: &str) -> Option<&Json> {
        match self {
            Json::Obj(pairs) => pairs.iter().find(|(k, _)| k == key).map(|(_, v)| v),
            _ => None,
        }
    }

    pub fn as_f64(&self) -> Option<f64> {
        match self {
            Json::Num(n) => Some(*n),
            _ => None,
        }
    }

    pub fn as_bool(&self) -> Option<bool> {
        match self {
            Json::Bool(b) => Some(*b),
            _ => None,
        }
    }

    pub fn as_str(&self) -> Option<&str> {
        match self {
            Json::Str(s) => Some(s),
            _ => None,
        }
    }

    fn _ws(b: &[u8], i: &mut usize) {
        while *i < b.len() && (b[*i] as char).is_ascii_whitespace() {
            *i += 1;
        }
    }

    fn _parse(b: &[u8], i: &mut usize) -> Result<Json, String> {
        Json::_ws(b, i);
        let c = *b.get(*i).ok_or("unexpected end of json")?;
        match c {
            b'{' => {
                *i += 1;
                let mut pairs = Vec::new();
                Json::_ws(b, i);
                if b.get(*i) == Some(&b'}') {
                    *i += 1;
                    return Ok(Json::Obj(pairs));
                }
                loop {
                    Json::_ws(b, i);
                    let k = match Json::_parse(b, i)? {
                        Json::Str(s) => s,
                        _ => return Err("object key must be string".into()),
                    };
                    Json::_ws(b, i);
                    if b.get(*i) != Some(&b':') {
                        return Err("expected ':'".into());
                    }
                    *i += 1;
                    let v = Json::_parse(b, i)?;
                    pairs.push((k, v));
                    Json::_ws(b, i);
                    match b.get(*i) {
                        Some(&b',') => *i += 1,
                        Some(&b'}') => {
                            *i += 1;
                            return Ok(Json::Obj(pairs));
                        }
                        _ => return Err("expected ',' or '}'".into()),
                    }
                }
            }
            b'[' => {
                *i += 1;
                let mut items = Vec::new();
                Json::_ws(b, i);
                if b.get(*i) == Some(&b']') {
                    *i += 1;
                    return Ok(Json::Arr(items));
                }
                loop {
                    let v = Json::_parse(b, i)?;
                    items.push(v);
                    Json::_ws(b, i);
                    match b.get(*i) {
                        Some(&b',') => *i += 1,
                        Some(&b']') => {
                            *i += 1;
                            return Ok(Json::Arr(items));
                        }
                        _ => return Err("expected ',' or ']'".into()),
                    }
                }
            }
            b'"' => {
                *i += 1;
                let mut out = String::new();
                while let Some(&ch) = b.get(*i) {
                    *i += 1;
                    match ch {
                        b'"' => return Ok(Json::Str(out)),
                        b'\\' => {
                            let e = *b.get(*i).ok_or("bad escape")?;
                            *i += 1;
                            out.push(match e {
                                b'n' => '\n',
                                b't' => '\t',
                                b'r' => '\r',
                                other => other as char,
                            });
                        }
                        other => out.push(other as char),
                    }
                }
                Err("unterminated string".into())
            }
            b't' => {
                Json::_expect(b, i, "true")?;
                Ok(Json::Bool(true))
            }
            b'f' => {
                Json::_expect(b, i, "false")?;
                Ok(Json::Bool(false))
            }
            b'n' => {
                Json::_expect(b, i, "null")?;
                Ok(Json::Null)
            }
            _ => {
                let start = *i;
                while *i < b.len()
                    && matches!(b[*i], b'-' | b'+' | b'.' | b'e' | b'E' | b'0'..=b'9')
                {
                    *i += 1;
                }
                if start == *i {
                    return Err(format!("unexpected char {:?} at {}", c as char, start));
                }
                let txt = String::from_utf8_lossy(&b[start..*i]).to_string();
                txt.parse::<f64>()
                    .map(Json::Num)
                    .map_err(|_| format!("bad number '{}'", txt))
            }
        }
    }

    fn _expect(b: &[u8], i: &mut usize, word: &str) -> Result<(), String> {
        if b.len() >= *i + word.len() && &b[*i..*i + word.len()] == word.as_bytes() {
            *i += word.len();
            Ok(())
        } else {
            Err(format!("expected '{}'", word))
        }
    }
}

// ------------------------------------------------------------- configuration

/// Camera/play mode resolved from identity substrings.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Mode {
    Orbit3D,
    TopDown,
    Side2D5,
}

#[derive(Debug, Clone)]
pub struct GameplayConfig {
    pub mode: Mode,
    pub gravity: f32,
    pub jump_velocity: f32,
    pub run_speed: f32,
    pub coyote_s: f32,
    pub buffer_s: f32,
    pub enemy_aggro_m: f32,
    pub corpse_run: bool,
    pub lives: Option<u32>,
    pub score_goal: Option<u32>,
    pub interact_m: f32,
    pub kill_m: f32,
    /// Points per pickup (25 when gameplay.scoring.coins is truthy, else 10).
    pub coins_value: u32,
    pub objective: String,
}

impl Default for GameplayConfig {
    fn default() -> Self {
        Self {
            mode: Mode::Orbit3D,
            gravity: 22.0,
            jump_velocity: 8.0,
            run_speed: 7.0,
            coyote_s: 0.1,
            buffer_s: 0.12,
            enemy_aggro_m: 6.0,
            corpse_run: false,
            lives: None,
            score_goal: None,
            interact_m: 1.6,
            kill_m: 1.1,
            coins_value: 10,
            objective: "explore the world".to_string(),
        }
    }
}

impl GameplayConfig {
    /// Parse from the raw world_state.json text (defaults mirror the JS).
    pub fn from_state_json(text: &str) -> GameplayConfig {
        let mut cfg = GameplayConfig::default();
        let Ok(root) = Json::parse(text) else {
            return cfg;
        };
        let id = root.get("identity");
        let movement = id.and_then(|i| i.get("movement")).and_then(|v| v.as_str()).unwrap_or("");
        let camera = id.and_then(|i| i.get("camera")).and_then(|v| v.as_str()).unwrap_or("");
        cfg.mode = resolve_mode(movement, camera);

        if let Some(gp) = root.get("gameplay") {
            if let Some(p) = gp.get("physics") {
                let n = |k: &str, d: f32| p.get(k).and_then(|v| v.as_f64()).unwrap_or(d as f64) as f32;
                cfg.gravity = n("gravity", cfg.gravity);
                cfg.jump_velocity = n("jump_velocity", cfg.jump_velocity);
                cfg.run_speed = n("run_speed", cfg.run_speed);
                cfg.coyote_s = n("coyote_time_s", cfg.coyote_s);
                // JS fallback: buffer = coyote + 0.02
                cfg.buffer_s = p
                    .get("jump_buffer_s")
                    .and_then(|v| v.as_f64())
                    .map(|v| v as f32)
                    .unwrap_or(cfg.coyote_s + 0.02);
            }
            if let Some(o) = gp.get("objective").and_then(|v| v.as_str()) {
                cfg.objective = o.to_string();
            }
            let n = |k: &str, d: f32| gp.get(k).and_then(|v| v.as_f64()).unwrap_or(d as f64) as f32;
            cfg.enemy_aggro_m = n("enemy_aggro_m", cfg.enemy_aggro_m);
            cfg.interact_m = n("interact_radius_m", cfg.interact_m);
            cfg.kill_m = n("kill_radius_m", cfg.kill_m);
            if gp
                .get("scoring")
                .and_then(|s| s.get("coins"))
                .and_then(|v| v.as_bool())
                .unwrap_or(false)
            {
                cfg.coins_value = 25;
            }
            cfg.corpse_run = gp.get("corpse_run").and_then(|v| v.as_bool()).unwrap_or(false);
            cfg.lives = gp.get("lives").and_then(|v| v.as_f64()).map(|v| v as u32);
            cfg.score_goal = gp
                .get("score_goal")
                .and_then(|v| v.as_f64())
                .filter(|v| *v > 0.0)
                .map(|v| v as u32);
        }
        cfg
    }
}

/// Substring mode resolution -- identical to the runtime contract.
pub fn resolve_mode(movement: &str, camera: &str) -> Mode {
    if movement.contains("platformer") || camera.contains("side") {
        Mode::Side2D5
    } else if camera.contains("top_down") || camera.contains("isometric") {
        Mode::TopDown
    } else {
        Mode::Orbit3D
    }
}

// ------------------------------------------------------------------- session

const KILL_PLANE_Y: f32 = -14.0;
const DEAD_FREEZE_S: f32 = 0.7;
const ENEMY_SPEED: f32 = 3.2;

/// Interaction outcomes deferred out of the entity sweep.
enum Ev {
    /// Pickup consumed; Some(item name) for story-flavored toasts.
    Score(Option<String>),
    Goal,
    Checkpoint(Vec3),
    Poi(String),
}

/// Enemy combat tier - drives speed and special behavior in the sweep.
#[derive(Clone, Copy, Debug, Default, PartialEq)]
enum Tier {
    #[default]
    Mook,
    Elite,
    Boss,
}

#[derive(Clone, Debug)]
struct Aabb {
    min: Vec3,
    max: Vec3,
}

#[derive(Clone, Debug)]
struct Interactive {
    name: String,
    pos: Vec3,
    base_y: f32,
    enemy: bool,
    hazard: bool,
    scoring: bool,
    goal: bool,
    checkpoint: bool,
    poi: bool,
    alive: bool,
    seen_poi: bool,
    /// combat tier (mook/elite/boss) for speed + lunge behavior
    tier: Tier,
    /// seconds until the next lunge is allowed (bosses/elites)
    lunge_cd: f32,
}

/// One interactive playthrough of a generated world.
pub struct Session {
    pub cfg: GameplayConfig,
    pub genre_label: String,

    spawn: Vec3,
    pos: Vec3,
    vel: Vec3,
    grounded: bool,
    coyote: f32,
    buffer: f32,
    cam_yaw: f32,
    pub score: u32,
    lives_left: Option<u32>,
    pub game_over: bool,
    pub won: bool,
    dead_until: f32,
    now: f32,
    toast: (String, f32),

    solids: Vec<Aabb>,
    interactives: Vec<Interactive>,
    /// Per-enemy cruise height restore target.
    enemy_base_y: HashMap<usize, f32>,
    /// Animated-clock for rigs; advanced every step.
    pub anim_t: f32,
    /// Set when rendering inputs changed (enemy moved, pickup consumed).
    scene_dirty: bool,
    /// Any enemy currently chasing (drives rig animation intensity).
    any_chasing: bool,

    // camera outputs
    pub cam_pos: Vec3,
    pub cam_yaw_out: f32,
    pub cam_pitch: f32,
    top_zoom: f32,
}

/// Per-frame input snapshot assembled by the platform layer.
#[derive(Clone, Copy, Debug, Default)]
pub struct PlayInput {
    /// forward axis intent: +1 W, -1 S
    pub f: f32,
    /// strafe axis intent: +1 D, -1 A
    pub s: f32,
    /// jump pressed this frame (Space; also W in side-view)
    pub jump_pressed: bool,
    /// horizontal mouse delta for 3D look
    pub mouse_dx: f32,
}

impl Session {
    pub fn new(graph: &SceneGraph, cfg: GameplayConfig, asset_base: &str) -> Session {
        let solids = build_solids(graph, asset_base);
        let mut spawn = Vec3::new(0.0, 1.2, 4.0);
        let mut interactives = Vec::new();
        let mut enemy_base_y = HashMap::new();

        let mut ids: Vec<u32> = graph.nodes.keys().copied().collect();
        ids.sort_unstable();
        for id in ids {
            if id == graph.root_id {
                continue;
            }
            let Some(node) = graph.get(id) else { continue };
            let tags = &node.tags;
            let has = |t: &str| tags.iter().any(|x| x == t);
            if has("player") || has("start") {
                spawn = node.position;
                spawn.1 += 1.2;
            }
            let enemy = has("enemy");
            let hazard = has("hazard");
            let scoring = has("pickup") || has("score") || has("token") || has("dice")
                || has("objective");
            let goal = has("goal") || has("win");
            let checkpoint = has("checkpoint");
            let poi = has("poi");
            if !(enemy || hazard || scoring || goal || checkpoint || poi) {
                continue;
            }
            let lift = if enemy || hazard { 0.0 } else { 0.5 };
            let pos = Vec3::new(node.position.0, node.position.1 + lift, node.position.2);
            // combat tier from story-merge naming conventions
            // (Boss_*/Elite_*/Mook_*); drives speed + lunge behavior
            let tier = if !enemy {
                Tier::Mook
            } else {
                let nl = node.name.to_lowercase();
                if nl.starts_with("boss_") || nl.contains("boss") {
                    Tier::Boss
                } else if nl.starts_with("elite_") {
                    Tier::Elite
                } else if node.tags.iter().any(|t| {
                    t == "model:brute" || t == "model:knight"
                }) {
                    Tier::Elite
                } else {
                    Tier::Mook
                }
            };
            if enemy {
                enemy_base_y.insert(interactives.len(), node.position.1);
            }
            interactives.push(Interactive {
                name: node.name.clone(),
                pos,
                base_y: node.position.1,
                enemy,
                hazard,
                scoring,
                goal,
                checkpoint,
                poi,
                alive: true,
                seen_poi: false,
                tier,
                lunge_cd: 0.0,
            });
        }

        let genre_label = root_genre(graph);
        let mut s = Session {
            cfg,
            genre_label,
            spawn,
            pos: spawn,
            vel: Vec3::ZERO,
            grounded: false,
            coyote: 0.0,
            buffer: 0.0,
            cam_yaw: std::f32::consts::PI,
            score: 0,
            lives_left: None,
            game_over: false,
            won: false,
            dead_until: 0.0,
            now: 0.0,
            toast: (String::new(), 0.0),
            solids,
            interactives,
            enemy_base_y,
            anim_t: 0.0,
            scene_dirty: true,
            any_chasing: false,
            cam_pos: Vec3::ZERO,
            cam_yaw_out: std::f32::consts::PI,
            cam_pitch: 0.0,
            top_zoom: 34.0,
        };
        s.lives_left = s.cfg.lives;
        s.update_camera();
        s
    }

    pub fn toast_text(&self) -> &str {
        &self.toast.0
    }

    /// Rig animation clock - advances every simulated step.
    pub fn anim_speed(&self) -> f32 {
        if self.any_chasing { 1.0 } else { 0.35 }
    }

    /// Rendering inputs changed since the last assembled tracer scene.
    pub fn scene_dirty(&self) -> bool {
        self.scene_dirty
    }

    pub fn clear_scene_dirty(&mut self) {
        self.scene_dirty = false;
    }

    fn set_toast(&mut self, msg: &str) {
        self.toast = (msg.to_string(), self.now);
    }

    /// Advance the simulation by dt (already clamped <= 0.05 by the caller).
    pub fn step(&mut self, dt: f32, input: PlayInput) {
        self.now += dt;
        self.anim_t += dt;
        if self.game_over || self.now < self.dead_until {
            self.update_camera();
            return;
        }

        // planar velocity per camera mode (contract-identical mappings)
        let (dir_x, dir_z) = match self.cfg.mode {
            Mode::Side2D5 => (input.s, 0.0),
            Mode::TopDown => (input.s, -input.f),
            Mode::Orbit3D => {
                let (sy, cy) = self.cam_yaw.sin_cos();
                let fx = -sy;
                let fz = -cy;
                (fx * input.f - fz * input.s, fz * input.f + fx * input.s)
            }
        };
        let len = (dir_x * dir_x + dir_z * dir_z).sqrt().max(1.0);
        let moving = dir_x != 0.0 || dir_z != 0.0;
        self.vel.0 = if moving { dir_x / len * self.cfg.run_speed } else { 0.0 };
        self.vel.2 = if moving { dir_z / len * self.cfg.run_speed } else { 0.0 };

        // vertical: coyote time + jump buffering
        self.coyote = if self.grounded { self.cfg.coyote_s } else { (self.coyote - dt).max(0.0) };
        self.buffer = (self.buffer - dt).max(0.0);
        if input.jump_pressed {
            self.buffer = self.cfg.buffer_s;
        }
        if self.buffer > 0.0 && self.coyote > 0.0 {
            self.vel.1 = self.cfg.jump_velocity;
            self.coyote = 0.0;
            self.buffer = 0.0;
        }
        self.vel.1 -= self.cfg.gravity * dt;
        self.pos.0 += self.vel.0 * dt;
        self.pos.1 += self.vel.1 * dt;
        self.pos.2 += self.vel.2 * dt;

        // ground snap
        let gy = self.ground_at(self.pos.0, self.pos.2, self.pos.1);
        self.grounded = false;
        if gy > f32::NEG_INFINITY && self.pos.1 <= gy + 0.05 && self.vel.1 <= 0.0 {
            self.pos.1 = gy;
            self.vel.1 = 0.0;
            self.grounded = true;
        }
        self.collide_walls();
        self.collide_ceiling();
        if self.pos.1 < KILL_PLANE_Y {
            self.die("fell into the dark - back to checkpoint");
        }

        if !self.game_over {
            self.interactions(dt);
            if let Some(goal) = self.cfg.score_goal {
                if !self.won && self.score >= goal {
                    self.won = true;
                }
            }
        }

        if self.cfg.mode == Mode::Orbit3D {
            self.cam_yaw -= input.mouse_dx * 0.003;
        }
        self.update_camera();
    }

    fn ground_at(&self, x: f32, z: f32, y: f32) -> f32 {
        let mut best = f32::NEG_INFINITY;
        for b in &self.solids {
            if x >= b.min.0 - 0.3 && x <= b.max.0 + 0.3 && z >= b.min.2 - 0.3 && z <= b.max.2 + 0.3 {
                if b.max.1 <= y + 0.6 && b.max.1 > best {
                    best = b.max.1;
                }
            }
        }
        best
    }

    fn collide_walls(&mut self) {
        for b in &self.solids {
            if self.pos.1 >= b.max.1 - 0.15 || self.pos.1 + 1.7 <= b.min.1 {
                continue;
            }
            if self.pos.0 < b.min.0 - 0.45
                || self.pos.0 > b.max.0 + 0.45
                || self.pos.2 < b.min.2 - 0.45
                || self.pos.2 > b.max.2 + 0.45
            {
                continue;
            }
            let dxl = self.pos.0 - (b.min.0 - 0.45);
            let dxr = (b.max.0 + 0.45) - self.pos.0;
            let dzl = self.pos.2 - (b.min.2 - 0.45);
            let dzr = (b.max.2 + 0.45) - self.pos.2;
            let m = dxl.min(dxr).min(dzl).min(dzr);
            if (m - dxl).abs() < 1e-6 {
                self.pos.0 = b.min.0 - 0.45;
                self.vel.0 = self.vel.0.min(0.0);
            } else if (m - dxr).abs() < 1e-6 {
                self.pos.0 = b.max.0 + 0.45;
                self.vel.0 = self.vel.0.max(0.0);
            } else if (m - dzl).abs() < 1e-6 {
                self.pos.2 = b.min.2 - 0.45;
                self.vel.2 = self.vel.2.min(0.0);
            } else {
                self.pos.2 = b.max.2 + 0.45;
                self.vel.2 = self.vel.2.max(0.0);
            }
        }
    }

    fn collide_ceiling(&mut self) {
        if self.vel.1 <= 0.0 {
            return;
        }
        for b in &self.solids {
            if self.pos.0 < b.min.0 - 0.3
                || self.pos.0 > b.max.0 + 0.3
                || self.pos.2 < b.min.2 - 0.3
                || self.pos.2 > b.max.2 + 0.3
            {
                continue;
            }
            if self.pos.1 + 1.8 > b.min.1 && self.pos.1 < b.min.1 {
                self.pos.1 = b.min.1 - 1.85;
                self.vel.1 = 0.0;
            }
        }
    }

    fn interactions(&mut self, dt: f32) {
        // Work on a local copy so entity updates can consult world state
        // freely; death (if any) applies once, after the sweep.
        let mut its = std::mem::take(&mut self.interactives);
        let (death, events, moved, chasing) = self.sweep_interactions(&mut its, dt);
        self.interactives = its;
        for ev in events {
            match ev {
                Ev::Score(name) => {
                    let pts = self.cfg.coins_value;
                    self.score += pts;
                    match name {
                        Some(n) => self.set_toast(&format!("+{} {}", pts, n)),
                        None => self.set_toast(&format!("+{}", pts)),
                    }
                }
                Ev::Goal => self.won = true,
                Ev::Checkpoint(p) => {
                    self.spawn = Vec3::new(p.0, p.1 + 1.2, p.2);
                    self.set_toast("checkpoint lit");
                }
                Ev::Poi(name) => self.set_toast(&name),
            }
        }
        if let Some(msg) = death {
            self.die(msg);
        }
        self.any_chasing = chasing;
        if moved || !death.is_none() {
            self.scene_dirty = true;
        }
    }

    fn sweep_interactions(
        &self,
        its: &mut [Interactive],
        dt: f32,
    ) -> (Option<&'static str>, Vec<Ev>, bool, bool) {
        let mut events = Vec::new();
        let mut moved = false;
        let mut chasing_any = false;
        for (idx, it) in its.iter_mut().enumerate() {
            if !it.alive {
                continue;
            }
            if it.enemy {
                let dx = it.pos.0 - self.pos.0;
                let dz = it.pos.2 - self.pos.2;
                let hd = (dx * dx + dz * dz).sqrt().max(1e-4);
                let dy = it.pos.1 - self.pos.1;
                let d3 = (hd * hd + dy * dy).sqrt();
                if d3 < self.cfg.enemy_aggro_m && d3 > 0.1 {
                    chasing_any = true;
                    // tier pacing: bosses press harder, elites keep pressure
                    let (speed_mul, can_lunge) = match it.tier {
                        Tier::Boss => (1.35, true),
                        Tier::Elite => (1.15, false),
                        Tier::Mook => (1.0, false),
                    };
                    // boss lunge: short explosive dash on a cooldown
                    let mut lunge = 1.0;
                    if can_lunge {
                        it.lunge_cd -= dt;
                        if it.lunge_cd <= 0.0 && hd < 8.0 {
                            lunge = 2.4;
                            it.lunge_cd = 3.5;
                        }
                    }
                    let sp = ENEMY_SPEED * speed_mul * lunge;
                    // home on the plane only; keep cruise height
                    let nx = it.pos.0 - dx / hd * sp * dt;
                    let nz = it.pos.2 - dz / hd * sp * dt;
                    if !self.point_blocked(nx, it.pos.1, nz) {
                        if (nx - it.pos.0).abs() > 1e-5 || (nz - it.pos.2).abs() > 1e-5 {
                            moved = true;
                        }
                        it.pos.0 = nx;
                        it.pos.2 = nz;
                    }
                    if let Some(by) = self.enemy_base_y.get(&idx) {
                        it.pos.1 += (*by - it.pos.1) * (dt * 2.0).min(1.0);
                    }
                }
                if hd < self.cfg.kill_m + 0.4 && dy.abs() < 2.5 {
                    return (
                        Some(if self.cfg.corpse_run {
                            "you died - corpse run begins"
                        } else {
                            "caught - respawning"
                        }),
                        events,
                        moved,
                        chasing_any,
                    );
                }
            } else {
                let dx = it.pos.0 - self.pos.0;
                let dy = it.pos.1 - self.pos.1;
                let dz = it.pos.2 - self.pos.2;
                if dx * dx + dy * dy + dz * dz > self.cfg.interact_m * self.cfg.interact_m {
                    continue;
                }
                if it.hazard {
                    return (Some("hazard!"), events, moved, chasing_any);
                } else if it.scoring {
                    it.alive = false;
                    events.push(Ev::Score(if it.poi { Some(it.name.clone()) } else { None }));
                } else if it.goal {
                    events.push(Ev::Goal);
                } else if it.checkpoint {
                    it.alive = false;
                    events.push(Ev::Checkpoint(it.pos));
                } else if it.poi && !it.seen_poi {
                    it.seen_poi = true;
                    events.push(Ev::Poi(it.name.clone()));
                }
            }
        }
        (None, events, moved, chasing_any)
    }

    fn point_blocked(&self, x: f32, y: f32, z: f32) -> bool {
        for b in &self.solids {
            if x >= b.min.0 - 0.3
                && x <= b.max.0 + 0.3
                && z >= b.min.2 - 0.3
                && z <= b.max.2 + 0.3
                && y < b.max.1
                && y > b.min.1
            {
                return true;
            }
        }
        false
    }

    fn die(&mut self, msg: &str) {
        if self.game_over {
            return;
        }
        self.dead_until = self.now + DEAD_FREEZE_S;
        self.pos = self.spawn;
        self.vel = Vec3::ZERO;
        self.set_toast(msg);
        if let Some(l) = self.lives_left.as_mut() {
            *l = l.saturating_sub(1);
            if *l == 0 {
                self.game_over = true;
            }
        }
    }

    fn update_camera(&mut self) {
        match self.cfg.mode {
            Mode::TopDown => {
                self.cam_pos = Vec3::new(
                    self.pos.0,
                    self.top_zoom,
                    self.pos.2 + self.top_zoom * 0.35,
                );
                self.cam_yaw_out = std::f32::consts::PI;
                self.cam_pitch = -1.25;
            }
            Mode::Side2D5 => {
                self.cam_pos = Vec3::new(self.pos.0 + 2.0, self.pos.1 + 6.0, 16.0);
                self.cam_yaw_out = 0.0;
                self.cam_pitch = -0.28;
            }
            Mode::Orbit3D => {
                let (sy, cy) = self.cam_yaw.sin_cos();
                self.cam_pos = Vec3::new(
                    self.pos.0 + sy * 9.0,
                    self.pos.1 + 4.5,
                    self.pos.2 + cy * 9.0,
                );
                self.cam_yaw_out = self.cam_yaw;
                self.cam_pitch = -0.35;
            }
        }
    }

    /// Feed the engine camera: position + yaw/pitch so `CameraControls::
    /// to_camera` produces the session view without new renderer plumbing.
    pub fn apply_camera(&self, cam: &mut litt_pathtracer::CameraControls) {
        cam.position = self.cam_pos;
        cam.yaw = self.cam_yaw_out;
        cam.pitch = self.cam_pitch;
    }

    /// HUD lines (text, rgb 0..1) rendered by the platform layer.
    pub fn hud_lines(&self) -> Vec<(String, [f32; 3])> {
        let white = [0.85, 0.92, 1.0];
        let gold = [1.0, 0.84, 0.30];
        let red = [1.0, 0.55, 0.45];
        let mut out = vec![
            (
                format!(
                    "{} | {}",
                    self.genre_label.to_uppercase(),
                    self.cfg.objective
                ),
                white,
            ),
            (format!("SCORE {}", self.score), gold),
        ];
        if let Some(l) = self.lives_left {
            out.push((
                format!("LIVES {}", l),
                if l <= 1 { red } else { white },
            ));
        }
        if let Some(t) = self.recent_toast() {
            out.push((t.to_string(), gold));
        }
        out
    }

    fn recent_toast(&self) -> Option<String> {
        if self.now - self.toast.1 < 2.0 && !self.toast.0.is_empty() {
            return Some(self.toast.0.clone());
        }
        None
    }

    pub fn banner(&self) -> Option<&'static str> {
        if self.game_over {
            Some("GAME OVER - ESC TO QUIT")
        } else if self.won {
            Some("GOAL REACHED!")
        } else {
            None
        }
    }
}

fn root_genre(graph: &SceneGraph) -> String {
    graph
        .get(graph.root_id)
        .map(|n| n.name.clone())
        .unwrap_or_else(|| "litt".to_string())
}

/// Solid AABBs from every walkable-tagged node with a resolvable model.
fn build_solids(graph: &SceneGraph, asset_base: &str) -> Vec<Aabb> {
    const SOLID_TAGS: [&str; 7] =
        ["floor", "level", "board", "track", "hub", "terrain", "platform"];
    let mut cache: HashMap<String, Option<(Vec3, Vec3)>> = HashMap::new();
    let mut out = Vec::new();
    let mut ids: Vec<u32> = graph.nodes.keys().copied().collect();
    ids.sort_unstable();
    for id in ids {
        if id == graph.root_id {
            continue;
        }
        let Some(node) = graph.get(id) else { continue };
        let is_solid = node.tags.iter().any(|t| SOLID_TAGS.contains(&t.as_str()));
        if !is_solid {
            continue;
        }
        let model = node
            .tags
            .iter()
            .find(|t| t.starts_with("model:"))
            .map(|t| t["model:".len()..].to_string());
        let Some(name) = model else { continue };
        let bounds = cache.entry(name.clone()).or_insert_with(|| {
            let path = format!("{}/models/{}.obj", asset_base.trim_end_matches('/'), name);
            ObjLoader::load_from_file(&path).ok().map(|m| mesh_bounds(&m))
        });
        let Some((bmin, bmax)) = bounds.as_ref() else { continue };
        let s = node.scale.0.max(0.001);
        out.push(Aabb {
            min: Vec3::new(
                node.position.0 + bmin.0 * s,
                node.position.1 + bmin.1 * s,
                node.position.2 + bmin.2 * s,
            ),
            max: Vec3::new(
                node.position.0 + bmax.0 * s,
                node.position.1 + bmax.1 * s,
                node.position.2 + bmax.2 * s,
            ),
        });
    }
    out
}

fn mesh_bounds(model: &litt_asset::Model) -> (Vec3, Vec3) {
    let mut min = Vec3::new(f32::MAX, f32::MAX, f32::MAX);
    let mut max = Vec3::new(f32::MIN, f32::MIN, f32::MIN);
    for mesh in &model.meshes {
        for v in &mesh.vertices {
            let p = v.position;
            min.0 = min.0.min(p.0);
            min.1 = min.1.min(p.1);
            min.2 = min.2.min(p.2);
            max.0 = max.0.max(p.0);
            max.1 = max.1.max(p.1);
            max.2 = max.2.max(p.2);
        }
    }
    (min, max)
}

// --------------------------------------------------------------------- tests

#[cfg(test)]
mod tests {
    use super::*;

    const STATE_JSON: &str = r#"{
      "theme": "underground_caves",
      "identity": {"archetype": "dungeon_crawler", "camera": "side_scrolling",
                   "movement": "parkour_movement"},
      "gameplay": {
        "genre": "dungeon_crawler",
        "objective": "test objective",
        "enemy_aggro_m": 8.0,
        "corpse_run": true,
        "spawn": [0.0, 0.0, 4.5],
        "lives": 3,
        "score_goal": 600,
        "physics": {"gravity": 30, "jump_velocity": 12, "run_speed": 8,
                    "coyote_time_s": 0.12}
      }
    }"#;

    #[test]
    fn json_scanner_handles_generator_output() {
        let v = Json::parse(STATE_JSON).unwrap();
        assert_eq!(
            v.get("identity").unwrap().get("camera").unwrap().as_str(),
            Some("side_scrolling")
        );
        let gp = v.get("gameplay").unwrap();
        assert_eq!(gp.get("physics").unwrap().get("gravity").unwrap().as_f64(), Some(30.0));
        assert_eq!(gp.get("lives").unwrap().as_f64(), Some(3.0));
    }

    #[test]
    fn config_parses_all_contract_fields() {
        let cfg = GameplayConfig::from_state_json(STATE_JSON);
        assert_eq!(cfg.mode, Mode::Side2D5);
        assert_eq!(cfg.gravity, 30.0);
        assert_eq!(cfg.jump_velocity, 12.0);
        assert_eq!(cfg.lives, Some(3));
        assert_eq!(cfg.score_goal, Some(600));
        assert!(cfg.corpse_run);
        assert_eq!(cfg.objective, "test objective");
        // jump_buffer_s absent -> coyote + 0.02 fallback
        assert!((cfg.buffer_s - 0.14).abs() < 1e-4);
    }

    #[test]
    fn mode_resolution_matches_runtime_contract() {
        assert_eq!(resolve_mode("walk", "orbit"), Mode::Orbit3D);
        assert_eq!(resolve_mode("platformer_movement", "orbit"), Mode::Side2D5);
        assert_eq!(resolve_mode("walk", "top_down"), Mode::TopDown);
        assert_eq!(resolve_mode("walk", "isometric"), Mode::TopDown);
        assert_eq!(resolve_mode("walk", "side_scrolling"), Mode::Side2D5);
    }

    #[test]
    fn defaults_match_reference_runtime() {
        let cfg = GameplayConfig::from_state_json("{}");
        assert_eq!(cfg.gravity, 22.0);
        assert_eq!(cfg.jump_velocity, 8.0);
        assert_eq!(cfg.run_speed, 7.0);
        assert!((cfg.coyote_s - 0.1).abs() < 1e-5);
        assert_eq!(cfg.enemy_aggro_m, 6.0);
    }

    fn empty_session(mode: Mode) -> Session {
        let g = SceneGraph::new();
        Session::new(&g, GameplayConfig { mode, ..Default::default() }, ".")
    }

    fn run_seconds(s: &mut Session, secs: f32, input: PlayInput) {
        let steps = (secs / 0.016) as usize;
        for _ in 0..steps {
            s.step(0.016, input);
        }
    }

    #[test]
    fn top_down_w_pushes_away_negative_z() {
        let mut s = empty_session(Mode::TopDown);
        let z0 = s.pos.2;
        run_seconds(&mut s, 0.5, PlayInput { f: 1.0, ..Default::default() });
        assert!(s.pos.2 < z0 - 0.5, "W must move -z, moved to {}", s.pos.2);
    }

    #[test]
    fn side_view_d_strafes_positive_x_and_w_jumps() {
        let mut s = empty_session(Mode::Side2D5);
        let x0 = s.pos.0;
        run_seconds(&mut s, 0.4, PlayInput { s: 1.0, ..Default::default() });
        assert!(s.pos.0 > x0 + 0.5, "D must strafe +x");
        // jumping needs ground contact first (as in every real world)
        let mut s2 = empty_session(Mode::Side2D5);
        let y0 = s2.pos.1;
        s2.grounded = true;
        s2.step(0.016, PlayInput { jump_pressed: true, ..Default::default() });
        assert!(s2.vel.1 > 0.0 && s2.pos.1 > y0, "W must jump in 2D5");
        assert!(!s2.grounded, "jumping leaves the ground");
    }

    #[test]
    fn gravity_pulls_down_then_lands() {
        let mut s = empty_session(Mode::Orbit3D);
        let y0 = s.pos.1;
        s.step(0.05, PlayInput::default());
        assert!(s.pos.1 < y0, "gravity must pull down");
    }

    #[test]
    fn lives_decrement_to_game_over() {
        let mut s = empty_session(Mode::Orbit3D);
        s.lives_left = Some(2);
        s.die("hit");
        assert_eq!(s.lives_left, Some(1));
        assert!(!s.game_over);
        s.die("hit");
        assert_eq!(s.lives_left, Some(0));
        assert!(s.game_over);
        assert_eq!(s.banner(), Some("GAME OVER - ESC TO QUIT"));
    }

    #[test]
    fn score_goal_triggers_win() {
        let mut s = empty_session(Mode::Orbit3D);
        s.cfg.score_goal = Some(50);
        s.score = 50;
        s.step(0.016, PlayInput::default());
        assert!(s.won);
    }

    #[test]
    fn kill_plane_respawns_at_spawn() {
        let mut s = empty_session(Mode::Orbit3D);
        s.pos.1 = -20.0;
        s.step(0.016, PlayInput::default());
        assert_eq!(s.pos.0, s.spawn.0);
        assert_eq!(s.pos.1, s.spawn.1);
    }

    #[test]
    fn enemy_homes_on_plane_not_into_ground() {
        let mut s = empty_session(Mode::Orbit3D);
        s.interactives.push(Interactive {
            name: "Drone".into(),
            pos: Vec3::new(3.0, 1.2, 1.5),
            base_y: 1.2,
            enemy: true,
            hazard: false,
            scoring: false,
            goal: false,
            checkpoint: false,
            poi: false,
            alive: true,
            seen_poi: false,
            tier: Tier::Mook,
            lunge_cd: 0.0,
        });
        s.enemy_base_y.insert(0, 1.2);
        let y_before = s.interactives[0].pos.1;
        run_seconds(&mut s, 1.0, PlayInput::default());
        let e = &s.interactives[0];
        assert!(e.pos.0 < 3.0 || e.pos.2 < 1.5, "enemy must close distance");
        assert!(
            (e.pos.1 - 1.2).abs() < 0.05 && e.pos.1 >= y_before - 0.05,
            "enemy must hold cruise height, got {}",
            e.pos.1
        );
    }

    #[test]
    fn boss_tier_presses_harder_than_mook() {
        let mk = |tier: Tier| {
            let mut s = empty_session(Mode::Orbit3D);
            s.interactives.push(Interactive {
                name: if tier == Tier::Boss { "Boss_X".into() } else { "Drone".into() },
                pos: Vec3::new(4.0, 1.2, 0.0),
                base_y: 1.2,
                enemy: true,
                hazard: false,
                scoring: false,
                goal: false,
                checkpoint: false,
                poi: false,
                alive: true,
                seen_poi: false,
                tier,
                lunge_cd: 0.0,
            });
            s.enemy_base_y.insert(0, 1.2);
            s
        };
        let mut mook = mk(Tier::Mook);
        let mut boss = mk(Tier::Boss);
        run_seconds(&mut mook, 1.2, PlayInput::default());
        run_seconds(&mut boss, 1.2, PlayInput::default());
        let dm = (mook.interactives[0].pos.0.powi(2)
            + mook.interactives[0].pos.2.powi(2))
            .sqrt();
        let db = (boss.interactives[0].pos.0.powi(2)
            + boss.interactives[0].pos.2.powi(2))
            .sqrt();
        assert!(db < dm, "boss must close distance faster ({} vs {})", db, dm);
    }

    #[test]
    fn checkpoint_moves_spawn() {
        let mut s = empty_session(Mode::Orbit3D);
        s.interactives.push(Interactive {
            name: "CP".into(),
            pos: Vec3::new(2.0, 1.0, 2.0),
            base_y: 1.0,
            enemy: false,
            hazard: false,
            scoring: false,
            goal: false,
            checkpoint: true,
            poi: false,
            alive: true,
            seen_poi: false,
            tier: Tier::Mook,
            lunge_cd: 0.0,
        });
        s.pos = Vec3::new(2.0, 1.0, 2.0);
        s.step(0.016, PlayInput::default());
        assert!(!s.interactives[0].alive);
        assert!((s.spawn.0 - 2.0).abs() < 1e-4 && (s.spawn.1 - 2.2).abs() < 1e-4);
        assert!(s.toast_text().contains("checkpoint"));
    }
}
