//! Player controller component
//! Simple WASD + mouse look
use litt_math::*;

#[derive(Clone, Debug)]
pub struct Player {
    pub position: Vec3,
    pub rotation: Vec2,
    pub velocity: Vec3,
    pub speed: f32,
    pub look_speed: f32,
    pub is_ground: bool,
}

impl Player {
    pub fn new() -> Self {
        Self {
            position: Vec3::new(0.0, 1.0, 0.0),
            rotation: Vec2::new(0.0, 0.0),
            velocity: Vec3::ZERO,
            speed: 5.0,
            look_speed: 0.002,
            is_ground: false,
        }
    }
    pub fn update(&mut self, dt: f32, keys: &[u8]) {
        let mut dir = Vec3::ZERO;
        if keys.contains(&b'w') || keys.contains(&b'W') { dir.2 -= 1.0; }
        if keys.contains(&b's') || keys.contains(&b'S') { dir.2 += 1.0; }
        if keys.contains(&b'a') || keys.contains(&b'A') { dir.0 -= 1.0; }
        if keys.contains(&b'd') || keys.contains(&b'D') { dir.0 += 1.0; }
        dir = dir.normalized() * self.speed;
        self.position = self.position + dir * dt;
    }
    pub fn update_look(&mut self, dx: f32, dy: f32) {
        self.rotation.0 += dx * self.look_speed;
        self.rotation.1 += dy * self.look_speed;
        self.rotation.1 = self.rotation.1.clamp(-1.5, 1.5);
    }
}

impl Default for Player { fn default() -> Self { Self::new() } }
